#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for access/index
#
# IDENTIFICATION
#    $Header: /project/eecs/db/cvsroot/postgres/src/backend/access/index/Makefile,v 1.2 2004/03/24 08:10:48 chungwu Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/access/index
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = genam.o indexam.o istrat.o

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
