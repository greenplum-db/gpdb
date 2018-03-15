# $PostgreSQL: pgsql/contrib/Makefile,v 1.93 2010/06/14 16:17:56 sriggs Exp $

subdir = contrib
top_builddir = ..
include $(top_builddir)/src/Makefile.global

SUBDIRS = \
		auto_explain	\
		btree_gin	\
		citext		\
		dblink		\
		dict_int	\
		dict_xsyn	\
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
		pg_trgm		\
		pg_upgrade	\
		pg_upgrade_support \
		pgbench		\
		pgcrypto	\
		test_parser

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
		indexscan \

ifeq ($(with_openssl),yes)
SUBDIRS += sslinfo
endif

ifeq ($(with_ossp_uuid),yes)
SUBDIRS += uuid-ossp
endif

ifeq ($(with_libxml),yes)
SUBDIRS += xml2
endif

ifeq ($(enable_orca),yes)
SUBDIRS += orca_debug
endif

SUBDIRS += gp_replica_check

# Missing:
#		start-scripts	\ (does not have a makefile)


all install installdirs uninstall distprep clean distclean maintainer-clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@ || exit; \
	done

# We'd like check operations to run all the subtests before failing.
check installcheck:
	@CHECKERR=0; for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@ || CHECKERR=$$?; \
	done; \
	exit $$CHECKERR
