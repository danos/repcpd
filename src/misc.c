/**
 * @file misc.c  PCP misc functions
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>
#include "pcpd.h"


int conf_get_sa(const struct conf *conf, const char *name, struct sa *sa)
{
	struct pl pl;
	int err;

	err = conf_get(conf, name, &pl);
	if (err)
		return err;

	err = sa_decode(sa, pl.p, pl.l);
	if (err)
		return err;

	return 0;
}
