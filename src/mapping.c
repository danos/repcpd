/**
 * @file mapping.c  PCP mapping table
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <string.h>
#include <re.h>
#include <repcpd.h>


struct mapping_table {
	struct backend *be;
	struct hash *ht;
	char *name;
	bool exiting;
};


static void mapping_destructor(void *arg)
{
	struct mapping *mapping = arg;

	list_unlink(&mapping->le);
	tmr_cancel(&mapping->tmr);

	if (mapping->committed && !mapping->table->exiting) {

		switch (mapping->opcode) {

		case PCP_MAP:
			mapping->table->be->delete(mapping->table->name,
						   mapping->map.proto,
						   &mapping->map.ext_addr,
						   mapping->ext_ifname,
						   &mapping->int_addr,
						   mapping->descr);
			break;

		case PCP_PEER:
			mapping->table->be->delete_snat(mapping->table->name,
							mapping->map.proto,
							&mapping->map.ext_addr,
						        mapping->ext_ifname,
							&mapping->int_addr,
							&mapping->remote_addr,
							mapping->descr);
			break;

		default:
			break;
		};

		info("mapping: deleted: proto=%s int=%J <----> ext=%J\n",
		     pcp_proto_name(mapping->map.proto),
		     &mapping->int_addr, &mapping->map.ext_addr);
	}

	mem_deref(mapping->descr);
	mem_deref(mapping->ext_ifname);
}


static void timeout(void *arg)
{
	struct mapping *mapping = arg;

	info("map: mapping expired (port %u -- external %J)\n",
	     mapping->map.int_port, &mapping->map.ext_addr);

	mem_deref(mapping);
}


static uint32_t key(int proto, const struct sa *int_addr,
		    const struct sa *rem_addr)
{
	uint32_t v;

	v = proto + sa_hash(int_addr, SA_ALL);

	if (sa_isset(rem_addr, SA_ALL))
		v += sa_hash(rem_addr, SA_ALL);

	return v;
}


/* note: "remote_addr" is optional (only for PEER) */
int mapping_create(struct mapping **mappingp, struct mapping_table *table,
		   enum pcp_opcode opcode,
		   int proto,
		   const struct sa *int_addr, const char *ext_ifname,
		   const struct sa *ext_addr, const struct sa *remote_addr,
		   uint32_t lifetime, const uint8_t nonce[12],
		   const char *descr)
{
	struct mapping *mapping;
	int err;

	if (!mappingp || !table || !int_addr || !ext_addr || !nonce)
		return EINVAL;

	mapping = mem_zalloc(sizeof(*mapping), mapping_destructor);
	if (!mapping)
		return ENOMEM;

	mapping->table = table;
	mapping->opcode = opcode;
	memcpy(mapping->map.nonce, nonce, 12);
	mapping->map.proto = proto;
	mapping->map.int_port = sa_port(int_addr);
	mapping->map.ext_addr = *ext_addr;
	if (remote_addr)
		mapping->remote_addr = *remote_addr;

	mapping->int_addr = *int_addr;
	mapping->lifetime = lifetime;

	err = str_dup(&mapping->ext_ifname, ext_ifname);
	if (err)
		goto out;

	if (descr) {
		err = str_dup(&mapping->descr, descr);
		if (err)
			goto out;
	}

	switch (opcode) {

	case PCP_MAP:
		err = table->be->append(table->name, proto, ext_addr,
					ext_ifname, int_addr,
					descr);
		break;

	case PCP_PEER:
		err = table->be->append_snat(table->name, proto,
					     ext_addr, ext_ifname, int_addr,
					     remote_addr, descr);
		break;

	default:
		break;
	}
	if (err) {
		warning("map: append rule failed (%m)\n", err);
		goto out;
	}

	tmr_start(&mapping->tmr, lifetime * 1000, timeout, mapping);

	hash_append(table->ht, key(proto, int_addr, remote_addr),
		    &mapping->le, mapping);

	mapping->committed = true;

	info("map: created mapping: proto=%s int=%J <---> ext=%J (%usec)\n",
	     pcp_proto_name(mapping->map.proto),
	     &mapping->int_addr, &mapping->map.ext_addr,
	     lifetime);

 out:
	if (err)
		mem_deref(mapping);
	else if (mappingp)
		*mappingp = mapping;

	return err;
}


void mapping_refresh(struct mapping *mapping, uint32_t lifetime)
{
	if (!mapping)
		return;

	tmr_start(&mapping->tmr, lifetime * 1000, timeout, mapping);
}


struct tuple {
	int proto;
	const struct sa *int_addr;
	const struct sa *remote_addr;  /* PEER only */
};


static bool hash_cmp_handler(struct le *le, void *arg)
{
	const struct mapping *map = le->data;
	const struct tuple *tup = arg;

	if (!sa_cmp(&map->int_addr, tup->int_addr, SA_ALL))
		return false;

	if (map->map.proto != tup->proto)
		return false;

	if (sa_isset(tup->remote_addr, SA_ALL) &&
	    !sa_cmp(&map->remote_addr, tup->remote_addr, SA_ALL))
		return false;

	return true;
}


struct mapping *mapping_find(const struct mapping_table *table,
			     int proto, const struct sa *int_addr)
{
	struct tuple tup;

	tup.proto       = proto;
	tup.int_addr    = int_addr;
	tup.remote_addr = NULL;

	return list_ledata(hash_lookup(table->ht, key(proto, int_addr, NULL),
				       hash_cmp_handler, &tup));
}


struct mapping *mapping_find_peer(const struct mapping_table *table,
				  int proto, const struct sa *int_addr,
				  const struct sa *remote_addr)
{
	struct tuple tup;

	tup.proto       = proto;
	tup.int_addr    = int_addr;
	tup.remote_addr = remote_addr;

	return list_ledata(hash_lookup(table->ht,
				       key(proto, int_addr, remote_addr),
				       hash_cmp_handler, &tup));
}


static void table_destructor(void *arg)
{
	struct mapping_table *table = arg;
	struct backend *be;

	be = backend_get();

	debug("mapping: table `%s' destroyed\n", table->name);

	table->exiting = true;

	hash_flush(table->ht);
	mem_deref(table->ht);

	if (be)
		be->flush(table->name);

	mem_deref(table->name);
}


int mapping_table_alloc(struct mapping_table **tablep, const char *name)
{
	struct mapping_table *table;
	int err;

	table = mem_zalloc(sizeof(*table), table_destructor);
	if (!table)
		return ENOMEM;

	err = str_dup(&table->name, name);
	if (err)
		goto out;

	err = hash_alloc(&table->ht, 64);
	if (err)
		goto out;

	table->be = backend_get();
	if (!table->be) {
		warning("mapping: could not find a suitable backend\n");
		err = ENOENT;
		goto out;
	}

	/* Flush and delete the backend table first */
	table->be->flush(name);

	err = table->be->new(name);
	if (err) {
		error("mapping: failed to create chain '%s' (%m)\n",
		      name, err);
		goto out;
	}

	info("mapping: created table `%s'\n", name);

 out:
	if (err)
		mem_deref(table);
	else
		*tablep = table;

	return err;
}
