#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for storage/smgr
#
# IDENTIFICATION
#    $Header: /project/eecs/db/cvsroot/postgres/src/backend/storage/smgr/Makefile,v 1.2 2004/03/24 08:11:09 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/storage/smgr
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = md.o mm.o smgr.o smgrtype.o

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
