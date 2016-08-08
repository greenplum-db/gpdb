MODULES = functions
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
override CFLAGS = $(CFLAGS_SL)
override CPPFLAGS=-D_GNU_SOURCE
plat = $(shell uname -s)
arch = $(shell arch)
ifeq "$(plat)" "Darwin"
    ifeq "$(arch)" "i386"
    override CFLAGS += $(CFLAGS_SL) -m32
    endif
endif

include $(PGXS)
