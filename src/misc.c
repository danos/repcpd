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


int conf_get_range(const struct conf *conf, const char *name,
		   uint32_t *min_value, uint32_t *max_value)
{
	struct pl val, pl_min, pl_max;
	int err;

	if (!conf || !name || !min_value || !max_value)
		return EINVAL;

	err = conf_get(conf, name, &val);
	if (err)
		return err;

	err = re_regex(val.p, val.l, "[0-9]+-[0-9]+", &pl_min, &pl_max);
	if (err) {
		warning("conf: %s: could not parse range: (%r)\n",
			name, &val);
		return err;
	}

	*min_value = pl_u32(&pl_min);
	*max_value = pl_u32(&pl_max);

	if (*min_value > *max_value) {
		warning("conf: %s: invalid range (%u - %u)\n",
			name, *min_value, *max_value);
		return EINVAL;
	}

	return 0;
}
