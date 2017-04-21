top_builddir = ../..
include $(top_builddir)/src/Makefile.global

# do nothing if global config is not enabled
ifeq ($(enable_gpperfmon),no)
all:
	@echo "Configuration not enabled for gpperfmon."

else
.PHONY: gpsmon gpmmon

all: gpsmon gpmmon

gpsmon:
	$(MAKE) -C src/gpmon gpsmon BUILDDIR=$(BUILDDIR)

gpmmon:
	$(MAKE) -C src/gpmon gpmmon BUILDDIR=$(BUILDDIR)

clean distclean:
	$(MAKE) -C src/gpmon clean

install:
	$(MAKE) -C src/gpmon install

endif