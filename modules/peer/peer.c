/**
 * @file peer.c  PCP module for PEER opcode handling
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <string.h>
#include <re.h>
#include <repcpd.h>


static struct mapping_table *table;


static bool request_handler(struct udp_sock *us, const struct sa *src,
			    struct mbuf *mb, struct pcp_msg *msg)
{
	struct pcp_peer peer;
	enum pcp_result result = PCP_NO_RESOURCES;
	struct mapping *mapping = NULL;
	struct sa int_addr;
	uint32_t lifetime;
	int err;

	if (msg->hdr.opcode != PCP_PEER)
		return false;

	info("peer: Request from %J\n", src);

	peer = *(struct pcp_peer *)pcp_msg_payload(msg);
	lifetime = msg->hdr.lifetime;
	int_addr = msg->hdr.cli_addr;
	sa_set_port(&int_addr, peer.map.int_port);

	/* Validate Remote Peer Address and Port */
	if (!peer.map.proto ||
	    sa_is_loopback(&peer.remote_addr) ||
	    sa_is_any(&peer.remote_addr)) {

		result = PCP_MALFORMED_REQUEST;
		goto error;
	}

	mapping = mapping_find_peer(table, peer.map.proto, &int_addr,
				    &peer.remote_addr);

	if (mapping) {
		info("peer: found mapping\n");
	}

	if (!mapping) {

		if (!sa_isset(&peer.map.ext_addr, SA_ALL) ||
		    repcpd_extaddr_exist(&peer.map.ext_addr)) {

			if (!sa_isset(&peer.map.ext_addr, SA_ALL)) {

				err = repcpd_extaddr_assign(&peer.map.ext_addr,
							    sa_port(&int_addr),
							    sa_af(src));
				if (err) {
					warning("peer: could not assign"
						       " external address\n");
					result = PCP_CANNOT_PROVIDE_EXTERNAL;
					goto error;
				}
			}

			err = mapping_create(&mapping, table, PCP_PEER,
					     peer.map.proto, &int_addr,
					     &peer.map.ext_addr,
					     &peer.remote_addr,
					     lifetime, peer.map.nonce, NULL);
			if (err) {
				warning("peer: failed to create mapping"
					       " (%m)\n", err);
				goto error;
			}
			else
				goto out;
		}
		else {
			result = PCP_CANNOT_PROVIDE_EXTERNAL;
			goto error;
		}
	}

	/* todo: if a matching mapping is found, and no previous
	 *       PEER opcode was..
	 */

	/* simple threat model */
	if (mapping) {
		if (0 != memcmp(peer.map.nonce, mapping->map.nonce,
				PCP_NONCE_SZ)) {
			result = PCP_NOT_AUTHORIZED;
			goto error;
		}
	}

 out:
	if (mapping) {
		info("peer: SUCCESS -- Suggested=%J, Assigned=%J\n",
		     &msg->pld.peer.map.ext_addr, &peer.map.ext_addr);
	}

	if (mapping) {
		peer = *(struct pcp_peer *)&mapping->map;
		peer.remote_addr = mapping->remote_addr;
	}

	/* reply SUCCESS */
	err = pcp_reply(us, src, mb, msg->hdr.opcode, PCP_SUCCESS,
			lifetime, repcpd_epoch_time(),
			&peer);
	if (err) {
		warning("peer: pcp_reply() failed (%m)\n", err);
		goto error;
	}

	return true;

 error:
	warning("peer: replying error %s\n", pcp_result_name(result));
	pcp_ereply(us, src, mb, result);

	return true;
}


static struct repcpd_pcp peer = {
	.reqh = request_handler,
};


static int module_init(void)
{
	int err;

	err = mapping_table_alloc(&table, "REPCPD-PEER");
	if (err)
		return err;

	repcpd_register_handler(&peer);

	debug("peer: module loaded\n");

	return 0;
}


static int module_close(void)
{
	repcpd_unregister_handler(&peer);
	table = mem_deref(table);

	debug("peer: module closed\n");

	return 0;
}


const struct mod_export exports = {
	.name  = "peer",
	.type  = "pcp",
	.init  = module_init,
	.close = module_close,
};
