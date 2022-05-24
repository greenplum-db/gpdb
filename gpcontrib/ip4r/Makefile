NO_PGXS := 1

MODULE_big = ip4r

ifndef NO_EXTENSION
EXTENSION= ip4r
SRC_SQL	= ip4r--2.4.sql \
	  ip4r--2.2--2.4.sql \
	  ip4r--2.1--2.2.sql \
	  ip4r--2.0--2.1.sql \
	  ip4r--unpackaged2.1--2.1.sql \
	  ip4r--unpackaged2.0--2.0.sql \
	  ip4r--unpackaged1--2.0.sql
DATA	= $(addprefix scripts/, $(SRC_SQL))
REGRESS = ip4r $(REGRESS_$(MAJORVERSION)) ip4r-explain
REGRESS_11 := ip4r-v11
REGRESS_12 := $(REGRESS_11)
else
DATA_built = ip4r.sql
EXTRA_CLEAN += ip4r.sql.in sql/ip4r-legacy.sql expected/ip4r-legacy.out
REGRESS = ip4r-legacy
endif

objdir	= src

DOCS	= README.ip4r
OBJS_C	= ip4r_module.o ip4r.o ip6r.o ipaddr.o iprange.o raw_io.o
OBJS	= $(addprefix src/, $(OBJS_C))
INCS	= ipr.h ipr_internal.h

HEADERS = src/ipr.h

# if VPATH is not already set, but the makefile is not in the current
# dir, then assume a vpath build using the makefile's directory as
# source. PGXS will set $(srcdir) accordingly.
ifndef VPATH
ifneq ($(realpath $(CURDIR)),$(realpath $(dir $(firstword $(MAKEFILE_LIST)))))
VPATH := $(dir $(firstword $(MAKEFILE_LIST)))
endif
endif

ifndef NO_PGXS
PG_CONFIG ?= pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/ip4r
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

ifeq ($(filter-out 7.% 8.0 8.1 8.2 8.3, $(MAJORVERSION)),)
$(error unsupported PostgreSQL version)
endif

$(OBJS): $(addprefix $(srcdir)/src/, $(INCS))

# for a vpath build, we need src/ to exist in the build dir before
# building any objects.
ifdef VPATH
all: vpath-mkdirs
.PHONY: vpath-mkdirs
$(OBJS): | vpath-mkdirs

vpath-mkdirs:
	$(MKDIR_P) $(objdir)
endif # VPATH

ifndef EXTENSION

ifeq ($(filter-out 8.4, $(MAJORVERSION)),)

ip4r.sql.in: $(srcdir)/scripts/ip4r--2.4.sql $(srcdir)/tools/legacy.sed
	sed -f $(srcdir)/tools/legacy.sed $< | sed -e '/^DO /,/^[$$]/d' >$@

else

ip4r.sql.in: $(srcdir)/scripts/ip4r--2.4.sql $(srcdir)/tools/legacy.sed
	sed -f $(srcdir)/tools/legacy.sed $< >$@

endif

# regression test doesn't like the idea of having to build files in
# the sql/ subdir, and looks for that only in $(srcdir). So disable
# legacy regression tests in vpath build.
ifndef VPATH
sql/ip4r-legacy.sql: sql/ip4r.sql tools/legacy-r.sed
	sed -f tools/legacy-r.sed $< >$@

expected/ip4r-legacy.out: expected/ip4r.out tools/legacy-r.sed
	sed -f tools/legacy-r.sed $< | sed -e '/^\\i /,+1d' >$@

installcheck: sql/ip4r-legacy.sql expected/ip4r-legacy.out
else
installcheck:
	@echo regression tests are disabled in legacy vpath build
endif # VPATH

else
ifeq ($(filter-out 8.% 9.0, $(MAJORVERSION)),)
$(error extension build not supported in versions before 9.1, use NO_EXTENSION=1)
endif
endif

