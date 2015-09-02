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

# We are unable to mock them up at the moment.  Real objects should contain them.
EXCL_OBJS=src/backend/bootstrap/bootparse.o \
			src/backend/catalog/caql/gram.o \
			src/backend/parser/gram.o \
			src/backend/nodes/readfast.o \
			src/backend/nodes/outfast.o \
			src/backend/regex/regcomp.o \
			src/backend/regex/regexec.o \
			src/backend/utils/adt/like.o \
			src/backend/utils/misc/guc.o \
			src/backend/utils/misc/guc_gp.o \
			src/backend/replication/repl_gram.o \
			src/timezone/%.o \
			src/backend/main/%.o \
			src/backend/gpopt/%.o \
			src/backend/gpopt/config/%.o \
			src/backend/gpopt/relcache/%.o \
			src/backend/gpopt/translate/%.o \
			src/backend/gpopt/utils/%.o

OBJFILES=$(top_srcdir)/src/backend/objfiles.txt
ALL_OBJS=$(addprefix $(top_srcdir)/, \
			$(filter-out $(EXCL_OBJS), \
				$(shell test -f $(OBJFILES) && cat $(OBJFILES))))

SUT_OBJ=$(top_srcdir)/$(subdir)/$(1).o
# A function that generates TARGET_OBJS = test case, mock objects, real objects,
# and cmockery.o. $(OBJFILES) contains %/objfiles.txt, because
# src/backend/Makefile will create it with rule=objfiles.txt, which is not
# expected in postgres rule.  It actually uses expand_subsys to obtain the .o
# file list.  But here we don't include common.mk so just clear out
# objfiles.txt from the list for $(TARGET_OBJS)
TARGET_OBJS=$(shell echo $(1)_test.o $(CMOCKERY_OBJS) $($(1)_REAL_OBJS) \
		$(patsubst $(top_srcdir)/src/%.o,$(MOCK_DIR)/%_mock.o,\
			$(filter-out $(SUT_OBJ) $($(1)_REAL_OBJS) %/objfiles.txt, \
				$(ALL_OBJS))))

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
mockup: $(patsubst $(top_srcdir)/src/%.o,$(MOCK_DIR)/%_mock.o,$(ALL_OBJS))
