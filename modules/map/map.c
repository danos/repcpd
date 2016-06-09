/**
 * @file map.c  PCP module for MAP opcode handling
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>


static struct mapping_table *table;


static bool request_handler(struct udp_sock *us, const struct sa *src,
			    struct mbuf *mb, struct pcp_msg *msg)
{
	enum pcp_result result = PCP_NO_RESOURCES;
	struct mapping *mapping = NULL;
	struct pcp_map *map = &msg->pld.map;
	int err;

	if (msg->hdr.opcode != PCP_MAP) return false;

	sa_set_port(&msg->hdr.cli_addr, map->int_port);

	if (msg->hdr.lifetime) {
		if (!map->int_port) {
			warning("map: wildcard/dmz not supported\n");
			result = PCP_UNSUPP_PROTOCOL;
			goto error;
		}
		else if (!map->proto) {
			result = PCP_MALFORMED_REQUEST;
			goto error;
		}

		msg->hdr.lifetime = pcp_lifetime_calculate(msg->hdr.lifetime);
	}

	mapping = mapping_find(table, map->proto, &msg->hdr.cli_addr);
	if (mapping) {
		/* Simple Threat Model, verify nonce */
		if (!pcp_nonce_cmp(msg, mapping->map.nonce)) {
			warning("map: request NONCE does not match"
				" existing mapping\n");
			result = PCP_NOT_AUTHORIZED;
			goto error;
		}

		map->ext_addr = mapping->map.ext_addr;
		mapping_refresh(mapping, msg->hdr.lifetime);
	}
	else {
		if (!repcpd_extaddr_exist(&map->ext_addr)) {

			if (pcp_msg_option(msg, PCP_OPTION_PREFER_FAILURE)) {

				result = PCP_CANNOT_PROVIDE_EXTERNAL;
				goto error;
			}

			err = repcpd_extaddr_assign(&map->ext_addr,
						    map->int_port,
						    sa_af(src));
			if (err) {
				warning("map: failed to assign"
					" a valid extaddr\n");

				result = PCP_CANNOT_PROVIDE_EXTERNAL;
				goto error;
			}
		}
	}

	if (msg->hdr.lifetime) {

		if (!mapping) {
			struct pcp_option *opt;

			opt = pcp_msg_option(msg, PCP_OPTION_DESCRIPTION);

			err = mapping_create(&mapping, table, PCP_MAP,
					     map->proto,
					     &msg->hdr.cli_addr,
					     &map->ext_addr, NULL,
					     msg->hdr.lifetime, map->nonce,
					     opt ? opt->u.description : NULL);
			if (err) {
				warning("mapping_create: %m\n", err);
				goto error;
			}
		}
	}
	else {
		mapping = mem_deref(mapping);
	}

	/* reply SUCCESS */
	err = pcp_reply(us, src, mb, msg->hdr.opcode, PCP_SUCCESS,
			msg->hdr.lifetime, repcpd_epoch_time(),
			mapping ? &mapping->map : map);
	if (err) {
		warning("map: pcp_reply_map() failed (%m)\n", err);
		goto error;
	}

	return true;

 error:
	warning("map: replying error %s\n", pcp_result_name(result));
	pcp_ereply(us, src, mb, result);

	return true;
}


static struct repcpd_pcp map = {
	.reqh = request_handler,
};


static int module_init(void)
{
	int err;

	err = mapping_table_alloc(&table, "REPCPD-MAP");
	if (err)
		return err;

	repcpd_register_handler(&map);

	debug("map: module loaded\n");

	return 0;
}


static int module_close(void)
{
	debug("map: module closing..\n");

	repcpd_unregister_handler(&map);
	table = mem_deref(table);

	return 0;
}


const struct mod_export exports = {
	.name  = "map",
	.type  = "pcp",
	.init  = module_init,
	.close = module_close,
};
