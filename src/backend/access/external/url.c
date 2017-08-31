/*-------------------------------------------------------------------------
 *
 * url.c
 *	  Core support for opening external relations via a URL
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	  src/backend/access/external/url.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/url.h"
#include "utils/uri.h"

/* GUC */
int readable_external_table_timeout = 0;

/*
 * url_fopen
 *
 * checks for URLs or types in the 'url' and basically use the real fopen() for
 * standard files, or if the url happens to be a command to execute it uses
 * popen to execute it.
 *
 * On error, ereport()s
 */
URL_FILE *
url_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate)
{
	/*
	 * if 'url' starts with "execute:" then it's a command to execute and
	 * not a url (the command specified in CREATE EXTERNAL TABLE .. EXECUTE)
	 */
	if (pg_strncasecmp(url, EXEC_URL_PREFIX, strlen(EXEC_URL_PREFIX)) == 0)
		return url_execute_fopen(url, forwrite, ev, pstate);
	else if (IS_FILE_URI(url))
		return url_file_fopen(url, forwrite, ev, pstate);
	else if (IS_HTTP_URI(url) || IS_GPFDIST_URI(url) || IS_GPFDISTS_URI(url))
		return url_curl_fopen(url, forwrite, ev, pstate);
	else
		return url_custom_fopen(url, forwrite, ev, pstate);
}

/*
 * url_fclose: Disposes of resources associated with this external web table.
 *
 * If failOnError is true, errors encountered while closing the resource results
 * in raising an ERROR.  This is particularly true for "execute:" resources where
 * command termination is not reflected until close is called.  If failOnClose is
 * false, close errors are just logged.  failOnClose should be false when closure
 * is due to LIMIT clause satisfaction.
 *
 * relname is passed in for being available in data messages only.
 */
void
url_fclose(URL_FILE *file, bool failOnError, const char *relname)
{
	if (file == NULL)
	{
		elog(WARNING, "internal error: call url_fclose with bad parameter");
		return;
	}

	switch (file->type)
	{
		case CFTYPE_FILE:
			url_file_fclose(file, failOnError, relname);
			break;

		case CFTYPE_EXEC:
			url_execute_fclose(file, failOnError, relname);
			break;

		case CFTYPE_CURL:
			url_curl_fclose(file, failOnError, relname);
			break;

		case CFTYPE_CUSTOM:
			url_custom_fclose(file, failOnError, relname);
			break;
			
		default: /* unknown or unsupported type - oh dear */
			elog(ERROR, "unrecognized external table type: %d", file->type);
			break;
    }
}

bool
url_feof(URL_FILE *file, int bytesread)
{
    switch (file->type)
    {
		case CFTYPE_FILE:
			return url_file_feof(file, bytesread);

		case CFTYPE_EXEC:
			return url_execute_feof(file, bytesread);

		case CFTYPE_CURL:
			return url_curl_feof(file, bytesread);

		case CFTYPE_CUSTOM:
			return url_custom_feof(file, bytesread);

		default: /* unknown or supported type - oh dear */
			elog(ERROR, "unrecognized external table type: %d", file->type);
    }
}


bool
url_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	switch (file->type)
	{
		case CFTYPE_FILE:
			return url_file_ferror(file, bytesread, ebuf, ebuflen);

		case CFTYPE_EXEC:
			return url_execute_ferror(file, bytesread, ebuf, ebuflen);

		case CFTYPE_CURL:
			return url_curl_ferror(file, bytesread, ebuf, ebuflen);

		case CFTYPE_CUSTOM:
			return url_custom_ferror(file, bytesread, ebuf, ebuflen);

		default: /* unknown or supported type - oh dear */
			elog(ERROR, "unrecognized external table type: %d", file->type);
	}
}

size_t
url_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
    switch (file->type)
    {
		case CFTYPE_FILE:
			return url_file_fread(ptr, size, file, pstate);

		case CFTYPE_EXEC:
			return url_execute_fread(ptr, size, file, pstate);

		case CFTYPE_CURL:
			return url_curl_fread(ptr, size, file, pstate);

		case CFTYPE_CUSTOM:
			return url_custom_fread(ptr, size, file, pstate);
				
		default: /* unknown or supported type */
			elog(ERROR, "unrecognized external table type: %d", file->type);
    }
}

size_t
url_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
    switch (file->type)
    {
		case CFTYPE_FILE:
			elog(ERROR, "CFTYPE_FILE not yet supported in url.c");
			return 0;		/* keep compiler quiet */

		case CFTYPE_EXEC:
			return url_execute_fwrite(ptr, size, file, pstate);

		case CFTYPE_CURL:
			return url_curl_fwrite(ptr, size, file, pstate);

		case CFTYPE_CUSTOM:
			return url_custom_fwrite(ptr, size, file, pstate);
			
		default: /* unknown or unsupported type */
			elog(ERROR, "unrecognized external table type: %d", file->type);
    }
}

/*
 * flush all remaining buffered data waiting to be written out to external source
 */
void
url_fflush(URL_FILE *file, CopyState pstate)
{
    switch (file->type)
    {
		case CFTYPE_FILE:
			elog(ERROR, "CFTYPE_FILE not yet supported in url.c");
			break;

		case CFTYPE_EXEC:
		case CFTYPE_CUSTOM:
			/* data isn't buffered on app level. no op */
			break;

		case CFTYPE_CURL:
			url_curl_fflush(file, pstate);
			break;

		default: /* unknown or unsupported type */
			elog(ERROR, "unrecognized external table type: %d", file->type);
    }
}
