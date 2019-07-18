# gpMgmt/doc/gpconfigs/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

DATA= gpinitsystem_config gpinitsystem_singlenode gpinitsystem_test \
	hostfile_exkeys hostfile_gpchecknet_ic1 hostfile_gpchecknet_ic2 \
	hostfile_gpcheckperf hostfile_gpexpand hostfile_gpinitsystem \
	hostfile_gpssh_allhosts hostfile_gpssh_segonly hostlist_singlenode

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(prefix)/docs/cli_help/gpconfigs'

install: installdirs
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(prefix)/docs/cli_help/gpconfigs/'$$file ; \
	done

uninstall:
	for file in $(DATA); do \
		rm -f '$(DESTDIR)$(prefix)/docs/cli_help/gpconfigs/'$$file ; \
	done


