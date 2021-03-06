SRCS		+= $(NAME).c
OBJS		+= $(NAME).o

SHLIB_LINK 	:= $(BE_DLLLIBS)

SO_MAJOR_VERSION := 0
SO_MINOR_VERSION := 0
rpath =

all: all-shared-lib

include $(top_srcdir)/src/Makefile.shlib

install: all
	$(INSTALL_SHLIB) $(shlib) $(DESTDIR)$(pkglibdir)/$(NAME)$(DLSUFFIX)

uninstall: uninstall-lib

clean distclean maintainer-clean: clean-lib
	rm -f $(OBJS)
