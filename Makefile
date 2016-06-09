#
# Makefile
#
# Copyright (C) 2010 - 2016 Creytiv.com
#

VER_MAJOR := 0
VER_MINOR := 4
VER_PATCH := 0

PROJECT   := repcpd
VERSION   := $(VER_MAJOR).$(VER_MINOR).$(VER_PATCH)

# PCP modules
MODULES	  := announce
MODULES   += map
MODULES   += peer

MODULES   += proxy

# backend modules
MODULES	  += iptables

# deployment modules
MODULES	  += syslog

MODULES	  += $(EXTRA_MODULES)

LIBRE_MK  := $(shell [ -f ../re/mk/re.mk ] && \
	echo "../re/mk/re.mk")
ifeq ($(LIBRE_MK),)
LIBRE_MK  := $(shell [ -f /usr/share/re/re.mk ] && \
	echo "/usr/share/re/re.mk")
endif
ifeq ($(LIBRE_MK),)
LIBRE_MK  := $(shell [ -f /usr/local/share/re/re.mk ] && \
	echo "/usr/local/share/re/re.mk")
endif

include $(LIBRE_MK)


INSTALL := install
ifeq ($(DESTDIR),)
PREFIX  := /usr/local
else
PREFIX  := /usr
endif
SBINDIR	:= $(PREFIX)/sbin
DATADIR := $(PREFIX)/share
ifeq ($(LIBDIR),)
LIBDIR  := $(PREFIX)/lib
endif
MOD_PATH:= $(LIBDIR)/$(PROJECT)/modules
CFLAGS	+= -I$(LIBRE_INC) -Iinclude
CFLAGS  += -I$(SYSROOT)/local/include/rew
BIN	:= $(PROJECT)$(BIN_SUFFIX)
MOD_BINS:= $(patsubst %,%.so,$(MODULES))
APP_MK	:= src/srcs.mk
MOD_MK	:= $(patsubst %,modules/%/module.mk,$(MODULES))
MOD_BLD	:= $(patsubst %,$(BUILD)/modules/%,$(MODULES))

LIBS	+= -lrew

include $(APP_MK)
include $(MOD_MK)

OBJS	?= $(patsubst %.c,$(BUILD)/src/%.o,$(SRCS))

all: $(MOD_BINS) $(BIN)

-include $(OBJS:.o=.d)

# GPROF requires static linking
$(BIN): $(OBJS)
	@echo "  LD      $@"
ifneq ($(GPROF),)
	@$(LD) $(LFLAGS) $(APP_LFLAGS) $^ ../re/libre.a $(LIBS) -o $@
else
	@$(LD) $(LFLAGS) $(APP_LFLAGS) $^ -L$(LIBRE_SO) -lre $(LIBS) -o $@
endif

$(BUILD)/%.o: %.c $(BUILD) Makefile $(APP_MK)
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -o $@ -c $< $(DFLAGS)

$(BUILD): Makefile
	@mkdir -p $(BUILD)/src $(MOD_BLD)
	@touch $@

clean:
	@rm -rf $(BIN) $(MOD_BINS) $(BUILD)

install: $(BIN) $(MOD_BINS)
	@mkdir -p $(DESTDIR)$(SBINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(SBINDIR)
	@mkdir -p $(DESTDIR)$(MOD_PATH)
	$(INSTALL) -m 0644 $(MOD_BINS) $(DESTDIR)$(MOD_PATH)

config:
	@mkdir -p $(DESTDIR)/etc
	$(INSTALL) -m 0644 etc/repcpd.conf $(DESTDIR)/etc/.
