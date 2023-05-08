# gpMgmt/bin/gppylib/programs/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

PROGRAMS= initstandby.py test_utils_helper.py

DATA= __init__.py clsAddMirrors.py clsRecoverSegment.py clsSystemState.py \
	programIoUtils.py clsRecoverSegment_triples.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/programs'

install: installdirs
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(libdir)/python/gppylib/programs/'$$file ; \
	done
	$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(libdir)/python/gppylib/programs/clsAddMirrors.py'
	$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(libdir)/python/gppylib/programs/clsRecoverSegment.py'
	$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(libdir)/python/gppylib/programs/clsRecoverSegment_triples.py'
	$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(libdir)/python/gppylib/programs/clsSystemState.py'

uninstall:
	for file in $(PROGRAMS) $(DATA); do \
		rm -f '$(DESTDIR)$(libdir)/python/gppylib/programs/'$$file ; \
	done

clean distclean:
	rm -f *.pyc
