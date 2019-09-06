# gpMgmt/doc/Makefile

top_builddir = ../..
include $(top_builddir)/src/Makefile.global

SUBDIRS= gpconfigs

$(recurse)

DOCS= gpactivatestandby_help gpaddmirrors_help gpcheckperf_help \
	gpconfig_help gpdeletesystem_help gpexpand_help gpfdist_help \
	gpinitstandby_help gpinitsystem_help gpload_help gplogfilter_help \
	gpmapreduce_help gpperfmon_install_help gppkg_help gprecoverseg_help \
	gpreload_help gpscp_help gpssh-exkeys_help gpssh_help gpstart_help \
	gpstate_help gpstop_help gpsys1_help

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(prefix)/docs/cli_help'

install: installdirs
	for file in $(DOCS); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(prefix)/docs/cli_help/'$$file ; \
	done

uninstall:
	for file in $(DOCS); do \
		rm -f '$(DESTDIR)$(prefix)/docs/cli_help/'$$file ; \
	done

