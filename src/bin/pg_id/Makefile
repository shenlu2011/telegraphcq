#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_id
#
# Copyright (C) 2000 by PostgreSQL Global Development Team
#
# $Header: /project/eecs/db/cvsroot/postgres/src/bin/pg_id/Makefile,v 1.2 2004/03/24 08:11:24 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/bin/pg_id
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

all: pg_id

pg_id: pg_id.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

install: all installdirs
	$(INSTALL_PROGRAM) pg_id$(X) $(DESTDIR)$(bindir)/pg_id$(X)

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/pg_id$(X)

depend dep:
	$(CC) -MM $(CFLAGS) *.c >depend

clean distclean maintainer-clean:
	rm -f pg_id$(X) pg_id.o

ifeq (depend,$(wildcard depend))
include depend
endif
