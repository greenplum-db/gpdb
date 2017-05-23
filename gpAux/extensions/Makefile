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

all: mkgphdfs mkgpcloud

.PHONY:
mapreduce:
	@if [ "$(enable_mapreduce)" = "yes" ]; then \
		echo "gpmapreduce enabled"; \
		$(MAKE) -C gpmapreduce; \
	fi

install: mapreduce
	@if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce install; \
	fi

clean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce clean; fi


distclean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce distclean; fi

.PHONY:
mkgphdfs:
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gphdfs USE_PGXS=1 docdir=$(INSTLOC)/docs install \
		ANT=/opt/releng/apache-ant/bin/ant \
		JAVA_HOME=${JAVA_HOME}

.PHONY:
mkgpcloud:
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gpcloud USE_PGXS=1 install
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gpcloud/bin/gpcheckcloud USE_PGXS=1 install

# Only include include these files when running enterprise build
ENTERPRISE_TARGETS="mkgphdfs mkorafce"
ifneq ($(findstring $(MAKECMDGOALS), $(ENTERPRISE_TARGETS)),)
include $(BLD_TOP)/Makefile.global
include $(BLD_TOP)/Makefile.thirdparty
endif

# Use $JAVA_HOME if specified, otherwise look for java in the likely places.
# Note that Darwin's JVM layout is pretty weird.  JDK 1.6 apears to be the
# default for Snow Leopard, but JDK 1.5 is for Leopard.
# See http://developer.apple.com/qa/qa2001/qa1170.html
ifeq ($(wildcard "${JAVA_HOME}/bin/java"),)
  JDK="${JAVA_HOME}"
else
  ifeq ($(wildcard "/System/Library/Frameworks/JavaVM.framework/Versions/1.6/Home/bin/java"),)
    JDK="/System/Library/Frameworks/JavaVM.framework/Versions/1.6/Home"
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


.PHONY:
mkorafce:
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C orafce install USE_PGXS=1

installcheck:
	if [ -d "$(ext_dir)" ]; then \
		PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C orafce installcheck USE_PGXS=1 && \
		PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gphdfs installcheck USE_PGXS=1 ; \
	fi; \
	if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce installcheck; \
	fi

