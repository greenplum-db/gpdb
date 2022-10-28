# gpMgmt/bin/lib/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS= pexpect

$(recurse)

PROGRAMS= __init__.py \
	gp_bash_functions.sh \
	gp_bash_version.sh \
	gpconfigurenewsegment \
	gpcreateseg.sh \
	gppinggpfdist.py \
	gpstate.py \
	multidd

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib'

standalone_dir=./standalone/gpmgmt_bin_lib
installstandalone: installdirs
ifeq ($(enable_standalone_utils),yes)
	python3 ../../setup.py build --force && \
	python3 ../../setup.py install_exe --skip-build --install-dir="$(libdir)/$(standalone_dir)"; \
	for file_name in $(PROGRAMS); do \
		if [ -e $(libdir)/$(standalone_dir)/__frozen__$$file_name ]; then \
			ln -sf $(libdir)/$(standalone_dir)/__frozen__$$file_name $(DESTDIR)$(bindir)/lib/$$file_name; \
		fi \
	done
endif


install: installdirs installstandalone
	for file in $(PROGRAMS); do \
		if [ ! -e $(DESTDIR)$(bindir)/lib/$$file ]; then \
			$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/lib/'$$file ; \
		fi \
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

