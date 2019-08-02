# gpMgmt/bin/gpconfig_modules/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

PROGRAMS= __init__.py compare_segment_guc.py database_segment_guc.py \
	file_segment_guc.py guc_collection.py parse_guc_metadata.py \
	segment_guc.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/gpconfig_modules'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/gpconfig_modules/'$$file ; \
	done

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(bindir)/gpconfig_modules/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

