#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= proxy
$(MOD)_SRCS	+= proxy.c
$(MOD)_LFLAGS	+=

include mk/mod.mk
