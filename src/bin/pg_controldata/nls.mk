# src/bin/pg_controldata/nls.mk
CATALOG_NAME     = pg_controldata
AVAIL_LANGUAGES  = cs de es fr it ja ko pl ru sv tr uk vi zh_CN
GETTEXT_FILES    = pg_controldata.c ../../common/controldata_utils.c
GETTEXT_TRIGGERS = $(FRONTEND_COMMON_GETTEXT_TRIGGERS)
GETTEXT_FLAGS    = $(FRONTEND_COMMON_GETTEXT_FLAGS)
