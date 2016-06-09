/**
 * @file pcp.c  PCP Server
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <string.h>
#include <re.h>
#include <repcpd.h>
#include "pcpd.h"


static struct {
	struct list pcpl;
} pcpx;


void repcpd_process_msg(struct udp_sock *us, const struct sa *src,
			const struct sa *dst, struct mbuf *mb)
{
	struct pcp_option *opt;
	struct pcp_msg *msg = NULL;
	enum pcp_result result = PCP_SUCCESS;
	struct le *le;
	size_t start;
	bool handled = false;
	int err;

	if (!us || !src || !dst || !mb)
		return;

	debug("pcp: received %zu bytes from %J\n",
	      mbuf_get_left(mb), src);

	if (mbuf_get_left(mb) < PCP_MIN_PACKET) {
		warning("pcp: ignore short packet from %J\n", src);
		return;
	}

	start = mb->pos;

	err = pcp_msg_decode(&msg, mb);
	mb->pos = start;
	if (err) {
		warning("pcp: could not decode message from %J (%m)\n",
			src, err);

		result = PCP_MALFORMED_REQUEST;
		goto out;
	}

	debug("pcp: %H\n", pcp_msg_printhdr, msg);

	/* Validate PCP request */
	if (msg->hdr.resp) {
		warning("pcp: ignore response from %J\n", src);
		goto out;
	}

	if (msg->hdr.version != PCP_VERSION) {
		result = PCP_UNSUPP_VERSION;
		goto out;
	}

	if (!sa_cmp(&msg->hdr.cli_addr, src, SA_ADDR)) {
		warning("pcp: client-addr is %j, but source-addr is %j\n",
			&msg->hdr.cli_addr, src);
		result = PCP_ADDRESS_MISMATCH;
		goto out;
	}

	opt = pcp_msg_option(msg, PCP_OPTION_THIRD_PARTY);
	if (opt) {

		/* todo: add config to decide if
		   THIRD_PARTY is allowed or not */

		if (sa_cmp(&opt->u.third_party, src, SA_ADDR)) {

			warning("pcp: THIRD_PARTY is same as"
				" source address (%J)\n", src);
			result = PCP_MALFORMED_REQUEST;
			goto out;
		}
	}

	/* Handle PCP Request in the modules */
	le = pcpx.pcpl.head;
	while (le) {
		struct repcpd_pcp *st = le->data;

		le = le->next;

		if (st->reqh && st->reqh(us, src, mb, msg)) {
			handled = true;
			break;
		}
	}

	if (!handled)
		result = PCP_UNSUPP_OPCODE;

 out:
	mem_deref(msg);

	if (result != PCP_SUCCESS) {

		err = pcp_ereply(us, src, mb, result);
		if (err) {
			warning("pcp: ereply failed (%m)\n", err);
		}
	}
}


void repcpd_register_handler(struct repcpd_pcp *pcp)
{
	if (!pcp)
		return;

	list_append(&pcpx.pcpl, &pcp->le, pcp);
}


void repcpd_unregister_handler(struct repcpd_pcp *pcp)
{
	if (!pcp)
		return;

	list_unlink(&pcp->le);
}


int pcp_ereply(struct udp_sock *us, const struct sa *dst, struct mbuf *req,
	       enum pcp_result result)
{
	enum pcp_opcode opcode;

	if (!us || !dst || !req)
		return EINVAL;

	opcode = mbuf_buf(req)[1];

	info("pcp: reply error to %J -- opcode=%s result=%s req=%u bytes\n",
	      dst, pcp_opcode_name(opcode), pcp_result_name(result), req->end);

	return pcp_reply(us, dst, req, opcode, result, 0,
			 repcpd_epoch_time(), NULL);
}


bool pcp_nonce_cmp(const struct pcp_msg *msg,
		   const uint8_t nonce[PCP_NONCE_SZ])
{
	if (!msg || !nonce)
		return false;

	return 0 == memcmp(msg->pld.map.nonce, nonce, PCP_NONCE_SZ);
}


uint32_t pcp_lifetime_calculate(uint32_t lifetime)
{
	if (lifetime < LIFETIME_MIN)
		return LIFETIME_MIN;
	if (lifetime > LIFETIME_MAX)
		return LIFETIME_MAX;

	return lifetime;
}
