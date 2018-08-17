# contrib/Makefile

subdir = contrib
top_builddir = ..
include $(top_builddir)/src/Makefile.global

SUBDIRS = \
		auth_delay	\
		auto_explain	\
		btree_gin	\
		citext		\
		dblink		\
		dict_int	\
		dict_xsyn	\
		dummy_seclabel	\
		file_fdw	\
		fuzzystrmatch	\
		hstore		\
		intarray	\
		pageinspect	\
		passwordcheck	\
		pg_archivecleanup \
		pg_buffercache	\
		pg_freespacemap \
		pg_standby	\
		pg_stat_statements \
		pg_test_fsync	\
		pg_test_timing	\
		pg_trgm		\
		pg_upgrade	\
		pg_upgrade_support \
		pgbench		\
		pgcrypto	\
		pgrowlocks	\
		pgstattuple	\
		spi		\
		tcn		\
		test_parser	\
		tsearch2	\
		unaccent


# GPDB_92_MERGE_FIXME: Why some files (especially Makefile) are missing under
# these directories?
#       seg         \
		tablefunc	\
		vacuumlo

# Greenplum-specific additions (to ease merge pain).
SUBDIRS += \
		xlogdump \
		formatter \
		formatter_fixedwidth \
		extprotocol \
		gp_distribution_policy \
		gp_internal_tools \
		gp_inject_fault \
		gp_cancel_query \
		gp_sparse_vector \
		indexscan \

ifeq ($(with_openssl),yes)
SUBDIRS += sslinfo
else
ALWAYS_SUBDIRS += sslinfo
endif

ifeq ($(with_ossp_uuid),yes)
SUBDIRS += uuid-ossp
else
ALWAYS_SUBDIRS += uuid-ossp
endif

ifeq ($(with_libxml),yes)
SUBDIRS += xml2
else
ALWAYS_SUBDIRS += xml2
endif

ifeq ($(with_selinux),yes)
SUBDIRS += sepgsql
else
ALWAYS_SUBDIRS += sepgsql
endif

ifeq ($(enable_orca),yes)
SUBDIRS += orca_debug
endif

SUBDIRS += gp_replica_check

# Missing:
#		start-scripts	\ (does not have a makefile)


$(recurse)
$(recurse_always)
