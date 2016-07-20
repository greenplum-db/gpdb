subdir=src/bin/pg_dump/cdb
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdb_dump_util cdb_bsa_util cdb_lockbox

override CPPFLAGS+= -I$(top_srcdir)/src/interfaces/libpq -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/include

include $(top_builddir)/src/Makefile.mock

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto, $(LIBS))

ifeq ($(enable_ddboost), yes)
DDBOOSTLIB += -lDDBoost
endif

ifeq ($(enable_netbackup), yes)
NETBACKUPLIB75 += -L$(top_srcdir)/gpAux/ext/rhel5_x86_64/Netbackup/nbu75/lib -lxbsa64 -lnbclientcST -lnbbasecST -lvxcPBXST
NETBACKUPLIB71 += -L$(top_srcdir)/gpAux/ext/rhel5_x86_64/Netbackup/nbu71/lib -lxbsa64 -lnbclientcST -lnbbasecST -lvxcPBXST
GPBSALIB += -L$(top_builddir)/src/bin/pg_dump/cdb -lgpbsa -Wl,-rpath-link,'$(top_srcdir)/gpAux/ext/rhel5_x86_64/Netbackup/nbu75/lib'
override CFLAGS += -I$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu75/include
endif


# The command line here should resemble the one used to build cdb_dump_agent
cdb_dump_util.t: cdb_dump_util_test.o ../../dumputils.o ../cdb_lockbox.o ../../keywords.o ../../kwlookup.o $(CMOCKERY_OBJS)
	$(CC) $^ $(libpq) $(LDFLAGS) $(LIBS) $(DDBOOSTLIB) -o $@

# The command line here should resemble the one used to build libgpbsa
cdb_bsa_util.t: cdb_bsa_util_test.o ../cdb_dump_util.o ../cdb_lockbox.o ../../dumputils.o ../../keywords.o ../../kwlookup.o $(CMOCKERY_OBJS)
	$(CC) $^ $(libpq) $(LDFLAGS) $(LIBS) $(DDBOOSTLIB) $(NETBACKUPLIB75) -o $@

# The command line here should resemble the one used to build libgpbsa
cdb_lockbox.t: cdb_lockbox_test.o ../cdb_dump_util.o ../../dumputils.o ../../keywords.o ../../kwlookup.o $(CMOCKERY_OBJS)
	$(CC) $^ $(libpq) $(LDFLAGS) $(LIBS) $(DDBOOSTLIB) $(NETBACKUPLIB75) -o $@
