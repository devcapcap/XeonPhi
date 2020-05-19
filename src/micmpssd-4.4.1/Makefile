# Intel MIC Platform Software Stack (MPSS)
# Copyright (c) 2016, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.

INSTALL = install
INSTALL_d = $(INSTALL) -d
INSTALL_f = $(INSTALL) -m 644

prefix = /usr
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sbindir = $(exec_prefix)/sbin
datarootdir = $(prefix)/share
sysconfdir = /etc

CROSS ?= k1om-mpss-linux-
CC ?= $(CROSS)gcc
CFLAGS ?= -g
LDFLAGS ?= -g
LDLIBS = -pthread -lscif

ALL_CFLAGS = $(CFLAGS)
ALL_LDFLAGS = $(LDFLAGS)

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += $(USERWARNFLAGS)

FILES = mpssd.c

HEADERS =
PROGRAMS = mpssd

.PHONY: all install clean

all: $(PROGRAMS)

install: $(PROGRAMS)
	$(INSTALL_d) $(DESTDIR)$(sbindir)
	$(INSTALL) mpssd $(DESTDIR)$(sbindir)
	$(INSTALL_d) $(DESTDIR)$(sysconfdir)/init.d
	$(INSTALL_d) $(DESTDIR)$(sysconfdir)/rc5.d
	$(INSTALL) mpss $(DESTDIR)$(sysconfdir)/init.d
	ln -s ../init.d/mpss $(DESTDIR)$(sysconfdir)/rc5.d/S80mpss

clean:
	rm -f $(PROGRAMS) $(FILES:%.c=%.o)

$(PROGRAMS): mpssd.o
	$(CC) $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(FILES:%.c=%.o): %.o: %.c $(HEADERS)
	$(CC) $(ALL_CFLAGS) $< -c -o $@
