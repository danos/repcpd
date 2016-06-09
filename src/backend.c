/**
 * @file backend.c  PCP mapping backend api
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>
#include "pcpd.h"


static struct list backendl;


void backend_register(struct backend *be)
{
	list_append(&backendl, &be->le, be);
}


void backend_unregister(struct backend *be)
{
	list_unlink(&be->le);
}


struct backend *backend_get(void)
{
	return list_ledata(backendl.head);
}
