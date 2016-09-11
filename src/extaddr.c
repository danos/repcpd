/**
 * @file extaddr.c  PCP external address handling
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>
#include "pcpd.h"


struct extaddr {
	struct le le;
	struct sa addr;
	char *ifname;
};


static struct list extaddrl;


static void destructor(void *arg)
{
	struct extaddr *ea = arg;

	mem_deref(ea->ifname);

	list_unlink(&ea->le);
}


static int extaddr_add(const char *ifname, const struct sa *addr)
{
	struct extaddr *ea;
	int err;

	ea = mem_zalloc(sizeof(*ea), destructor);
	if (!ea)
		return ENOMEM;

	ea->addr = *addr;

	err = str_dup(&ea->ifname, ifname);
	if (err)
		goto out;

	list_append(&extaddrl, &ea->le, ea);

	debug("added external interface: %s with IP-address %j\n",
	      ifname, &ea->addr);

 out:
	if (err)
		mem_deref(ea);

	return err;
}


static int listen_handler(const struct pl *interface, void *arg)
{
	char ifname[64];
	struct sa addr;
	bool valid = false;
	int err = 0;
	(void)arg;

	pl_strcpy(interface, ifname, sizeof(ifname));

	if (0 == net_if_getaddr(ifname, AF_INET, &addr)) {
		err |= extaddr_add(ifname, &addr);
		valid = true;
	}

	if (0 == net_if_getaddr(ifname, AF_INET6, &addr)) {
		err |= extaddr_add(ifname, &addr);
		valid = true;
	}

	if (!valid) {
		warning("no valid address for interface `%r'\n",
			interface);
	}

	return err;
}


int repcpd_extaddr_init(void)
{
	int err;

	list_init(&extaddrl);

	err = conf_apply(_conf(), "external_interface", listen_handler, 0);
	if (err)
		goto out;

 out:
	if (err)
		repcpd_extaddr_close();

	return err;
}


void repcpd_extaddr_close(void)
{
	list_flush(&extaddrl);
}


bool repcpd_extaddr_exist(const struct sa *addr)
{
	struct le *le;

	for (le = extaddrl.head; le; le = le->next) {

		struct extaddr *ea = le->data;

		if (sa_cmp(&ea->addr, addr, SA_ADDR))
			return true;
	}

	return false;
}


/* note: af is suggested AF -- AF_UNSPEC means any */
int repcpd_extaddr_assign(struct sa *ext_addr, uint16_t int_port, int af)
{
	struct sa *ea;

	if (!ext_addr || !int_port)
		return EINVAL;

	ea = repcpd_extaddr_find(af);
	if (!ea)
		return EAFNOSUPPORT;

	*ext_addr = *ea;
	sa_set_port(ext_addr, int_port);

	return 0;
}


char *repcpd_extaddr_ifname_find(int af)
{
	struct le *le;

	for (le = extaddrl.head; le; le = le->next) {

		struct extaddr *ea = le->data;

		if (af != AF_UNSPEC && af != sa_af(&ea->addr))
			continue;

		return ea->ifname;
	}

	return NULL;
}


struct sa *repcpd_extaddr_find(int af)
{
	struct le *le;

	for (le = extaddrl.head; le; le = le->next) {

		struct extaddr *ea = le->data;

		if (af != AF_UNSPEC && af != sa_af(&ea->addr))
			continue;

		return &ea->addr;
	}

	return NULL;
}
