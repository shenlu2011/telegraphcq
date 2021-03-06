#-------------------------------------------------------------------------
#
# Makefile for src/bin/psql
#
# Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# $Header: /project/eecs/db/cvsroot/postgres/src/bin/psql/Makefile,v 1.2 2004/03/24 08:11:25 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/bin/psql
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

REFDOCDIR= $(top_srcdir)/doc/src/sgml/ref

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS=command.o common.o help.o input.o stringutils.o mainloop.o copy.o \
	startup.o prompt.o variables.o large_obj.o print.o describe.o \
	sprompt.o tab-complete.o mbprint.o

all: submake-libpq submake-libpgport psql

psql: $(OBJS) $(libpq_builddir)/libpq.a
	$(CC) $(CFLAGS) $(OBJS) $(libpq) $(LDFLAGS) $(LIBS) -o $@

help.o: $(srcdir)/sql_help.h

ifdef PERL
$(srcdir)/sql_help.h: create_help.pl $(wildcard $(REFDOCDIR)/*.sgml)
	$(PERL) $< $(REFDOCDIR) $@
else
$(srcdir)/sql_help.h:
	@echo "*** Perl is needed to build psql help."
endif

distprep: $(srcdir)/sql_help.h

install: all installdirs
	$(INSTALL_PROGRAM) psql$(X) $(DESTDIR)$(bindir)/psql$(X)

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/psql$(X)

clean distclean:
	rm -f psql$(X) $(OBJS)

maintainer-clean: distclean
	rm -f $(srcdir)/sql_help.h
