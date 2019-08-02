# gpMgmt/bin/gpssh_modules/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

PROGRAMS= __init__.py gppxssh_wrapper.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/gpssh_modules'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/gpssh_modules/'$$file ; \
	done

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(bindir)/gpssh_modules/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

