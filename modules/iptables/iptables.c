/**
 * @file iptables.c  PCP netfilter manipulation using iptables
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <repcpd.h>


/*
 * NOTE: we are using iptables/netfilter to create and destroy mappings,
 *       currently using the system() api. it would probably be better
 *       to use libnetfilter instead. we should use that API to lookup
 *       existing mappings...
 */


static int print_comment(struct re_printf *pf, const char *comment)
{
	if (!comment)
		return 0;

	return re_hprintf(pf, " -m comment --comment \"%s\"", comment);
}


static int iptables_cmd(const char *fmt, ...)
{
	char cmd[512];
	va_list ap;

	va_start(ap, fmt);
	re_vsnprintf(cmd, sizeof(cmd), fmt, ap);
	va_end(ap);

	debug("iptables: command [%s]\n", cmd);

	return system(cmd);
}


static int backend_new(const char *name)
{
	int err;

	err = iptables_cmd("iptables -t nat -N %s", name);
	if (err)
		return err;

#if 0
	/* todo: needed for openwrt ? */
	/* jump from the INPUT chain to our new custom chain */
	return iptables_cmd("iptables -t nat -A PREROUTING -j %s", chain);
#endif

	return 0;
}


static void backend_flush(const char *name)
{
	info("flush\n");
	iptables_cmd("iptables -t nat -F %s 2>/dev/null", name);
	iptables_cmd("iptables -t nat -X %s 2>/dev/null", name);
}


static int backend_append(const char *name, int proto,
			  const struct sa *ext_addr, const char *ext_ifname,
			  const struct sa *int_addr,
			  const char *descr)
{

	return iptables_cmd("iptables -t nat -A %s -p %s -i %s --dport %u"
			    " -j DNAT --to %J"
			    "%H",
			    name, pcp_proto_name(proto), ext_ifname,
			    sa_port(ext_addr), int_addr,
			    print_comment, descr);
}


static void backend_delete(const char *name, int proto,
			   const struct sa *ext_addr, const char *ext_ifname,
			   const struct sa *int_addr,
			   const char *descr)
{
	iptables_cmd("iptables -t nat -D %s -p %s -i %s --dport %u"
		     " -j DNAT --to %J"
		     "%H",
		     name, pcp_proto_name(proto), ext_ifname,
		     sa_port(ext_addr), int_addr,
		     print_comment, descr);
}


static int backend_append_snat(const char *name, int proto,
			       const struct sa *ext_addr,
			       const char *ext_ifname,
			       const struct sa *int_addr,
			       const struct sa *remote_addr,
			       const char *descr)
{
	/* --to is what it should be re-written _TO_ */

	return iptables_cmd("iptables -t nat -A %s -p %s -i %s"
			    " --source %j --sport %u"
			    " --dst %j --dport %u"
			    " -j SNAT --to %J"
			    "%H",
			    name, pcp_proto_name(proto), ext_ifname,
			    int_addr, sa_port(int_addr),
			    remote_addr, sa_port(remote_addr),
			    ext_addr,
			    print_comment, descr);
}


static void backend_delete_snat(const char *name, int proto,
				const struct sa *ext_addr,
				const char *ext_ifname,
				const struct sa *int_addr,
				const struct sa *peer_addr,
				const char *descr)
{
	iptables_cmd("iptables -t nat -D %s -p %s -i %s"
		     " --source %j --sport %u"
		     " --dst %j --dport %u"
		     " -j SNAT --to %J"
		     "%H",
		     name, pcp_proto_name(proto), ext_ifname,
		     int_addr, sa_port(int_addr),
		     peer_addr, sa_port(peer_addr),
		     ext_addr,
		     print_comment, descr);
}


static struct backend be = {
	.new    = backend_new,
	.flush  = backend_flush,
	.append = backend_append,
	.delete = backend_delete,
	.append_snat = backend_append_snat,
	.delete_snat = backend_delete_snat,
};


static int module_init(void)
{
	backend_register(&be);

	debug("iptables: module loaded\n");

	return 0;
}


static int module_close(void)
{
	backend_unregister(&be);

	debug("iptables: module closed\n");

	return 0;
}


const struct mod_export exports = {
	.name  = "iptables",
	.type  = "pcp",
	.init  = module_init,
	.close = module_close,
};
