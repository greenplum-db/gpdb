subdir = gpMgmt/bin/ifaddrs
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS := main.o

all: ifaddrs

ifaddrs: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LDFLAGS_EX) -o $@$(X)

install: all installdirs
	$(INSTALL_PROGRAM) ifaddrs$(X) '$(DESTDIR)$(libexecdir)'/ifaddrs$(X)

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libexecdir)'

uninstall:
	rm -f '$(DESTDIR)$(libexecdir)/ifaddrs$(X)'

clean distclean maintainer-clean:
	rm -f ifaddrs$(X) $(OBJS)
