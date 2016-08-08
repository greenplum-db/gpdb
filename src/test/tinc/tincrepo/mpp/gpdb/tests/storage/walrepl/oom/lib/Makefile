PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

NAME = oom_malloc
all:
	$(CC) $(CFLAGS) $(SHLIB_LINK) -shared -fPIC -o $(NAME)$(DLSUFFIX) $(NAME).c

clean:
	rm -f $(NAME)$(DLSUFFIX)
