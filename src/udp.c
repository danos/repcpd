/**
 * @file udp.c UDP Transport
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>
#include "pcpd.h"


struct udp_lstnr {
	struct le le;
	struct sa bnd_addr;
	struct udp_sock *us;
};


static struct list lstnrl;


static void udp_recv(const struct sa *src, struct mbuf *mb, void *arg)
{
	struct udp_lstnr *ul = arg;

	repcpd_process_msg(ul->us, src, &ul->bnd_addr, mb);
}


static void destructor(void *arg)
{
	struct udp_lstnr *ul = arg;

	list_unlink(&ul->le);
	mem_deref(ul->us);
}


static int listen_handler(const struct pl *addrport, void *arg)
{
	struct udp_lstnr *ul = NULL;
	int err = ENOMEM;
	(void)arg;

	ul = mem_zalloc(sizeof(*ul), destructor);
	if (!ul) {
		warning("udp listen error: %m\n", err);
		goto out;
	}

	list_append(&lstnrl, &ul->le, ul);

	err = sa_decode(&ul->bnd_addr, addrport->p, addrport->l);
	if (err || sa_is_any(&ul->bnd_addr) || !sa_port(&ul->bnd_addr)) {
		warning("bad udp_listen directive: '%r'\n", addrport);
		err = EINVAL;
		goto out;
	}

	err = udp_listen(&ul->us, &ul->bnd_addr, udp_recv, ul);
	if (err) {
		warning("udp listen %J: %m\n", &ul->bnd_addr, err);
		goto out;
	}

	debug("udp listen: %J\n", &ul->bnd_addr);

 out:
	if (err)
		mem_deref(ul);

	return err;
}


int repcpd_udp_init(void)
{
	int err;

	list_init(&lstnrl);

	err = conf_apply(_conf(), "udp_listen", listen_handler, 0);
	if (err)
		goto out;

 out:
	if (err)
		repcpd_udp_close();

	return err;
}


void repcpd_udp_close(void)
{
	list_flush(&lstnrl);
}


void repcpd_udp_apply(repcpd_udp_apply_h *h, void *arg)
{
	struct le *le;

	if (!h)
		return;

	for (le = lstnrl.head; le; le = le->next) {
		struct udp_lstnr *ul = le->data;

		h(&ul->bnd_addr, ul->us, arg);
	}
}
