#
# Rules for backend mock test programs.
#
# We have a mechanism where every test program is automatically linked with
# mock versions of every backend file, except for those listed in
# <testname>_REAL_OBJS variable.

include $(top_srcdir)/src/Makefile.mock

override CPPFLAGS+= -I$(top_srcdir)/src/backend/libpq \
					-I$(top_srcdir)/src/backend/gp_libpq_fe \
					-I$(top_srcdir)/src/backend/postmaster \
					-I. -I$(top_builddir)/src/port \
					-DDLSUFFIX=$(DLSUFFIX) \
					-I$(top_srcdir)/src/backend/utils/stat

# TODO: add ldl for quick hack; we need to figure out why
# postgres in src/backend/Makefile doesn't need this and -pthread.
MOCK_LIBS := -ldl $(filter-out -lpgport -ledit, $(LIBS))

# This is a basic set of backend object files that every test program is linked
# with by default.
BACKEND_REAL_OBJS=\
	src/backend/access/common/heaptuple.o \
	src/backend/access/heap/tuptoaster.o \
	src/backend/access/hash/hashfunc.o \
	src/backend/access/transam/filerepdefs.o \
	src/backend/access/transam/transam.o \
	src/backend/cdb/cdbgang.o \
	src/backend/lib/stringinfo.o \
	src/backend/nodes/bitmapset.o \
	src/backend/nodes/equalfuncs.o \
	src/backend/nodes/list.o \
	src/backend/nodes/makefuncs.o \
	src/backend/nodes/value.o \
	src/backend/storage/file/compress_nothing.o \
	src/backend/storage/page/itemptr.o \
	src/backend/utils/adt/arrayfuncs.o \
	src/backend/utils/adt/arrayutils.o \
	src/backend/utils/adt/datetime.o \
	src/backend/utils/adt/datum.o \
	src/backend/utils/fmgr/fmgr.o \
	src/backend/utils/hash/hashfn.o \
	src/backend/utils/init/globals.o \
	src/backend/bootstrap/bootparse.o \
	src/backend/catalog/caql/catquery.o \
	src/backend/catalog/caql/gram.o \
	src/backend/catalog/core/catcore.o \
	src/backend/catalog/core/catcoretable.o \
	src/backend/parser/gram.o \
	src/backend/parser/kwlookup.o \
	src/backend/parser/parse_type.o \
	src/backend/parser/scansup.o \
	src/backend/regex/regcomp.o \
	src/backend/regex/regexec.o \
	src/backend/utils/adt/like.o \
	src/backend/utils/adt/varlena.o \
	src/backend/utils/misc/atomic.o \
	src/backend/utils/misc/bitstream.o \
	src/backend/utils/misc/guc.o \
	src/backend/utils/misc/guc_gp.o \
	src/backend/utils/misc/size.o \
	src/backend/utils/time/tqual.o \
	src/backend/replication/repl_gram.o \
	src/timezone/pgtz.o \
	src/timezone/localtime.o \
	src/timezone/strftime.o \
	src/port/libpgport_srv.a

# These files are not needed by any test program, so don't mock or link them.
EXCL_OBJS=\
	src/timezone/%.o \
	src/backend/main/%.o \
	src/backend/gpopt/%.o \
	src/backend/gpopt/config/%.o \
	src/backend/gpopt/relcache/%.o \
	src/backend/gpopt/translate/%.o \
	src/backend/gpopt/utils/%.o \

# These files cannot be mocked, because they #include header files or other
# C files that are not in the standard include path.
EXCL_OBJS+=\
	src/backend/bootstrap/bootparse.o \
	src/backend/parser/gram.o \
	src/backend/catalog/caql/gram.o \
	src/backend/nodes/outfast.o \
	src/backend/nodes/readfast.o \
	src/backend/replication/repl_gram.o \
	src/backend/regex/regcomp.o \
	src/backend/regex/regexec.o \
	src/backend/utils/adt/like.o \
	src/backend/utils/misc/guc.o \
	src/backend/utils/misc/guc_gp.o

# Create a _mock.c version of every backend object file that's not listed in
# EXCL_OBJS
OBJFILES=$(top_srcdir)/src/backend/objfiles.txt
MOCK_OBJS=$(addprefix $(top_srcdir)/, \
			$(filter-out $(EXCL_OBJS), \
				$(shell test -f $(OBJFILES) && cat $(OBJFILES))))

SUT_OBJ=$(top_srcdir)/$(subdir)/$(1).o

# A function that generates TARGET_OBJS = test case, mock objects, real objects,
# and cmockery.o.
TARGET_OBJS=$(1)_test.o $($(1)_OBJS) $(CMOCKERY_OBJS)

# Add the non-mocked versions of requested files.
TARGET_OBJS+=$($(1)_REAL_OBJS)

# Add the standard set of backend .o files that have not been explicitly
# mocked
TARGET_OBJS+=$(filter-out $(SUT_OBJ) $($(1)_MOCK_OBJS), $(addprefix $(top_srcdir)/, $(BACKEND_REAL_OBJS)))

# Add mock versions of other backend .o files
#
# $(OBJFILES) contains %/objfiles.txt, because src/backend/Makefile will
# create it with rule=objfiles.txt, which is not expected in postgres rule.
# It actually uses expand_subsys to obtain the .o file list.  But here we
# don't include common.mk so just clear out objfiles.txt from the list for
# $(TARGET_OBJS)
TARGET_OBJS+=$(patsubst $(top_srcdir)/src/%.o,$(MOCK_DIR)/%_mock.o,\
			$($(1)_MOCK_OBJS) \
			$(filter-out $(SUT_OBJ) $(addprefix $(top_srcdir)/, $(BACKEND_REAL_OBJS)) $($(1)_REAL_OBJS) %/objfiles.txt, \
				$(MOCK_OBJS)))

# The test target depends on $(OBJFILES) which would update files including mocks.
%.t: $(OBJFILES) $(CMOCKERY_OBJS) %_test.o mockup-phony
	$(CC) $(CFLAGS) $(LDFLAGS) $(call TARGET_OBJS,$*) $(MOCK_LIBS) -o $@

# We'd like to call only src/backend, but it seems we should build src/port and
# src/timezone before src/backend.  This is not the case when main build has finished,
# but this makes sure a simple make works fine in this directory any time.
# With PARTIAL_LINKING it will generate objfiles.txt
$(OBJFILES): $(MOCK_OBJS)
	$(MAKE) -C $(top_srcdir)/src
	$(MAKE) PARTIAL_LINKING= -C $(top_srcdir)/src/backend objfiles.txt

# Need to separate make, as it's using the output of another make.
mockup-phony: $(OBJFILES)
	$(MAKE) mockup

.PHONY:
mockup: $(patsubst $(top_srcdir)/src/%.o,$(MOCK_DIR)/%_mock.o,$(MOCK_OBJS))
