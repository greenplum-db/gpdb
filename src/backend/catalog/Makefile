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
       objectaccess.o objectaddress.o pg_aggregate.o pg_collation.o \
       pg_constraint.o pg_conversion.o \
       pg_depend.o pg_enum.o pg_inherits.o pg_largeobject.o pg_namespace.o \
       pg_operator.o pg_proc.o pg_range.o pg_db_role_setting.o pg_shdepend.o \
       pg_type.o storage.o toasting.o

OBJS += pg_extprotocol.o \
       pg_proc_callback.o \
       aoseg.o aoblkdir.o gp_fastsequence.o gp_segment_config.o \
       pg_attribute_encoding.o pg_compression.o aovisimap.o \
       pg_appendonly.o \
       oid_dispatch.o aocatalog.o storage_tablespace.o storage_database.o \
       storage_tablespace_twophase.o storage_tablespace_xact.o


BKIFILES = postgres.bki postgres.description postgres.shdescription

CATALOG_JSON:= $(addprefix $(top_srcdir)/gpMgmt/bin/gppylib/data/, $(addsuffix .json,$(GP_MAJORVERSION)))

include $(top_srcdir)/src/backend/common.mk

all: $(BKIFILES) schemapg.h $(CATALOG_JSON) postgres.description postgres.shdescription

# Note: there are some undocumented dependencies on the ordering in which
# the catalog header files are assembled into postgres.bki.  In particular,
# indexing.h had better be last, and toasting.h just before it.

POSTGRES_BKI_SRCS = $(addprefix $(top_srcdir)/src/include/catalog/,\
	pg_type.h pg_attribute.h pg_class.h \
	pg_attrdef.h pg_constraint.h pg_inherits.h pg_index.h pg_operator.h \
	pg_opfamily.h pg_opclass.h pg_am.h pg_amop.h pg_amproc.h \
	pg_language.h pg_largeobject_metadata.h pg_largeobject.h pg_aggregate.h \
	pg_statistic.h pg_rewrite.h pg_trigger.h pg_event_trigger.h pg_description.h \
	pg_cast.h pg_enum.h pg_namespace.h pg_conversion.h pg_depend.h \
	pg_database.h pg_db_role_setting.h pg_tablespace.h pg_pltemplate.h \
	pg_authid.h pg_auth_members.h pg_shdepend.h pg_shdescription.h \
	pg_ts_config.h pg_ts_config_map.h pg_ts_dict.h \
	pg_ts_parser.h pg_ts_template.h pg_extension.h \
	pg_foreign_data_wrapper.h pg_foreign_server.h pg_user_mapping.h \
	pg_resqueue.h pg_resqueuecapability.h pg_resourcetype.h \
	pg_resgroup.h pg_resgroupcapability.h \
	gp_configuration_history.h gp_id.h gp_policy.h gp_version.h \
	gp_segment_config.h \
	pg_appendonly.h \
	gp_fastsequence.h pg_extprotocol.h \
	pg_partition.h pg_partition_rule.h \
	pg_attribute_encoding.h \
	pg_auth_time_constraint.h \
	pg_compression.h \
	pg_proc_callback.h \
	pg_partition_encoding.h \
	pg_type_encoding.h \
	pg_stat_last_operation.h pg_stat_last_shoperation.h \
	pg_foreign_table.h pg_policy.h pg_replication_origin.h \
	pg_default_acl.h pg_init_privs.h pg_seclabel.h pg_shseclabel.h \
	pg_collation.h pg_range.h pg_transform.h \
	toasting.h indexing.h \
    )

POSTGRES_BKI_SRCS := pg_proc_combined.h $(POSTGRES_BKI_SRCS)


# location of Catalog.pm
catalogdir = $(top_srcdir)/src/backend/catalog

# locations of headers that genbki.pl needs to read
pg_includes = -I$(top_srcdir)/src/include/catalog -I$(top_builddir)/src/include/catalog

# see notes in src/backend/parser/Makefile about multiple output files
postgres.description: postgres.bki
	touch $@

postgres.shdescription: postgres.bki
	touch $@

schemapg.h: postgres.bki
	touch $@

# In Greenplum, contents of the pg_proc catalog is in two files, pg_proc.h
# and pg_proc_gp.h. Combine them into a single file, so that genbki.pl can
# process it.
pg_proc_combined.h: $(top_srcdir)/src/include/catalog/pg_proc.h $(top_srcdir)/src/include/catalog/pg_proc_gp.h
	cat $^ > $@

catalog_json: $(CATALOG_JSON)

$(CATALOG_JSON): process_foreign_keys.pl $(POSTGRES_BKI_SRCS) $(top_srcdir)/src/include/catalog/catversion.h
	cat $(POSTGRES_BKI_SRCS) $(top_srcdir)/src/include/catalog/catversion.h | $(PERL) process_foreign_keys.pl > $@

# Technically, this should depend on Makefile.global, but then
# postgres.bki would need to be rebuilt after every configure run,
# even in distribution tarballs.  So this is cheating a bit, but it
# will achieve the goal of updating the version number when it
# changes.
postgres.bki: genbki.pl Catalog.pm $(POSTGRES_BKI_SRCS) $(top_srcdir)/configure $(top_srcdir)/src/include/catalog/duplicate_oids
	cd $(top_srcdir)/src/include/catalog && $(PERL) ./duplicate_oids
	$(PERL) -I $(catalogdir) $< $(pg_includes) --set-version=$(PG_MAJORVERSION) $(POSTGRES_BKI_SRCS)

.PHONY: install-data
install-data: $(BKIFILES) installdirs
	$(INSTALL_DATA) $(call vpathsearch,postgres.bki) '$(DESTDIR)$(datadir)/postgres.bki'
	$(INSTALL_DATA) $(call vpathsearch,postgres.description) '$(DESTDIR)$(datadir)/postgres.description'
	$(INSTALL_DATA) $(call vpathsearch,postgres.shdescription) '$(DESTDIR)$(datadir)/postgres.shdescription'
	$(INSTALL_DATA) $(srcdir)/system_views.sql '$(DESTDIR)$(datadir)/system_views.sql'
	$(INSTALL_DATA) $(srcdir)/information_schema.sql '$(DESTDIR)$(datadir)/information_schema.sql'
	$(INSTALL_DATA) $(call vpathsearch,cdb_schema.sql) '$(DESTDIR)$(datadir)/cdb_init.d/cdb_schema.sql'
	$(INSTALL_DATA) $(srcdir)/sql_features.txt '$(DESTDIR)$(datadir)/sql_features.txt'
	$(INSTALL_DATA) $(call vpathsearch,gp_toolkit.sql) '$(DESTDIR)$(datadir)/cdb_init.d/gp_toolkit.sql'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(datadir)'
	$(MKDIR_P) '$(DESTDIR)$(datadir)/cdb_init.d'

.PHONY: uninstall-data
uninstall-data:
	rm -f $(addprefix '$(DESTDIR)$(datadir)'/, $(BKIFILES) system_views.sql information_schema.sql cdb_init.d/cdb_schema.sql sql_features.txt)

# postgres.bki, postgres.description, postgres.shdescription, and schemapg.h
# are in the distribution tarball, so they are not cleaned here.
clean:
	rm -f pg_proc_combined.h

maintainer-clean: clean
	rm -f $(BKIFILES)
