# $Header: /project/eecs/db/cvsroot/postgres/src/test/locale/Makefile,v 1.2 2004/03/24 08:11:37 chungwu Exp $

subdir = src/test/locale
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global


PROGS = test-ctype
DIRS = de_DE.ISO8859-1 gr_GR.ISO8859-7 koi8-r koi8-to-win1251

all: $(PROGS)

clean: 
	rm -f $(PROGS)
	for d in $(DIRS); do \
		$(MAKE) -C $$d clean; \
	done

check-%: all
	@$(MAKE) -C `echo $@ | sed 's/^check-//'` test
