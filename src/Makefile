#-------------------------------------------------------------------------
#
# Makefile for src
#
# Copyright (c) 1994, Regents of the University of California
#
# $Header: /project/eecs/db/cvsroot/postgres/src/Makefile,v 1.5 2004/07/15 20:39:53 owenc Exp $
#
#-------------------------------------------------------------------------

subdir = src
top_builddir = ..
include Makefile.global


all install installdirs uninstall dep depend distprep:
	$(MAKE) -C port $@
	$(MAKE) -C backend $@
	$(MAKE) -C backend/utils/mb/conversion_procs $@
	$(MAKE) -C include $@
	$(MAKE) -C interfaces $@
	$(MAKE) -C bin $@
	$(MAKE) -C pl $@

install-all-headers:
	$(MAKE) -C include $@

clean:
	$(MAKE) -C port $@
	$(MAKE) -C backend $@
	$(MAKE) -C include $@
	$(MAKE) -C interfaces $@
	$(MAKE) -C bin $@
	$(MAKE) -C pl $@
	$(MAKE) -C test $@
	$(MAKE) -C tutorial $@
	$(MAKE) -C utils $@

distclean maintainer-clean:
	-$(MAKE) -C port $@
	-$(MAKE) -C backend $@
	-$(MAKE) -C include $@
	-$(MAKE) -C interfaces $@
	-$(MAKE) -C bin $@
	-$(MAKE) -C pl $@
	-$(MAKE) -C test $@
	-$(MAKE) -C tutorial $@
	-$(MAKE) -C utils $@
	rm -f Makefile.port Makefile.global
