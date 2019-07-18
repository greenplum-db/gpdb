# gpMgmt/bin/gpcheckcat_modules/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

PROGRAMS= __init__.py foreign_key_check.py leaked_schema_dropper.py \
	orphan_toast_table_issues.py orphaned_toast_tables_check.py repair.py \
	repair_missing_extraneous.py unique_index_violation_check.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/gpcheckcat_modules'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/gpcheckcat_modules/'$$file ; \
	done

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(bindir)/gpcheckcat_modules/'$$file ; \
	done

clean distclean:
	rm -f *.pyc
