default: all

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SERVER_OBJS=gpnetbenchServer.o
CLIENT_OBJS=gpnetbenchClient.o

OBJS = $(SERVER_OBJS) $(CLIENT_OBJS)

all: gpnetbenchServer gpnetbenchClient

gpnetbenchServer: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SERVER_OBJS) -o $@$(X)

gpnetbenchClient: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CLIENT_OBJS) -o $@$(X)

clean distclean maintainer-clean:
	rm -rf $(OBJS) gpnetbenchServer$(X) gpnetbenchClient$(X)

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib'

install: all installdirs
	$(INSTALL_PROGRAM) gpnetbenchClient$(X) '$(DESTDIR)$(bindir)/lib/gpnetbenchClient$(X)'
	$(INSTALL_PROGRAM) gpnetbenchServer$(X) '$(DESTDIR)$(bindir)/lib/gpnetbenchServer$(X)'
