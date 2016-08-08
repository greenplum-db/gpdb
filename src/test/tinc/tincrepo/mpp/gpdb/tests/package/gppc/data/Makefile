MODULE_big = gppc_test
OBJS = gppc_test.o

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir) -lgppc
gppc_config_path = $(shell $(PG_CONFIG) --includedir)/gppc_config.h
include $(PGXS)

# Version check. Some tests cannot run against newer backends.
# We intentionally leave possibility (capability) to run tests
# against different GPDB version from building time (say, build
# agains 4.1 and run test on 4.2 without recompile), so that
# we can test compatibility which is the main purpose of GPPC.
#
# For now, no additional tests are defined.
GP_VERSION_NUM := $(shell test -f $(gppc_config_path) && \
        (grep 'define  *GP_VERSION_NUM' $(gppc_config_path) \
                | sed 's/^[^0-9]*\([0-9]*\)/\1/') || echo 0)

installcheck: install

