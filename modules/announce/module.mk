#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= announce
$(MOD)_SRCS	+= announce.c
$(MOD)_LFLAGS	+=

include mk/mod.mk
