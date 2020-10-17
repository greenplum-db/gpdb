# Include
include ../../include/makefile.inc

# Options
DEBUG_S3_SYMBOL = y

# Flags
PG_LIBS += $(COMMON_LINK_OPTIONS)
PG_CPPFLAGS += $(COMMON_CPP_FLAGS) -I../../include -I../../lib -I$(libpq_srcdir) -I$(libpq_srcdir)/postgresql/server/utils -DS3_STANDALONE -DS3_STANDALONE_CHECKCLOUD

ifeq ($(DEBUG_S3_SYMBOL),y)
	PG_CPPFLAGS += -g
endif

# Targets
PROGRAM = gpcheckcloud
OBJS = gpcheckcloud.o ../../lib/http_parser.o ../../lib/ini.o $(COMMON_OBJS)

# Launch
ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

# Don't bother with building LLVM bytecode for this client tool.
# It would be a waste of time, but also, it doesn't work, because
# of the way we build the source files from another directory into
# .o files in this directory.
with_llvm = no

include $(top_srcdir)/contrib/contrib-global.mk
endif

%.o: ../../src/%.cpp
	@# CPPFLAGS := $(PG_CPPFLAGS) $(CPPFLAGS)
	$(CXX) -c $(CPPFLAGS) $< -o $@
