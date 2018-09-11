# protect the default target for this file from the targets in Makefile.global
# and Makefile.thirdparty
default: all

BLD_TOP=..
include $(BLD_TOP)/Makefile.windows

ext_dir=../ext

top_builddir = ../..
include $(top_builddir)/src/Makefile.global

ENV_JAVA_HOME:=$(shell echo $$JAVA_HOME)
ifneq (${ENV_JAVA_HOME},)
	JAVA_HOME=${ENV_JAVA_HOME}
endif

.PHONY: all install clean distclean installcheck gphdfs mkgphdfs mkorafce

#
# targets built via top_builddir/GNUmakefile
#

all: gphdfs

gphdfs:
	@if [ "$(enable_gphdfs)" = "yes" ]; then \
		echo "gphdfs enabled"; \
		$(MAKE) -C gphdfs; \
	fi

install:
	@if [ "$(enable_gphdfs)" = "yes" ]; then \
		$(MAKE) -C gphdfs docdir=$(INSTLOC)/docs install; \
	fi

clean:
	if [ "$(enable_gphdfs)" = "yes" ]; then $(MAKE) -C gphdfs clean; fi

distclean:
	if [ "$(enable_gphdfs)" = "yes" ]; then $(MAKE) -C gphdfs distclean; fi

installcheck:
	if [ -d "$(ext_dir)" ]; then \
		PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C orafce installcheck USE_PGXS=1 && \
		PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gphdfs installcheck USE_PGXS=1 ; \
	fi; \

#
# below targets will be built via top_builddir/gpAux/Makefile
#

mkgphdfs:
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gphdfs USE_PGXS=1 docdir=$(INSTLOC)/docs install \
		ANT=/opt/releng/apache-ant/bin/ant \
		JAVA_HOME=${JAVA_HOME}

# Only include these files when running enterprise build
ENTERPRISE_TARGETS="mkgphdfs mkorafce"
ifneq ($(findstring $(MAKECMDGOALS), $(ENTERPRISE_TARGETS)),)
include $(BLD_TOP)/Makefile.global
include $(BLD_TOP)/Makefile.thirdparty
endif

# Use $JAVA_HOME if specified, otherwise look for java in the likely places.
ifeq ($(wildcard "${JAVA_HOME}/bin/java"),)
  JDK="${JAVA_HOME}"
else
  ifeq ($(wildcard "/System/Library/Frameworks/JavaVM.framework/Versions/1.7/Home/bin/java"),)
    JDK="/System/Library/Frameworks/JavaVM.framework/Versions/1.7/Home"
  else
    ifeq ($(wildcard "/usr/java/bin/java"),)
      JDK="/usr/java"
    else
      echo JAVA_HOME not found
    endif
  endif
endif
JAVA=${JAVA_HOME}/bin/java
JAVAC=${JAVA_HOME}/bin/javac
JAVAH=${JAVA_HOME}/bin/javah
JAR=${JAVA_HOME}/bin/jar
JAVADOC=${JAVA_HOME}/bin/javadoc

mkorafce:
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C orafce install USE_PGXS=1
