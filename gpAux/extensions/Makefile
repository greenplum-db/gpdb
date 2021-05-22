# protect the default target for this file from the targets in Makefile.global
default: all

BLD_TOP=..

ext_dir=../ext

top_builddir = ../..
include $(top_builddir)/src/Makefile.global

ENV_JAVA_HOME:=$(shell echo $$JAVA_HOME)
ifneq (${ENV_JAVA_HOME},)
	JAVA_HOME=${ENV_JAVA_HOME}
endif

#
# below targets will be built via top_builddir/gpAux/Makefile
#

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
