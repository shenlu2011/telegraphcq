#-------------------------------------------------------------------------
#
# Makefile for ecpg library
#
# Copyright (c) 1994, Regents of the University of California
#
# $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/ecpg/lib/Makefile,v 1.2 2004/03/24 08:11:30 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/interfaces/ecpg/lib
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

NAME= ecpg
SO_MAJOR_VERSION= 3
SO_MINOR_VERSION= 4.1

override CPPFLAGS := -I$(top_srcdir)/src/interfaces/ecpg/include -I$(libpq_srcdir) $(CPPFLAGS)


OBJS= execute.o typename.o descriptor.o data.o error.o prepare.o memory.o \
	connect.o misc.o

SHLIB_LINK= $(libpq)

all: all-lib

# Shared library stuff
include $(top_srcdir)/src/Makefile.shlib

install: all installdirs install-lib

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(libdir)

uninstall: uninstall-lib

clean distclean maintainer-clean: clean-lib
	rm -f $(OBJS)

depend dep:
	$(CC) -MM $(CFLAGS) *.c >depend

ifeq (depend,$(wildcard depend))
include depend
endif
