# Makefile for PL/Perl
# $Header: /project/eecs/db/cvsroot/postgres/src/pl/plperl/GNUmakefile,v 1.2 2004/03/24 08:11:35 chungwu Exp $

subdir = src/pl/plperl
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

ifeq ($(perl_useshrplib),true)
shared_libperl = yes
endif

# If we don't have a shared library and the platform doesn't allow it
# to work without, we have to skip it.
ifneq (,$(findstring yes, $(shared_libperl)$(allow_nonpic_in_shlib)))

# The code isn't clean with regard to these warnings.
ifeq ($(GCC),yes)
override CFLAGS := $(filter-out -Wall -Wmissing-declarations -Wmissing-prototypes, $(CFLAGS))
endif

override CPPFLAGS := -I$(srcdir) -I$(perl_archlibexp)/CORE $(CPPFLAGS)


NAME = plperl
SO_MAJOR_VERSION = 0
SO_MINOR_VERSION = 0

OBJS = plperl.o eloglvl.o SPI.o
SHLIB_LINK = $(perl_embed_ldflags) $(BE_DLLLIBS)

include $(top_srcdir)/src/Makefile.shlib


all: all-lib

SPI.c: SPI.xs
	$(PERL) $(perl_privlibexp)/ExtUtils/xsubpp -typemap $(perl_privlibexp)/ExtUtils/typemap $< >$@

install: all installdirs
	$(INSTALL_SHLIB) $(shlib) $(DESTDIR)$(pkglibdir)/plperl$(DLSUFFIX)

installdirs:
	$(mkinstalldirs) $(DESTDIR)$(pkglibdir)

uninstall:
	rm -f $(DESTDIR)$(pkglibdir)/plperl$(DLSUFFIX)

clean distclean maintainer-clean: clean-lib
	rm -f SPI.c $(OBJS)

else # can't build

all:
	@echo ""; \
	 echo "*** Cannot build PL/Perl because libperl is not a shared library."; \
	 echo "*** You might have to rebuild your Perl installation.  Refer to"; \
	 echo "*** the documentation for details."; \
	 echo ""

endif # can't build
