/**
 * @file proxy.c  PCP proxy module
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>


/*
 * draft-ietf-pcp-proxy-04
 */


struct pending {
	struct le le;
	struct mbuf *mb_req;
	struct sa laddr;
	struct sa src;
	struct udp_sock *us;
	struct udp_sock *us_recv;
	struct tmr tmr;
	size_t req_size;
};


static struct sa pcp_server;
static struct list pendingl;


static void destructor(void *arg)
{
	struct pending *pend = arg;

	info("proxy: Pending context deleted -- %u bytes"
	     " from %J to %J\n", pend->req_size,
	     &pend->src, &pcp_server);

	list_unlink(&pend->le);
	tmr_cancel(&pend->tmr);
	mem_deref(pend->mb_req);
	mem_deref(pend->us);
	mem_deref(pend->us_recv);
}


static void udp_recv(const struct sa *src, struct mbuf *mb, void *arg)
{
	struct pending *pend = arg;
	int err;

	info("proxy: received %u bytes from %J\n", mb->end, src);

	/* remove any options (e.g. THIRD_PARTY) added by our proxy */
	mb->end = pend->req_size;

	err = udp_send(pend->us_recv, &pend->src, mb);
	if (err) {
		warning("proxy: could not send %u bytes to %J (%m)\n",
			mb->end, &pend->src, err);
	}

	mem_deref(pend);
}


static void timeout(void *arg)
{
	struct pending *pend = arg;

	info("proxy: request timed out\n");

	/* todo: reply error ? what does the I-D say ? */

	mem_deref(pend);
}


static int proxy_request(struct pending **pendp, struct udp_sock *us_recv,
			 const struct sa *src, struct mbuf *mb)
{
	struct pending *pend;
	int err;

	pend = mem_zalloc(sizeof(*pend), destructor);
	if (!pend)
		return ENOMEM;

	pend->mb_req   = mem_ref(mb);
	pend->src      = *src;
	pend->us_recv  = mem_ref(us_recv);
	pend->req_size = mbuf_get_left(mb);

	sa_init(&pend->laddr, sa_af(&pcp_server));

	err = udp_listen(&pend->us, &pend->laddr, udp_recv, pend);
	if (err)
		goto out;

	err = udp_connect(pend->us, &pcp_server);
	if (err)
		goto out;
	err = udp_local_get(pend->us, &pend->laddr);
	if (err)
		goto out;

	list_append(&pendingl, &pend->le, pend);

 out:
	if (err)
		mem_deref(pend);
	else if (pendp)
		*pendp = pend;

	return err;
}


static int pcp_proxy_send(struct udp_sock *us, const struct sa *dst,
			  struct mbuf *mb, const struct sa *cli_addr,
			  const struct sa *third_party)
{
	size_t start;
	int err;

	if (!us || !dst || !mb || !cli_addr || !third_party)
		return EINVAL;

	start = mb->pos;
	mb->pos = start + 8;

	err = pcp_ipaddr_encode(mb, cli_addr);
	if (err)
		goto out;

	mb->pos = mb->end;

	err = pcp_option_encode(mb, PCP_OPTION_THIRD_PARTY, third_party);
	if (err)
		goto out;

	mb->pos = start;

	err = udp_send(us, dst, mb);
	if (err)
		goto out;

 out:
	mb->pos = start;

	return err;
}


static bool request_handler(struct udp_sock *us, const struct sa *src,
			    struct mbuf *mb, struct pcp_msg *msg)
{
	struct pending *pend;
	int err;

	if (msg->hdr.opcode == PCP_ANNOUNCE)
		return false;

	info("proxy: proxying %s request from %J to %J\n",
	     pcp_opcode_name(msg->hdr.opcode), src, &pcp_server);

	err = proxy_request(&pend, us, src, mb);
	if (err)
		goto out;

	err = pcp_proxy_send(pend->us, &pcp_server, mb,
			     &pend->laddr, src);
	if (err)
		goto out;

	tmr_start(&pend->tmr, 20*1000, timeout, pend);

 out:
	if (err) {
		warning("proxy: failed to proxy request from %J (%m)\n",
			src, err);

		pcp_ereply(us, src, mb, PCP_NO_RESOURCES);
	}

	return true;
}


static struct repcpd_pcp proxy = {
	.reqh = request_handler,
};


static int module_init(void)
{
	int err;

	err = conf_get_sa(_conf(), "proxy_target", &pcp_server);
	if (err) {
		warning("proxy: missing 'proxy_target' in config\n");
		return err;
	}

	repcpd_register_handler(&proxy);

	debug("proxy: module loaded (target PCP server is %J)\n",
	      &pcp_server);

	return 0;
}


static int module_close(void)
{
	list_flush(&pendingl);

	repcpd_unregister_handler(&proxy);

	debug("proxy: module closed\n");

	return 0;
}


const struct mod_export exports = {
	.name = "proxy",
	.type = "pcp",
	.init = module_init,
	.close = module_close,
};
