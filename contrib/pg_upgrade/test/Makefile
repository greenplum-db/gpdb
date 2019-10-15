subdir=contrib/pg_upgrade
top_builddir=../../..
include $(top_builddir)/src/Makefile.global

TARGETS=check_gp

override CPPFLAGS+= -I$(top_srcdir)/src/interfaces/libpq

include $(top_builddir)/src/Makefile.mock

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto, $(LIBS))

check_gp.t: \
	mock_util.o \
	mock_server.o \
	check_gp_test.o \
	$(MOCK_DIR)/backend/libpq/fe-exec_mock.o \
	$(CMOCKERY_OBJS) $(filter-out ../util.o ../server.o ../pg_upgrade.o, $(wildcard ../*.o))
	$(CC) $^ $(libpq_pgport) $(LDFLAGS) $(LIBS) $(libpq) -o $@
