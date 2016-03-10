# Makefile for codegen

subdir = src/backend/codegen
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

all:
	make -C utils/build all

install:
	make -C utils/build install

clean:
	rm -rf utils/build


OBJS = init_codegen.o

override CXXFLAGS := -std=c++11 $(CXXFLAGS) -L./utils/build -lgpcodegen
include $(top_srcdir)/src/backend/common.mk
