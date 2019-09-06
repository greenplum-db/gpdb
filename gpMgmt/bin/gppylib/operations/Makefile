# gpMgmt/bin/gppylib/operations/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

PROGRAMS= initstandby.py test_utils_helper.py

DATA= __init__.py buildMirrorSegments.py deletesystem.py package.py \
	rebalanceSegments.py reload.py segment_reconfigurer.py startSegments.py \
	unix.py utils.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/operations'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(libdir)/python/gppylib/operations/'$$file ; \
	done
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(libdir)/python/gppylib/operations/'$$file ; \
	done

uninstall:
	for file in $(PROGRAMS) $(DATA); do \
		rm -f '$(DESTDIR)$(libdir)/python/gppylib/operations/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

