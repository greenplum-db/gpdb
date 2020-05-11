subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS = ps_status bitstream bitmap_compression faultinjector_warnings pg_mkdir_p guc guc_gp

faultinjector_warnings.t: ../faultinjector_warnings.o faultinjector_warnings_test.o


include $(top_srcdir)/src/backend/mock.mk
