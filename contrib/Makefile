# contrib/Makefile

subdir = contrib
top_builddir = ..
include $(top_builddir)/src/Makefile.global

# Greenplum specific changes to the targetlist:
# tsearch2 and postgres_fdw are currently disabled due to test failures.
# cube and the related earthdistance are disabled since GPDB define CUBE
# as a keyword

SUBDIRS = \
		adminpack	\
		auth_delay	\
		auto_explain	\
		btree_gin	\
		btree_gist	\
		chkpass		\
		citext		\
		dblink		\
		dict_int	\
		dict_xsyn	\
		dummy_seclabel	\
		file_fdw	\
		fuzzystrmatch	\
		hstore		\
		intagg		\
		intarray	\
		isn		\
		lo		\
		ltree		\
		oid2name	\
		pageinspect	\
		passwordcheck	\
		pg_archivecleanup \
		pg_buffercache	\
		pg_freespacemap \
		pg_prewarm	\
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
		pg_xlogdump	\
		seg		\
		spi		\
		tablefunc	\
		tcn		\
		test_decoding	\
		test_parser	\
		test_shm_mq	\
		unaccent	\
		vacuumlo	\
		worker_spi


# Greenplum-specific additions (to ease merge pain).
SUBDIRS += \
		formatter \
		formatter_fixedwidth \
		extprotocol \
		indexscan \

ifeq ($(with_openssl),yes)
SUBDIRS += sslinfo
else
ALWAYS_SUBDIRS += sslinfo
endif

ifneq ($(with_uuid),no)
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

# Missing:
#		start-scripts	\ (does not have a makefile)


$(recurse)
$(recurse_always)
