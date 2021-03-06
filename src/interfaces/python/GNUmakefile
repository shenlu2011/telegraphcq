# $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/python/GNUmakefile,v 1.2 2004/03/24 08:11:34 chungwu Exp $

subdir = src/interfaces/python
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

NAME = _pgmodule
SO_MAJOR_VERSION = 0
SO_MINOR_VERSION = 0
OBJS = pgmodule.o
SHLIB_LINK = $(libpq)
ifeq ($(PORTNAME), win)
override CPPFLAGS += -DUSE_DL_IMPORT
SHLIB_LINK += $(python_libspec)
endif


include $(top_srcdir)/src/Makefile.shlib

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS) $(python_includespec)

all: all-lib

all-lib: libpq-all

.PHONY: libpq-all
libpq-all:
	$(MAKE) -C $(libpq_builddir) all

install-warning-msg := { \
echo "*** Skipping the installation of the Python interface module for lack"; \
echo "*** of permissions.  To install it, change to the directory"; \
echo "***     `pwd`,"; \
echo "*** become the appropriate user, and do '$(MAKE) install'."; }

install: all installdirs
	@if test -w $(DESTDIR)$(python_moduleexecdir) && test -w $(DESTDIR)$(python_moduledir); then \
	  echo "$(INSTALL_SHLIB) $(shlib) $(DESTDIR)$(python_moduleexecdir)/_pgmodule$(DLSUFFIX)"; \
	  $(INSTALL_SHLIB) $(shlib) $(DESTDIR)$(python_moduleexecdir)/_pgmodule$(DLSUFFIX); \
	\
	  echo "$(INSTALL_DATA) $(srcdir)/pg.py $(DESTDIR)$(python_moduledir)/pg.py"; \
	  $(INSTALL_DATA) $(srcdir)/pg.py $(DESTDIR)$(python_moduledir)/pg.py; \
	\
	  echo "$(INSTALL_DATA) $(srcdir)/pgdb.py $(DESTDIR)$(python_moduledir)/pgdb.py"; \
	  $(INSTALL_DATA) $(srcdir)/pgdb.py $(DESTDIR)$(python_moduledir)/pgdb.py; \
	else \
	  $(install-warning-msg); \
	fi

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(python_moduleexecdir) $(DESTDIR)$(python_moduledir)

uninstall:
	rm -f $(DESTDIR)$(python_moduleexecdir)/_pgmodule$(DLSUFFIX) \
	      $(DESTDIR)$(python_moduledir)/pg.py \
	      $(DESTDIR)$(python_moduledir)/pgdb.py

clean distclean maintainer-clean: clean-lib
	rm -f $(OBJS)
