#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for postmaster
#
# IDENTIFICATION
#    $Header: /project/eecs/db/cvsroot/postgres/src/backend/postmaster/Makefile,v 1.2 2004/03/24 08:11:05 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/postmaster
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = postmaster.o pgstat.o

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
