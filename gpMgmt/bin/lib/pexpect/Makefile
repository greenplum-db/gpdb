# gpMgmt/bin/lib/pexpect/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

PROGRAMS= __init__.py pxssh.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib/pexpect'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/lib/pexpect/'$$file ; \
	done

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(bindir)/lib/pexpect/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

