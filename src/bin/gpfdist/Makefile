subdir = src/bin/gpfdist
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(srcdir) $(CPPFLAGS) $(apr_includes) $(apr_cppflags)
override CFLAGS := $(CFLAGS) $(apr_cflags)

OBJS = gpfdist.o gpfdist_helper.o fstream.o gfile.o
# configure should have been run by this point.
# we are adding the gpfdist libraries here instead
# of the top level, so that the backend does not
# need to link with unnecessary libraries.
GPFDIST_LIBS = $(EVENT_LIBS)

ifeq ($(PORTNAME),aix)
  GPFDIST_LIBS += -lbsd
endif

ifeq ($(have_yaml),yes)
  OBJS += transform.o
  LIBS += $(YAML_LIBS)
  ifneq ($(PORTNAME),win32)
    override CPPFLAGS := -DGPFXDIST $(CPPFLAGS)
  endif
endif

ifeq ($(PORTNAME),win32)
  override CPPFLAGS := -I$(top_builddir)/src/port $(CPPFLAGS)
  OBJS += $(top_builddir)/src/port/glob.o
endif

LDLIBS += $(LIBS) $(GPFDIST_LIBS) $(apr_link_ld_libs)

all: gpfdist$(X)

fstream.c: $(top_builddir)/src/backend/utils/misc/fstream/fstream.c
	ln -s $< $@

gfile.c: $(top_builddir)/src/backend/utils/misc/fstream/gfile.c
	ln -s $< $@

gfile.o: CPPFLAGS := $(CPPFLAGS) -DFRONTEND

gpfdist$(X): $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(OBJS) $(LDLIBS) -o $@

install: all
	$(INSTALL) gpfdist$(X) $(DESTDIR)$(prefix)/bin

installcheck:
	$(MAKE) -C regress installcheck

.PHONY: clean distclean
clean:
	rm -f $(OBJS) gpfdist$(X)

distclean: clean
	rm -f fstream.c gfile.c GNUmakefile config.log config.status
	rm -rf autom4te.cache
	$(MAKE) -C regress clean
