# $Header: /project/eecs/db/cvsroot/postgres/contrib/chkpass/Makefile,v 1.1.1.2 2003/04/17 22:33:50 sailesh Exp $

subdir = contrib/chkpass
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

MODULE_big = chkpass
OBJS = chkpass.o
SHLIB_LINK = $(filter -lcrypt, $(LIBS))
DATA_built = chkpass.sql
DOCS = README.chkpass

include $(top_srcdir)/contrib/contrib-global.mk
