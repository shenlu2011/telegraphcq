#
# Makefile for the storage manager subsystem
#
# $Header: /project/eecs/db/cvsroot/postgres/src/backend/storage/Makefile,v 1.2 2004/03/24 08:11:06 chungwu Exp $
#

subdir = src/backend/storage
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS     := buffer file freespace ipc large_object lmgr page smgr
SUBDIROBJS  := $(SUBDIRS:%=%/SUBSYS.o)

all: SUBSYS.o

SUBSYS.o: $(SUBDIROBJS)
	$(LD) $(LDREL) $(LDOUT) $@ $^

$(SUBDIROBJS): $(SUBDIRS:%=%-recursive) ;

.PHONY: $(SUBDIRS:%=%-recursive)
$(SUBDIRS:%=%-recursive):
	$(MAKE) -C $(subst -recursive,,$@) SUBSYS.o

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit; done
	rm -f SUBSYS.o

dep depend: 
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit; done
