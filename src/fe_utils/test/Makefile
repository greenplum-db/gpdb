subdir=src/fe_utils
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

TARGETS=print
PG_LIBS=-lpgcommon -lpgport -lpq -lm

include $(top_srcdir)/src/backend/mock.mk

# This test case doesn't follow all UT guides, because current the infrastructure of UT
# is for backend test only. Using it leads to many weird link errors, let's keep it
# simple here.
print.t: print_test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--wrap=exit $< ../mbprint.o $(CMOCKERY_OBJS) $(PG_LIBS) -o print.t
