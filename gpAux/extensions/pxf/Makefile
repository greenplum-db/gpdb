# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
#top_builddir = ../../..
#include $(top_builddir)/src/Makefile.global

MODULE_big = pxf
OBJS       = src/pxfprotocol.o
REGRESS    = basic

CFLAGS += -lcurl

#default: all

#all: pxf_protocol.c
#       gcc -o pxf_protocol pxf_protocol.c

#ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
#else
#subdir = src/bin/gpfusion
#include $(top_srcdir)/contrib/contrib-global.mk
#endif


all: unittest install

unittest:
    @$(MAKE) -C test test