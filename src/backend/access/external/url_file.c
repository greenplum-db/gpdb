/*-------------------------------------------------------------------------
 *
 * url_file.c
 *	  Core support for opening external relations via a file URL
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 *
 * IDENTIFICATION
 *	  src/backend/access/external/url_file.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/url.h"
#include "cdb/cdbsreh.h"
#include "fstream/gfile.h"
#include "fstream/fstream.h"
#include "utils/uri.h"
#include "utils/builtins.h"

URL_FILE *
url_file_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate, int *response_code, const char **response_string)
{
	URL_FILE   *file;
	char	   *path = strchr(url + strlen(PROTOCOL_FILE), '/');
	struct fstream_options fo;

	if (forwrite)
		elog(ERROR, "cannot change a readable external table \"%s\"", pstate->cur_relname);

	memset(&fo, 0, sizeof fo);

	if (!path)
		elog(ERROR, "External Table error opening file: '%s', invalid "
			 "file path", url);
	
	file = alloc_url_file(url);
	
	file->type = CFTYPE_FILE; /* marked as local FILE */
	fo.is_csv = pstate->csv_mode;
	fo.quote = pstate->quote ? *pstate->quote : 0;
	fo.escape = pstate->escape ? *pstate->escape : 0;
	fo.header = pstate->header_line;
	fo.bufsize = 32 * 1024;
	pstate->header_line = 0;

	/*
	 * Open the file stream. This includes opening the first file to be read
	 * and finding and preparing any other files to be opened later (if a
	 * wildcard or directory is used). In addition we check ahead of time
	 * that all the involved files exists and have proper read permissions.
	 */
	file->u.file.fp = fstream_open(path, &fo, response_code, response_string);

	/* couldn't open local file. return NULL to display proper error */
	if (!file->u.file.fp)
	{
		free(file);
		return NULL;
	}

	return file;
}

void
url_file_fclose(URL_FILE *file, bool failOnError, const char *relname)
{
	fstream_close(file->u.file.fp);
	/* fstream_close() returns no error indication. */

	free(file);
}


size_t
url_file_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	struct fstream_filename_and_offset fo;
	const int whole_rows = 0; /* get as much data as possible */
	size_t		want;
	int			n;

	n = fstream_read(file->u.file.fp, ptr, size, &fo, whole_rows, "", -1);
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
	return fstream_eof(file->u.file.fp) != 0;
}

bool
url_file_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	return fstream_get_error(file->u.file.fp) != 0;
}
