# $Header: /project/eecs/db/cvsroot/postgres/contrib/tsearch/Makefile,v 1.1.1.2 2003/04/17 22:34:53 sailesh Exp $

subdir = contrib/tsearch
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I. $(CPPFLAGS)

MODULE_big = tsearch
OBJS = crc32.o morph.o txtidx.o query.o gistidx.o rewrite.o

DATA_built = tsearch.sql
DOCS = README.tsearch
REGRESS = tsearch

# parser is compiled as part of query
query.o: parser.c

parser.c: parser.l
ifdef FLEX
	$(FLEX) $(FLEXFLAGS) -8 -Ptsearch_yy -o'$@' $<
else
	@$(missing) flex $< $@
endif

EXTRA_CLEAN = parser.c

include $(top_srcdir)/contrib/contrib-global.mk
# DO NOT DELETE
