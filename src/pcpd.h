/**
 * @file pcpd.h PCP Internal interface
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */


/* udp */
int  repcpd_udp_init(void);
void repcpd_udp_close(void);


/* pcp */
void repcpd_process_msg(struct udp_sock *us,
			const struct sa *src, const struct sa *dst,
			struct mbuf *mb);
