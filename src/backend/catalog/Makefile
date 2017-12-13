#-------------------------------------------------------------------------
#
# Makefile for backend/catalog
#
# src/backend/catalog/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/catalog
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = catalog.o dependency.o heap.o index.o indexing.o namespace.o aclchk.o \
       objectaddress.o pg_aggregate.o pg_constraint.o pg_conversion.o pg_depend.o pg_enum.o \
       pg_inherits.o pg_largeobject.o pg_namespace.o pg_operator.o pg_proc.o \
       pg_shdepend.o pg_type.o storage.o toasting.o \
       pg_exttable.o pg_extprotocol.o \
       pg_proc_callback.o \
       aoseg.o aoblkdir.o gp_fastsequence.o gp_segment_config.o \
       pg_attribute_encoding.o pg_compression.o aovisimap.o \
       gp_global_sequence.o gp_persistent.o pg_appendonly.o \
       oid_dispatch.o aocatalog.o zstd_compression.o

BKIFILES = postgres.bki postgres.description postgres.shdescription

CATALOG_JSON:= $(addprefix $(top_srcdir)/gpMgmt/bin/gppylib/data/, $(addsuffix .json,$(GP_MAJORVERSION)))

include $(top_srcdir)/src/backend/common.mk

all: $(BKIFILES) $(CATALOG_JSON)

# Note: there are some undocumented dependencies on the ordering in which
# the catalog header files are assembled into postgres.bki.  In particular,
# indexing.h had better be last, and toasting.h just before it.
# And pg_proc_gp.h must be immediately after pg_proc.h

POSTGRES_BKI_SRCS := $(addprefix $(top_srcdir)/src/include/catalog/, \
	pg_proc.h pg_proc_gp.h \
	pg_type.h pg_attribute.h pg_class.h \
	pg_attrdef.h pg_constraint.h pg_inherits.h pg_index.h pg_operator.h \
	pg_opfamily.h pg_opclass.h pg_am.h pg_amop.h pg_amproc.h \
	pg_language.h pg_largeobject.h pg_aggregate.h pg_statistic.h \
	pg_rewrite.h pg_trigger.h pg_listener.h pg_description.h pg_cast.h \
	pg_enum.h pg_namespace.h pg_conversion.h pg_depend.h \
	pg_database.h pg_tablespace.h pg_pltemplate.h \
	pg_authid.h pg_auth_members.h pg_shdepend.h pg_shdescription.h pg_resqueue.h pg_resgroup.h\
	pg_ts_config.h pg_ts_config_map.h pg_ts_dict.h \
	pg_ts_parser.h pg_ts_template.h pg_extension.h \
	pg_foreign_data_wrapper.h pg_foreign_server.h pg_user_mapping.h \
	gp_configuration_history.h gp_id.h gp_policy.h gp_version.h \
	gp_segment_config.h \
	pg_exttable.h pg_appendonly.h aoseg.h \
	gp_fastsequence.h pg_extprotocol.h \
	pg_partition.h pg_partition_rule.h pg_filespace.h pg_filespace_entry.h \
	gp_global_sequence.h gp_persistent.h \
	pg_attribute_encoding.h \
	pg_auth_time_constraint.h \
	pg_compression.h \
	pg_proc_callback.h \
	pg_partition_encoding.h \
	pg_type_encoding.h \
	toasting.h indexing.h \
    )

pg_includes = $(sort -I$(top_srcdir)/src/include -I$(top_builddir)/src/include)

# see explanation in ../parser/Makefile
postgres.description: postgres.bki ;

postgres.shdescription: postgres.bki ;

# In Greenplum, we have added extra columns to some catalog tables. To avoid
# having to change every single DATA row in those header files, which would
# create massive merge conflicts when merging with upstream, the extra
# columns are marked with GPDB_COLUMN_DEFAULT() lines in the header files,
# which provides a default for every data row that's missing the column. That
# way, new rows can have all the columns, but there's no need to modify rows
# inherited from upstream.
#
# process_col_defaults.pl reads the catalog headers, and creates a modified
# source file (postgres_bki_srcs) that has all the defaults injected into
# the DATA rows. That is fed to genbki.sh, instead of the original headers.
postgres_bki_srcs: process_col_defaults.pl $(POSTGRES_BKI_SRCS)
	cat $(POSTGRES_BKI_SRCS) | $(PERL) process_col_defaults.pl > postgres_bki_srcs

$(CATALOG_JSON): process_foreign_keys.pl $(POSTGRES_BKI_SRCS) $(top_srcdir)/src/include/catalog/catversion.h
	cat $(POSTGRES_BKI_SRCS) $(top_srcdir)/src/include/catalog/catversion.h | $(PERL) process_foreign_keys.pl > $@

postgres.bki: genbki.sh postgres_bki_srcs $(top_srcdir)/src/include/pg_config_manual.h
	AWK='$(AWK)' $(SHELL) $< $(pg_includes) --set-version=$(PG_VERSION) -o postgres postgres_bki_srcs

.PHONY: install-data
install-data: $(BKIFILES) installdirs
	$(INSTALL_DATA) $(call vpathsearch,postgres.bki) '$(DESTDIR)$(datadir)/postgres.bki'
	$(INSTALL_DATA) $(call vpathsearch,postgres.description) '$(DESTDIR)$(datadir)/postgres.description'
	$(INSTALL_DATA) $(call vpathsearch,postgres.shdescription) '$(DESTDIR)$(datadir)/postgres.shdescription'
	$(INSTALL_DATA) $(srcdir)/system_views.sql '$(DESTDIR)$(datadir)/system_views.sql'
	$(INSTALL_DATA) $(srcdir)/information_schema.sql '$(DESTDIR)$(datadir)/information_schema.sql'
	$(INSTALL_DATA) $(call vpathsearch,cdb_util.sql) '$(DESTDIR)$(datadir)/cdb_util.sql'
	$(INSTALL_DATA) $(call vpathsearch,cdb_schema.sql) '$(DESTDIR)$(datadir)/cdb_init.d/cdb_schema.sql'
	$(INSTALL_DATA) $(srcdir)/sql_features.txt '$(DESTDIR)$(datadir)/sql_features.txt'
	$(INSTALL_DATA) $(call vpathsearch,gp_toolkit.sql) '$(DESTDIR)$(datadir)/cdb_init.d/gp_toolkit.sql'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(datadir)'

.PHONY: uninstall-data
uninstall-data:
	rm -f $(addprefix '$(DESTDIR)$(datadir)'/, $(BKIFILES) system_views.sql information_schema.sql cdb_init.d/cdb_schema.sql sql_features.txt)

# postgres_bki_srcs is in the distribution tarball, so it is not cleaned here.
clean:
	rm -f $(BKIFILES)

maintainer-clean: clean
	rm -f postgres_bki_srcs
