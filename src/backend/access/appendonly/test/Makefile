subdir=src/backend/access/appendonly
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aomd appendonly_visimap appendonly_visimap_entry \
	aomd_filehandler aosegfiles

include $(top_srcdir)/src/backend/mock.mk

appendonly_visimap.t: \
	$(MOCK_DIR)/backend/access/appendonly/appendonly_visimap_entry_mock.o \
	$(MOCK_DIR)/backend/access/appendonly/appendonly_visimap_store_mock.o \
	$(MOCK_DIR)/backend/storage/file/buffile_mock.o \
	$(MOCK_DIR)/backend/utils/hash/dynahash_mock.o

# use the file to "mock itself" as the current testing infrastructure
#  uses mock.mk above to replace the file under test with a mock one.
#  In our case, we don't want to mock.
aomd_filehandler.t: \
	$(top_builddir)/src/backend/access/appendonly/aomd_filehandler.o

appendonly_visimap_entry.t:

aosegfiles.t: $(top_builddir)/src/backend/access/appendonly/aosegfiles.o
