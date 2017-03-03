PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

override CC:=gcc
override CFLAGS:=-m64

NAME = oom_malloc
all:
	$(CC) $(CFLAGS) -shared -fPIC -o $(NAME)$(DLSUFFIX) $(NAME).c

clean:
	rm -f $(NAME)$(DLSUFFIX)
