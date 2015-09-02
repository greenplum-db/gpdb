subdir=src/bin/pg_dump/cdb
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdb_dump_util cdb_bsa_util

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
	$(top_srcdir)/src/backend/utils/misc/guc_gp.o \
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
	$(top_srcdir)/src/backend/utils/misc/guc_gp.o \
	$(top_srcdir)/src/port/path.o \
	$(top_srcdir)/src/port/pgstrcasecmp.o \
	$(top_srcdir)/src/port/qsort.o \
	$(top_srcdir)/src/port/thread.o \
	$(top_srcdir)/src/timezone/localtime.o \
	$(top_srcdir)/src/timezone/pgtz.o \
	$(top_srcdir)/src/bin/pg_dump/cdb/cdb_dump_util.o 

override CPPFLAGS+= -I$(top_srcdir)/src/interfaces/libpq -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/include -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/include/rsa_csp/lb

include $(top_builddir)/src/Makefile.mock

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto, $(LIBS))

ifeq ($(enable_ddboost), yes)
DDBOOSTLIB += -lDDBoost
override CFLAGS += -I$(top_builddir)/gpAux/ext/$(BLD_ARCH)/include/rsa_csp -I$(top_builddir)/gpAux/ext/$(BLD_ARCH)/include/rsa_csp/lb
endif

ifeq ($(enable_netbackup), yes)
NETBACKUPLIB75 += -L$(top_srcdir)/gpAux/ext/rhel5_x86_64/Netbackup/nbu75/lib -lxbsa64 -lnbclientcST -lnbbasecST -lvxcPBXST
NETBACKUPLIB71 += -L$(top_srcdir)/gpAux/ext/rhel5_x86_64/Netbackup/nbu71/lib -lxbsa64 -lnbclientcST -lnbbasecST -lvxcPBXST
GPBSALIB += -L$(top_builddir)/src/bin/pg_dump/cdb -lgpbsa -Wl,-rpath-link,'$(top_srcdir)/gpAux/ext/rhel5_x86_64/Netbackup/nbu75/lib'
override CFLAGS += -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu75/include
endif


# The command line here should resemble the one used to build cdb_dump_agent
cdb_dump_util.t: cdb_dump_util_test.o $(CMOCKERY_OBJS)
	$(CC) $^ $(libpq) $(LDFLAGS) $(LIBS) $(DDBOOSTLIB) -o $@

# The command line here should resemble the one used to build libgpbsa
cdb_bsa_util.t: cdb_bsa_util_test.o ../cdb_dump_util.o  $(CMOCKERY_OBJS)
	$(CC) $^ $(libpq) $(LDFLAGS) $(LIBS) $(DDBOOSTLIB) $(NETBACKUPLIB75) -o $@
