#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for motion (CDB2)
#
#-------------------------------------------------------------------------


subdir = src/backend/cdb/motion
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = cdbmotion.o tupchunklist.o tupser.o  \
	ic_common.o ic_tcp.o ic_udpifc.o htupfifo.o tupleremap.o

ifeq ($(enable_ic_proxy),yes)
# server
OBJS += ic_proxy_bgworker.o
OBJS += ic_proxy_main.o
OBJS += ic_proxy_client.o
OBJS += ic_proxy_peer.o
OBJS += ic_proxy_router.o

# backend
OBJS += ic_proxy_backend.o

# utils
OBJS += ic_proxy_addr.o
OBJS += ic_proxy_key.o
OBJS += ic_proxy_packet.o
OBJS += ic_proxy_pkt_cache.o
OBJS += ic_proxy_iobuf.o
endif  # enable_ic_proxy

include $(top_srcdir)/src/backend/common.mk
