RELENG_TOOLS=$(HOME)/releng-tools

include $(BLD_TOP)/Makefile.global

# e.g. 
# $(RELENG_TOOLS)/geos-3.2.2
# $(RELENG_TOOLS)/proj-4.7.0
TOOL_ARGS=$(subst -, ,$*)
TOOL_NAME=$(word 1,$(TOOL_ARGS))
TOOL_VER=$(word 2,$(TOOL_ARGS))
$(RELENG_TOOLS)/%:
	$(MAKE) p4client
	@if [ -z "`p4 files //tools/$(TOOL_NAME)/$(TOOL_VER)/dist/$(BLD_ARCH)/\*`" ]; then \
		echo "****************************************************************************"; \
		echo "Could not find a distro for $(TOOL_NAME)-$(TOOL_VER) on $(BLD_ARCH) in the //tools/... depot!"; \
		echo "Exiting this build..."; \
		echo "****************************************************************************"; \
		exit 1; \
	fi
	mkdir -p $(RELENG_TOOLS)
	cd $(RELENG_TOOLS); p4 -c releng-tools sync -f //tools/$(TOOL_NAME)/$(TOOL_VER)/dist/$(BLD_ARCH)/...
	mv $(RELENG_TOOLS)/tmp/$(TOOL_NAME)/$(TOOL_VER)/dist/$(BLD_ARCH)/ $@
	rm -rf $(RELENG_TOOLS)/tmp
	# hack for R-2.13.0, and possibly other tools
	if [ -f $@/$(TOOL_NAME)-$(BLD_ARCH)-$(TOOL_VER).targz ]; then \
		cd $@; tar xvf $(TOOL_NAME)-$(BLD_ARCH)-$(TOOL_VER).targz; \
		mv $@/$(BLD_ARCH)/* $@; \
		rm -rf $@/$(BLD_ARCH) $@/$(TOOL_NAME)-$(BLD_ARCH)-$(TOOL_VER).targz; \
	fi
