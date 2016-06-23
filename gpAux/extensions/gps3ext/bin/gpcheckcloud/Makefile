# Include
include ../../include/makefile.inc

# Options
DEBUG_S3_SYMBOL = y

# Flags
PG_LIBS += $(COMMON_LINK_OPTIONS)
PG_CPPFLAGS += $(COMMON_CPP_FLAGS) -I../../include -I../../lib -DS3_CHK_CFG -DS3_STANDALONE

ifeq ($(DEBUG_S3_SYMBOL),y)
	PG_CPPFLAGS += -g
endif

# Targets
PROGRAM = gpcheckcloud
OBJS = gpcheckcloud.o ../../lib/http_parser.o ../../lib/ini.o $(addprefix ../../,$(COMMON_OBJS))

# Launch
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
