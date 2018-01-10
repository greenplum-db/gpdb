#
# Rules for backend mock test programs.
#
# We have a mechanism where every test program is automatically linked with
# mock versions of every backend file, except for those listed in
# <testname>_REAL_OBJS variable.

include $(top_srcdir)/src/Makefile.mock

override CPPFLAGS+= -I$(top_srcdir)/src/backend/libpq \
					-I$(libpq_srcdir) \
					-I$(top_srcdir)/src/backend/postmaster \
					-I. -I$(top_builddir)/src/port \
					-DDLSUFFIX=$(DLSUFFIX) \
					-I$(top_srcdir)/src/backend/utils/stat

# TODO: add ldl for quick hack; we need to figure out why
# postgres in src/backend/Makefile doesn't need this and -pthread.
MOCK_LIBS := -ldl $(filter-out -ledit, $(LIBS)) $(LDAP_LIBS_BE)

# These files are not linked into test programs.
EXCL_OBJS=\
	src/backend/main/main.o \
	src/backend/access/transam/rmgr.o \
	src/backend/utils/fmgrtab.o \
	src/backend/gpopt/%.o \
	src/backend/gpopt/config/%.o \
	src/backend/gpopt/relcache/%.o \
	src/backend/gpopt/translate/%.o \
	src/backend/gpopt/utils/%.o \

# More files that are not linked into test programs. There's no particular
# reason these couldn't be linked into, if necessary, but currently none of
# the tests need these, so better to leave them out to cut down on the size
# of the test programs. Feel free to link them back (i.e. remove them from
# this exclusion list) as needed.
EXCL_OBJS+=\
	src/backend/access/gist/%.o \
	src/backend/access/gin/%.o \
	src/backend/access/hash/hash.o \
	src/backend/access/hash/hashsearch.o \
	\
	src/backend/utils/adt/ascii.o \
	src/backend/utils/adt/cash.o \
	src/backend/utils/adt/char.o \
	src/backend/utils/adt/complex_type.o \
	src/backend/utils/adt/domains.o \
	src/backend/utils/adt/enum.o \
	src/backend/utils/adt/geo_ops.o \
	src/backend/utils/adt/geo_selfuncs.o \
	src/backend/utils/adt/gp_dump_oids.o \
	src/backend/utils/adt/gp_optimizer_functions.o \
	src/backend/utils/adt/interpolate.o \
	src/backend/utils/adt/json.o \
	src/backend/utils/adt/jsonfuncs.o \
	src/backend/utils/adt/like.o \
	src/backend/utils/adt/like_match.o \
	src/backend/utils/adt/lockfuncs.o \
	src/backend/utils/adt/mac.o \
	src/backend/utils/adt/matrix.o \
	src/backend/utils/adt/oracle_compat.o \
	src/backend/utils/adt/pgstatfuncs.o \
	src/backend/utils/adt/pivot.o \
	src/backend/utils/adt/pseudotypes.o \
	src/backend/utils/adt/quote.o \
	src/backend/utils/adt/rowtypes.o \
	src/backend/utils/adt/tsginidx.o \
	src/backend/utils/adt/tsgistidx.o \
	src/backend/utils/adt/tsquery.o \
	src/backend/utils/adt/tsquery_cleanup.o \
	src/backend/utils/adt/tsquery_gist.o \
	src/backend/utils/adt/tsquery_op.o \
	src/backend/utils/adt/tsquery_rewrite.o \
	src/backend/utils/adt/tsquery_util.o \
	src/backend/utils/adt/tsrank.o \
	src/backend/utils/adt/tsvector.o \
	src/backend/utils/adt/tsvector_op.o \
	src/backend/utils/adt/tsvector_parser.o \
	src/backend/utils/adt/txid.o \
	src/backend/utils/adt/uuid.o \
	src/backend/utils/adt/xid.o \
	src/backend/tsearch/dict.o \
	src/backend/tsearch/dict_ispell.o \
	src/backend/tsearch/dict_simple.o \
	src/backend/tsearch/dict_synonym.o \
	src/backend/tsearch/dict_thesaurus.o \
	src/backend/tsearch/regis.o \
	src/backend/tsearch/spell.o \
	src/backend/tsearch/to_tsany.o \
	src/backend/tsearch/ts_locale.o \
	src/backend/tsearch/ts_parse.o \
	src/backend/tsearch/ts_utils.o \
	src/backend/tsearch/wparser.o \
	src/backend/tsearch/wparser_def.o \

# These files are linked into every test program.
MOCK_OBJS=\
	$(top_srcdir)/src/test/unit/mock/fmgrtab_mock.o \
	$(top_srcdir)/src/test/unit/mock/rmgr_mock.o \
	$(top_srcdir)/src/test/unit/mock/main_mock.o
# No test programs currently exercise the ORCA translator library, so
# mock that instead of linking with the real library.
ifeq ($(enable_orca),yes)
MOCK_OBJS+=$(top_srcdir)/src/test/unit/mock/gpopt_mock.o
endif

# No test programs in GPDB currently exercise codegen, so
# mock that instead of linking with the real library.
ifeq ($(enable_codegen),yes)
MOCK_OBJS+=$(top_srcdir)/src/test/unit/mock/gpcodegen_mock.o
endif

# $(OBJFILES) contains %/objfiles.txt, because src/backend/Makefile will
# create it with rule=objfiles.txt, which is not expected in postgres rule.
# It actually uses expand_subsys to obtain the .o file list.  But here we
# don't include common.mk so just clear out objfiles.txt from the list for
# $(TARGET_OBJS)
OBJFILES=$(top_srcdir)/src/backend/objfiles.txt
ALL_OBJS=$(addprefix $(top_srcdir)/, \
			$(filter-out $(EXCL_OBJS) %/objfiles.txt, \
				$(shell test -f $(OBJFILES) && cat $(OBJFILES))))

# A function that generates a list of backend .o files that should be included
# in a test program.
#
# The argument is a list of backend object files that should *not* be included
BACKEND_OBJS=$(filter-out $(1), $(ALL_OBJS))

# If we are using linker's wrap feature in unit test, add wrap flags for
# those mocked functions
WRAP_FLAGS=-Wl,--wrap=
WRAP_SED_REGEXP='s/.*__wrap_\(\w*\)(.*/\1/p'
WRAP_FUNCS=$(addprefix $(WRAP_FLAGS), \
			$(sort $(shell sed -n $(WRAP_SED_REGEXP) $(1))))

# The test target depends on $(OBJFILES) which would update files including mocks.
%.t: $(OBJFILES) $(CMOCKERY_OBJS) $(MOCK_OBJS) %_test.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(call WRAP_FUNCS, $(top_srcdir)/$(subdir)/test/$*_test.c) $(call BACKEND_OBJS, $(top_srcdir)/$(subdir)/$*.o $(patsubst $(MOCK_DIR)/%_mock.o,$(top_builddir)/src/%.o, $^)) $(filter-out %/objfiles.txt, $^) $(MOCK_LIBS) -o $@

# We'd like to call only src/backend, but it seems we should build src/port and
# src/timezone before src/backend.  This is not the case when main build has finished,
# but this makes sure a simple make works fine in this directory any time.
# With PARTIAL_LINKING it will generate objfiles.txt
$(OBJFILES): $(ALL_OBJS)
	$(MAKE) -C $(top_srcdir)/src
	$(MAKE) PARTIAL_LINKING= -C $(top_srcdir)/src/backend objfiles.txt
