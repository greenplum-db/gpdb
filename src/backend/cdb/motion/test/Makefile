subdir=src/backend/cdb/motion
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbsenddummypacket

include $(top_builddir)/src/backend/mock.mk

cdbsenddummypacket.t: EXCL_OBJS += src/backend/cdb/motion/ic_udpifc.o
