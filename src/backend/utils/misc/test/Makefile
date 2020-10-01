subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS = ps_status bitstream bitmap_compression faultinjector_warnings pg_mkdir_p guc guc_gp

include $(top_srcdir)/src/backend/mock.mk

faultinjector_warnings.t: ../faultinjector_warnings.o \
	faultinjector_warnings_test.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o

ps_status.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o

bitstream.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o

bitmap_compression.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o

pg_mkdir_p.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o

guc.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o

guc_gp.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o
