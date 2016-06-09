#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= peer
$(MOD)_SRCS	+= peer.c
$(MOD)_LFLAGS	+=

include mk/mod.mk
