#-------------------------------------------------------------------------
#
# Makefile for parser
#
# Portions Copyright (c) 2003, Regents of the University of California
#
# $Header: /project/eecs/db/cvsroot/postgres/src/backend/parser/Makefile,v 1.7 2004/03/24 08:10:59 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/parser
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(srcdir) $(CPPFLAGS)

OBJS= analyze.o gram.o keywords.o parser.o parse_agg.o parse_clause.o \
      parse_expr.o parse_func.o parse_node.o parse_oper.o parse_relation.o \
      parse_type.o parse_wrapper.o parse_coerce.o parse_target.o scansup.o

FLEXFLAGS = -CF

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) $@ $^


# scan is compiled as part of gram
gram.o: $(srcdir)/scan.c


# There is no correct way to write a rule that generates two files.
# Rules with two targets don't have that meaning, they are merely
# shorthand for two otherwise separate rules.  To be safe for parallel
# make, we must chain the dependencies like this.  The semicolon is
# important, otherwise make will choose the built-in rule for
# gram.y=>gram.c.

$(srcdir)/gram.c: $(srcdir)/parse.h ;

# The sed hack is so that we can get the same error messages with
# bison 1.875 and later as we did with earlier bisons.  Eventually,
# I suppose, we should re-standardize on "syntax error" --- in which
# case flip the sed translation, but don't remove it.

$(srcdir)/parse.h: gram.y
ifdef YACC
	$(YACC) -d $(YFLAGS) $<
	sed -e 's/"syntax error/"parse error/' < y.tab.c > $(srcdir)/gram.c
	mv -f y.tab.h $(srcdir)/parse.h
	rm -f y.tab.c
else
	@$(missing) bison $< $@
endif


$(srcdir)/scan.c: scan.l
ifdef FLEX
	$(FLEX) $(FLEXFLAGS) -o'$@' $<
else
	@$(missing) flex $< $@
endif


# Force these dependencies to be known even without dependency info built:

keywords.o parse_clause.o parse_expr.o parser.o gram.o: $(srcdir)/parse.h


# gram.c, parse.h, and scan.c are in the distribution tarball, so they
# are not cleaned here.
clean: 
	rm -f SUBSYS.o $(OBJS)
# And the garbage that might have been left behind by partial build:
	@rm -f y.tab.c y.tab.h lex.yy.c
