# gpMgmt/bin/lib/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS= pexpect

$(recurse)

PROGRAMS= __init__.py gp_bash_functions.sh gp_bash_version.sh gpconfigurenewsegment \
	gpcreateseg.sh gphostcachelookup.py gppinggpfdist.py gpstate.py multidd

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/lib/'$$file ; \
	done
	$(INSTALL_DATA) crashreport.gdb '$(DESTDIR)$(bindir)/lib/crashreport.gdb'

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(bindir)/lib/'$$file ; \
	done
	rm -f '$(DESTDIR)$(bindir)/lib/crashreport.gdb'

distclean: clean
	rm -f gp_bash_version.sh

clean:
	rm -f *.pyc

