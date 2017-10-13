# $PostgreSQL: pgsql/contrib/Makefile,v 1.82.2.1 2008/05/08 16:49:47 tgl Exp $

subdir = contrib
top_builddir = ..
include $(top_builddir)/src/Makefile.global

WANTED_DIRS = \
		xlogdump \
		pgbench \
		changetrackingdump \
		formatter \
		formatter_fixedwidth \
		extprotocol \
		gp_distribution_policy \
		gp_internal_tools \
		gp_inject_fault \
		gp_cancel_query \
		indexscan \
		dict_int \
		dict_xsyn \
		hstore		\
		intarray \
		pageinspect \
		passwordcheck \
		pg_upgrade \
		pg_upgrade_support \
		test_parser \
		pgcrypto

ifeq ($(with_openssl),yes)
WANTED_DIRS += sslinfo
endif

ifeq ($(with_ossp_uuid),yes)
WANTED_DIRS += uuid-ossp
endif

ifeq ($(with_libxml),yes)
WANTED_DIRS += xml2
endif

ifeq ($(enable_orca),yes)
WANTED_DIRS += orca_debug
endif

ifeq ($(enable_segwalrep),yes)
WANTED_DIRS += gp_replica_check
endif

# Missing:
#		start-scripts	\ (does not have a makefile)


all install installdirs uninstall distprep clean distclean maintainer-clean:
	@for dir in $(WANTED_DIRS); do \
		$(MAKE) -C $$dir $@ || exit; \
	done

# We'd like check operations to run all the subtests before failing.
check installcheck:
	@CHECKERR=0; for dir in $(WANTED_DIRS); do \
		$(MAKE) -C $$dir $@ || CHECKERR=$$?; \
	done; \
	exit $$CHECKERR
