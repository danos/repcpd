#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= iptables
$(MOD)_SRCS	+= iptables.c
$(MOD)_LFLAGS	+=

include mk/mod.mk
