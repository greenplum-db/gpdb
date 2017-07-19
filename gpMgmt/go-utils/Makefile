# do nothing if global config is not enabled
ifeq ($(enable_go_utils),no)
all:
	@echo "Configuration not enabled for go-utils."

else
all:
	$(MAKE) -C src/gp_upgrade $@

clean distclean:
	$(MAKE) -C src/gp_upgrade clean

install:
	$(MAKE) -C src/gp_upgrade $@

endif