#
# Makefile for utils/adt
#
# $Header: /project/eecs/db/cvsroot/postgres/src/backend/utils/adt/Makefile,v 1.5 2004/03/24 08:11:11 chungwu Exp $
#

subdir = src/backend/utils/adt
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

# seems to be required for some date/time stuff 1999/07/22 bjm
ifeq "$(findstring alpha,$(host_cpu))" "alpha"
ifeq "$(GCC)" "yes"
override CFLAGS+= -mieee
endif
endif

OBJS = acl.o arrayfuncs.o arrayutils.o bool.o cash.o char.o \
	date.o datetime.o datum.o float.o format_type.o \
	geo_ops.o geo_selfuncs.o int.o int8.o like.o lockfuncs.o \
	misc.o nabstime.o name.o not_in.o numeric.o numutils.o \
	oid.o oracle_compat.o pseudotypes.o \
	regexp.o regproc.o ruleutils.o selfuncs.o sets.o \
	tid.o timestamp.o varbit.o varchar.o varlena.o version.o \
	network.o mac.o inet_net_ntop.o inet_net_pton.o \
	ri_triggers.o pg_lzcompress.o pg_locale.o formatting.o \
	ascii.o quote.o pgstatfuncs.o encode.o window_agg.o 

like.o: like.c like_match.c

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
