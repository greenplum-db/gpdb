##-------------------------------------------------------------------------------------
##
## Copyright (C) 2012 EMC Corp.
##
## @doc: GP optimizer build 
##
## @author: solimm1
##
##-------------------------------------------------------------------------------------

UNAME = $(shell uname)
UNAME_P = $(shell uname -p)
UNAME_M = $(shell uname -m)

ARCH_OS = GPOS_$(UNAME)
ARCH_CPU = GPOS_$(UNAME_P)

ifeq "$(BLD_TYPE)" "opt"
	GPOPT_flags = -O3 -fno-omit-frame-pointer -g3
else
	GPOPT_flags = -g3 -DGPOS_DEBUG
endif

ifeq (x86_64, $(UNAME_M))
	ARCH_BIT = GPOS_64BIT
else
	ARCH_BIT = GPOS_32BIT
endif

ifeq ($(ARCH_BIT), GPOS_32BIT)
	ARCH_FLAGS = -m32
else
	ARCH_FLAGS = -m64
endif

BLD_FLAGS = $(ARCH_FLAGS) -D$(ARCH_BIT) -D$(ARCH_CPU) -D$(ARCH_OS) $(GPOPT_flags)
override CPPFLAGS := -fPIC $(CPPFLAGS)
override CPPFLAGS := $(BLD_FLAGS)  $(CPPFLAGS)
override CPPFLAGS := -DGPOS_VERSION=\"$(LIBGPOS_VER)\" $(CPPFLAGS)
override CPPFLAGS := -DGPOPT_VERSION=\"$(OPTIMIZER_VER)\" $(CPPFLAGS)
override CPPFLAGS := -DXERCES_VERSION=\"$(XERCES_VER)\" $(CPPFLAGS)
override CPPFLAGS := $(XERCES_INCLUDEFLAGS) $(CPPFLAGS)
