# $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/ecpg/test/Makefile,v 1.2 2004/03/24 08:11:31 chungwu Exp $

subdir = src/interfaces/ecpg/test
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(srcdir)/../include $(CPPFLAGS) -g

ECPG = ../preproc/ecpg -I$(srcdir)/../include

TESTS = test1 test2 test3 test4 perftest dyntest dyntest2 test_notice test_code100 test_init testdynalloc

all: $(TESTS)

%: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -L../lib -L../../libpq $^ $(LIBS) -lecpg -lpq -o $@

%.c: %.pgc
	$(ECPG) $<

clean:
	rm -f $(TESTS) $(TESTS:%=%.o) $(TESTS:%=%.c) log
