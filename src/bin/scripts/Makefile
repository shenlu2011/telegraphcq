#-------------------------------------------------------------------------
#
# Makefile for src/bin/scripts
#
# Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# $Header: /project/eecs/db/cvsroot/postgres/src/bin/scripts/Makefile,v 1.2 2004/03/24 08:11:27 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/bin/scripts
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SCRIPTS := createdb dropdb createuser dropuser createlang droplang vacuumdb \
           clusterdb

all: $(SCRIPTS)

# we don't really need to edit createlang.sh at the moment,
# but keep the old generation procedure in case we need it again.
createlang: createlang.sh
	cp $< $@
	chmod a+x $@

install: all installdirs
	for i in $(filter-out createlang, $(SCRIPTS)); do \
	  $(INSTALL_SCRIPT) $(srcdir)/$$i $(DESTDIR)$(bindir)/$$i || exit; \
	done
	$(INSTALL_SCRIPT) createlang $(DESTDIR)$(bindir)/createlang

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(addprefix $(DESTDIR)$(bindir)/, $(SCRIPTS))

clean distclean maintainer-clean:
	rm -f createlang
