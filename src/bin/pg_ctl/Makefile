#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_ctl
#
# Copyright (c) 1999, PostgreSQL Global Development Group
#
# $Header: /project/eecs/db/cvsroot/postgres/src/bin/pg_ctl/Makefile,v 1.2 2004/03/24 08:11:21 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/bin/pg_ctl
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

all: pg_ctl

pg_ctl: pg_ctl.sh
	sed -e 's/@VERSION@/$(VERSION)/g' \
	    -e 's,@bindir@,$(bindir),g' \
	  $< >$@
	chmod a+x $@

install: all installdirs
	$(INSTALL_SCRIPT) pg_ctl $(DESTDIR)$(bindir)/pg_ctl

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/pg_ctl

clean distclean maintainer-clean:
	rm -f pg_ctl
