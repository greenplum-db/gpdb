subdir=src/bin/pg_dump/cdb
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdb_dump_util \
	cdb_bsa_util 

# Objects from backend, which don't need to be mocked but need to be linked.
cdb_dump_util_REAL_OBJS=\
	$(top_srcdir)/src/backend/access/hash/hashfunc.o \
	$(top_srcdir)/src/backend/access/transam/filerepdefs.o \
	$(top_srcdir)/src/backend/bootstrap/bootparse.o \
	$(top_srcdir)/src/backend/lib/stringinfo.o \
	$(top_srcdir)/src/backend/nodes/bitmapset.o \
	$(top_srcdir)/src/backend/nodes/equalfuncs.o \
	$(top_srcdir)/src/backend/nodes/list.o \
	$(top_srcdir)/src/backend/parser/gram.o \
	$(top_srcdir)/src/backend/regex/regcomp.o \
	$(top_srcdir)/src/backend/regex/regerror.o \
	$(top_srcdir)/src/backend/regex/regexec.o \
	$(top_srcdir)/src/backend/regex/regfree.o \
	$(top_srcdir)/src/backend/storage/page/itemptr.o \
	$(top_srcdir)/src/backend/utils/adt/datum.o \
	$(top_srcdir)/src/backend/utils/adt/like.o \
	$(top_srcdir)/src/backend/utils/hash/hashfn.o \
	$(top_srcdir)/src/backend/utils/init/globals.o \
	$(top_srcdir)/src/backend/utils/misc/guc.o \
	$(top_srcdir)/src/port/path.o \
	$(top_srcdir)/src/port/pgstrcasecmp.o \
	$(top_srcdir)/src/port/qsort.o \
	$(top_srcdir)/src/port/thread.o \
	$(top_srcdir)/src/timezone/localtime.o \
	$(top_srcdir)/src/timezone/pgtz.o 

cdb_bsa_util_REAL_OBJS=\
	$(top_srcdir)/src/backend/access/hash/hashfunc.o \
	$(top_srcdir)/src/backend/access/transam/filerepdefs.o \
	$(top_srcdir)/src/backend/bootstrap/bootparse.o \
	$(top_srcdir)/src/backend/lib/stringinfo.o \
	$(top_srcdir)/src/backend/nodes/bitmapset.o \
	$(top_srcdir)/src/backend/nodes/equalfuncs.o \
	$(top_srcdir)/src/backend/nodes/list.o \
	$(top_srcdir)/src/backend/parser/gram.o \
	$(top_srcdir)/src/backend/regex/regcomp.o \
	$(top_srcdir)/src/backend/regex/regerror.o \
	$(top_srcdir)/src/backend/regex/regexec.o \
	$(top_srcdir)/src/backend/regex/regfree.o \
	$(top_srcdir)/src/backend/storage/page/itemptr.o \
	$(top_srcdir)/src/backend/utils/adt/datum.o \
	$(top_srcdir)/src/backend/utils/adt/like.o \
	$(top_srcdir)/src/backend/utils/hash/hashfn.o \
	$(top_srcdir)/src/backend/utils/init/globals.o \
	$(top_srcdir)/src/backend/utils/misc/guc.o \
	$(top_srcdir)/src/port/path.o \
	$(top_srcdir)/src/port/pgstrcasecmp.o \
	$(top_srcdir)/src/port/qsort.o \
	$(top_srcdir)/src/port/thread.o \
	$(top_srcdir)/src/timezone/localtime.o \
	$(top_srcdir)/src/timezone/pgtz.o \
	$(top_srcdir)/src/bin/pg_dump/cdb/cdb_dump_util.o 

override CPPFLAGS+= -I$(top_srcdir)/src/interfaces/libpq -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/include -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/include/rsa_csp/lb

include $(top_builddir)/src/Makefile.mock

ifeq ($(enable_ddboost), yes)
override MOCK_LIBS += -lDDBoost 
endif

ifeq ($(enable_netbackup), yes)
override MOCK_LIBS += -L../../../../../gpAux/ext/rhel5_x86_64/Netbackup/nbu75/lib -lxbsa64 -lnbclientcST -lnbbasecST -lvxcPBXST
override CPPFLAGS += -I$(top_builddir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu75/include
endif
