#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for access/hash
#
# IDENTIFICATION
#    $Header: /project/eecs/db/cvsroot/postgres/src/backend/access/hash/Makefile,v 1.2 2004/03/24 07:59:40 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/access/hash
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = hash.o hashfunc.o hashinsert.o hashovfl.o hashpage.o hashscan.o \
       hashsearch.o hashstrat.o hashutil.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

depend dep:
	$(CC) -MM $(CFLAGS) *.c >depend

clean: 
	rm -f SUBSYS.o $(OBJS)

ifeq (depend,$(wildcard depend))
include depend
endif

