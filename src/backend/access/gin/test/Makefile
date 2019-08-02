# See README.md for notes on this Makefile

top_builddir = ../../../../..
subdir = src/backend/access/gin

include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.mock


TARGETS += ginpostinglist


.PHONY:
clean:
	@rm -f *.o *.t

.PHONY:
all: check


ginpostinglist.t: $(CMOCKERY_OBJS) $(top_srcdir)/$(subdir)/ginpostinglist.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) \
		$(CMOCKERY_OBJS) \
		$(top_srcdir)/$(subdir)/ginpostinglist.o \
		ginpostinglist_fakes.c \
		ginpostinglist_test.c \
		-o ginpostinglist.t


.PHONY:
check: ginpostinglist.t
	./ginpostinglist.t
