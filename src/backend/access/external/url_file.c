/*-------------------------------------------------------------------------
 *
 * url_file.c
 *	  Core support for opening external relations via a file URL
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	  src/backend/access/external/url_file.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/url.h"
#include "cdb/cdbsreh.h"
#include "commands/copy.h"
#include "fstream/gfile.h"
#include "fstream/fstream.h"
#include "utils/uri.h"
#include "utils/builtins.h"

/*
 * Private state for a file external table, backend by an fstream.
 */
typedef struct URL_FSTREAM_FILE
{
	URL_FILE	common;

	fstream_t  *fp;
} URL_FSTREAM_FILE;

URL_FILE *
url_file_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate)
{
	URL_FSTREAM_FILE *file;
	char	   *path = strchr(url + strlen(PROTOCOL_FILE), '/');
	struct fstream_options fo;
	int			response_code;
	const char *response_string;

	if (forwrite)
		elog(ERROR, "cannot change a readable external table \"%s\"", pstate->cur_relname);

	memset(&fo, 0, sizeof fo);

	if (!path)
		elog(ERROR, "External Table error opening file: '%s', invalid "
			 "file path", url);

	file = palloc0(sizeof(URL_FSTREAM_FILE));
	file->common.type = CFTYPE_FILE; /* marked as local FILE */
	file->common.url = pstrdup(url);

	fo.is_csv = pstate->csv_mode;
	fo.quote = pstate->quote ? *pstate->quote : 0;
	fo.escape = pstate->escape ? *pstate->escape : 0;
	fo.eol_type = pstate->eol_type;
	fo.header = pstate->header_line;
	fo.bufsize = 32 * 1024;
	pstate->header_line = 0;

	/*
	 * Open the file stream. This includes opening the first file to be read
	 * and finding and preparing any other files to be opened later (if a
	 * wildcard or directory is used). In addition we check ahead of time
	 * that all the involved files exists and have proper read permissions.
	 */
	file->fp = fstream_open(path, &fo, &response_code, &response_string);

	/* Couldn't open local file. Report error. */
	if (!file->fp)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %d %s",
						path, response_code, response_string)));

	return (URL_FILE *) file;
}

void
url_file_fclose(URL_FILE *file, bool failOnError, const char *relname)
{
	URL_FSTREAM_FILE *ffile = (URL_FSTREAM_FILE *) file;

	fstream_close(ffile->fp);

	pfree(ffile->common.url);
	pfree(ffile);
}

size_t
url_file_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	URL_FSTREAM_FILE *ffile = (URL_FSTREAM_FILE *) file;
	struct fstream_filename_and_offset fo;
	const int whole_rows = 0; /* get as much data as possible */
	size_t		want;
	int			n;

	n = fstream_read(ffile->fp, ptr, size, &fo, whole_rows, "", -1);
	want = n > 0 ? n : 0;

	if (n > 0 && fo.line_number)
	{
		pstate->cur_lineno = fo.line_number - 1;

		if (pstate->cdbsreh)
			snprintf(pstate->cdbsreh->filename,
					 sizeof pstate->cdbsreh->filename,
					 "%s [%s]", pstate->filename, fo.fname);
	}
	return want;
}

bool
url_file_feof(URL_FILE *file, int bytesread)
{
	URL_FSTREAM_FILE *ffile = (URL_FSTREAM_FILE *) file;

	return fstream_eof(ffile->fp) != 0;
}

bool
url_file_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	URL_FSTREAM_FILE *ffile = (URL_FSTREAM_FILE *) file;

	return fstream_get_error(ffile->fp) != 0;
}
