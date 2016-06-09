/**
 * @file announce.c  PCP Announce module
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>


static bool request_handler(struct udp_sock *us, const struct sa *src,
			    struct mbuf *mb, struct pcp_msg *msg)
{
	int err;

	if (msg->hdr.opcode != PCP_ANNOUNCE)
		return false;

	if (msg->hdr.lifetime != 0) {
		info("announce: ANNOUNCE request has"
		     " non-zero lifetime\n");

		pcp_ereply(us, src, mb, PCP_MALFORMED_REQUEST);
		return true;
	}

	info("announce: received PCP ANNOUNCE from %J (%u bytes)\n",
	     src, mb->end);

	err = pcp_reply(us, src, mb, msg->hdr.opcode, PCP_SUCCESS,
			0, repcpd_epoch_time(), NULL);
	if (err) {
		warning("announce: failed to reply: %m\n", err);
	}

	return true;
}


static struct repcpd_pcp announce = {
	.reqh = request_handler,
};


static void lstnr_handler(struct sa *bnd_addr, struct udp_sock *us,
			  void *arg)
{
	const char *maddr;
	struct sa dst;
	int err;

	(void)arg;

	switch (sa_af(bnd_addr)) {

	case AF_INET:
		maddr = "224.0.0.1";
		break;

	case AF_INET6:
		maddr = "ff02::1";
		break;

	default:
		return;
	}

	sa_set_str(&dst, maddr, PCP_PORT_CLI);

	err = pcp_reply(us, &dst, NULL, PCP_ANNOUNCE, PCP_SUCCESS,
			0, repcpd_epoch_time(), NULL);
	if (err) {
		warning("announce: failed to send ANNOUNCE from"
			" %J to %s (%m)\n", bnd_addr, maddr, err);
		return;
	}

	info("announce: sent ANNOUNCE from %J to %s\n", bnd_addr, maddr);
}


static int module_init(void)
{
	repcpd_register_handler(&announce);

	debug("announce: module loaded\n");

	repcpd_udp_apply(lstnr_handler, NULL);

	return 0;
}


static int module_close(void)
{
	repcpd_unregister_handler(&announce);

	debug("announce: module closed\n");

	return 0;
}


const struct mod_export exports = {
	.name = "announce",
	.type = "pcp",
	.init = module_init,
	.close = module_close,
};
