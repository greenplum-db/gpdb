# src/pl/plperl/nls.mk
CATALOG_NAME	:= plperl
AVAIL_LANGUAGES	:=
GETTEXT_FILES	:= plperl.c SPI.c
GETTEXT_TRIGGERS:= errmsg errmsg_plural:1,2 errdetail errdetail_log errdetail_plural:1,2 errhint errcontext
