#-------------------------------------------------------------------------
#
# Makefile for libpgtcl library
#
# Copyright (c) 1994, Regents of the University of California
#
# $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/libpgtcl/Makefile,v 1.2 2004/03/24 08:11:32 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/interfaces/libpgtcl
top_builddir = ../../..
include ../../Makefile.global

NAME= pgtcl
SO_MAJOR_VERSION= 2
SO_MINOR_VERSION= 3

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS= pgtcl.o pgtclCmds.o pgtclId.o

SHLIB_LINK = $(libpq)

# If crypt is a separate library, rather than part of libc, it may need
# to be referenced separately to keep (broken) linkers happy.  (This is
# braindead; users of libpq should not need to know what it depends on.)
SHLIB_LINK+= $(filter -L%, $(LDFLAGS)) $(filter -lcrypt, $(LIBS))

all: submake-libpq all-lib

# Shared library stuff
include $(top_srcdir)/src/Makefile.shlib

install: all installdirs install-headers install-lib

.PHONY: install-headers
install-headers: libpgtcl.h
	$(INSTALL_DATA) $< $(DESTDIR)$(includedir)/libpgtcl.h

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(libdir) $(DESTDIR)$(includedir)

uninstall: uninstall-lib
	rm -f $(DESTDIR)$(includedir)/libpgtcl.h

clean distclean maintainer-clean: clean-lib
	rm -f $(OBJS)

depend dep:
	$(CC) -MM $(CFLAGS) *.c >depend

ifeq (depend,$(wildcard depend))
include depend
endif
