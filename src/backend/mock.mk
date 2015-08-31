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

# These files cannot be mocked, because they #include header files or other
# C files that are not in the standard include path.
NO_MOCK_OBJS+=\
	src/backend/libpq/auth.o \
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
	src/backend/utils/misc/guc_gp.o \
	src/timezone/localtime.o \
	src/timezone/pgtz.o \
	src/timezone/strftime.o

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

# $(OBJFILES) contains %/objfiles.txt, because src/backend/Makefile will
# create it with rule=objfiles.txt, which is not expected in postgres rule.
# It actually uses expand_subsys to obtain the .o file list.  But here we
# don't include common.mk so just clear out objfiles.txt from the list for
# $(TARGET_OBJS)
OBJFILES=$(top_srcdir)/src/backend/objfiles.txt
ALL_OBJS=$(addprefix $(top_srcdir)/, \
			$(filter-out $(EXCL_OBJS) %/objfiles.txt, \
				$(shell test -f $(OBJFILES) && cat $(OBJFILES))))

AUTO_MOCK_OBJS=$(filter-out $(NO_MOCK_OBJS) %/objfiles.txt %.a, \
	$(shell test -f $(OBJFILES) && cat $(OBJFILES)))

# A function that generates TARGET_OBJS = test case, mock objects, real objects,
# and cmockery.o.
TARGET_OBJS=$(1)_test.o $($(1)_OBJS) $(CMOCKERY_OBJS)

# Add mocked versions of all requested backend .o files.
TARGET_OBJS+=$(patsubst $(top_srcdir)/src/%.o,$(MOCK_DIR)/%_mock.o,\
			$($(1)_MOCK_OBJS) )

# Add all backend .o files that have not been explicitly mocked. Also leave
# out the file we're testing; the test .c file is expected to #include the
# real file. (That allows the test program to call static functions in the
# SUT)
SUT_OBJ=$(top_srcdir)/$(subdir)/$(1).o
TARGET_OBJS+=$(filter-out $(SUT_OBJ) $($(1)_MOCK_OBJS), $(ALL_OBJS))

# Add the common mock objects
TARGET_OBJS+=$(MOCK_OBJS)

# The test target depends on $(OBJFILES) which would update files including mocks.
%.t: $(OBJFILES) $(CMOCKERY_OBJS) %_test.o mockup-phony
	$(CC) $(CFLAGS) $(LDFLAGS) $(call TARGET_OBJS,$*) $(MOCK_LIBS) -o $@

# We'd like to call only src/backend, but it seems we should build src/port and
# src/timezone before src/backend.  This is not the case when main build has finished,
# but this makes sure a simple make works fine in this directory any time.
# With PARTIAL_LINKING it will generate objfiles.txt
$(OBJFILES): $(ALL_OBJS)
	$(MAKE) -C $(top_srcdir)/src
	$(MAKE) PARTIAL_LINKING= -C $(top_srcdir)/src/backend objfiles.txt

# Need to separate make, as it's using the output of another make.
mockup-phony: $(OBJFILES)
	$(MAKE) mockup

.PHONY:
mockup: $(patsubst src/%.o,$(MOCK_DIR)/%_mock.o,$(AUTO_MOCK_OBJS)) $(MOCK_OBJS)
