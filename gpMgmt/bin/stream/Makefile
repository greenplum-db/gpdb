# gpMgmt/bin/stream/Makefile

top_builddir=../../..
include $(top_builddir)/src/Makefile.global

OBJS= stream.o

all: stream

stream: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@$(X)

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib'
	# This install location is due to legacy reasons kept for backwards
	# compatibility until it can be deprecated.
	$(MKDIR_P) '$(DESTDIR)$(bindir)/stream'

install: stream installdirs
	$(INSTALL_PROGRAM) stream$(X) '$(DESTDIR)$(bindir)/lib/stream$(X)'
	# Symlink bin/lib/stream to bin/stream to maintain backward compatibility
	if [ ! -f $(DESTDIR)$(bindir)/stream/stream$(X) ] && [ ! -L $(DESTDIR)$(bindir)/stream/stream$(X) ]; then \
		$(LN_S) ../lib/stream$(X) $(DESTDIR)$(bindir)/stream/stream$(X); \
	fi

uninstall:
	rm -f '$(DESTDIR)$(bindir)/stream/stream$(X)'
	rm -f '$(DESTDIR)$(bindir)/lib/stream$(X)'

clean:
	rm -f stream$(X) $(OBJS)
