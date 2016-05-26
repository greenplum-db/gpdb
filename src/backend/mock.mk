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
MOCK_LIBS := -ldl $(filter-out -lpgport -ledit, $(LIBS)) $(LDAP_LIBS_BE)

# These files are not linked into test programs.
EXCL_OBJS=\
	src/backend/main/main.o \
	src/backend/gpopt/%.o \
	src/backend/gpopt/config/%.o \
	src/backend/gpopt/relcache/%.o \
	src/backend/gpopt/translate/%.o \
	src/backend/gpopt/utils/%.o

# These files are linked into every test program.
MOCK_OBJS=\
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
