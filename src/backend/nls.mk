# $PostgreSQL: pgsql/src/backend/nls.mk,v 1.22 2008/03/24 18:08:47 tgl Exp $
CATALOG_NAME	:= postgres
AVAIL_LANGUAGES	:= de es fr ja pt_BR tr
GETTEXT_FILES	:= + gettext-files
GETTEXT_TRIGGERS:= _ errmsg errmsg_plural:1,2 errdetail errdetail_log errdetail_plural:1,2 errhint errcontext write_stderr yyerror

gettext-files: distprep
	find $(srcdir)/ $(srcdir)/../port/ -name '*.c' -print >$@

my-maintainer-clean:
	rm -f gettext-files

.PHONY: my-maintainer-clean
maintainer-clean: my-maintainer-clean
