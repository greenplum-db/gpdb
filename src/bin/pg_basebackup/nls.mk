# src/bin/pg_basebackup/nls.mk
CATALOG_NAME     = pg_basebackup
AVAIL_LANGUAGES  = de es fr he it ja ko pl pt_BR ru sv tr vi zh_CN
GETTEXT_FILES    = $(FRONTEND_COMMON_GETTEXT_FILES) pg_basebackup.c pg_receivewal.c pg_recvlogical.c receivelog.c streamutil.c walmethods.c ../../common/fe_memutils.c ../../common/file_utils.c
GETTEXT_TRIGGERS = $(FRONTEND_COMMON_GETTEXT_TRIGGERS) simple_prompt tar_set_error
GETTEXT_FLAGS    = $(FRONTEND_COMMON_GETTEXT_FLAGS)
