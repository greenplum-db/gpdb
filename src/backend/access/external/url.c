/*-------------------------------------------------------------------------
 *
 * url.c
 *	  Core support for opening external relations via a URL
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *	  src/backend/access/external/url.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/url.h"
#include "cdb/cdbdtxcontextinfo.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbtm.h"
#include "commands/dbcommands.h"
#include "libpq/libpq-be.h"
#include "miscadmin.h"
#include "postmaster/postmaster.h"		/* postmaster port */
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/uri.h"

/* GUC */
int readable_external_table_timeout = 0;
int write_to_gpfdist_timeout = 300;

static void base16_encode(char *raw, int len, char *encoded);
static char *get_eol_delimiter(List *params);

void
external_set_env_vars(extvar_t *extvar, char *uri, bool csv, char *escape, char *quote, bool header, uint32 scancounter)
{
	external_set_env_vars_ext(extvar, uri, csv, escape, quote, EOL_UNKNOWN, header, scancounter, NULL);
}

void
external_set_env_vars_ext(extvar_t *extvar, char *uri, bool csv, char *escape, char *quote, int eol_type, bool header,
						  uint32 scancounter, List *params)
{
	time_t		now = time(0);
	struct tm  *tm = localtime(&now);
	char	   *result = (char *) palloc(7);	/* sign, 5 digits, '\0' */

	char	   *encoded_delim;
	int			line_delim_len;

	snprintf(extvar->GP_CSVOPT, sizeof(extvar->GP_CSVOPT),
			"m%dx%dq%dn%dh%d",
			csv ? 1 : 0,
			escape ? 255 & *escape : 0,
			quote ? 255 & *quote : 0,
			eol_type,
			header ? 1 : 0);

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		pg_ltoa(qdPostmasterPort, result);
		extvar->GP_MASTER_PORT = result;
		extvar->GP_MASTER_HOST = qdHostname;
	}
	else
	{
		CdbComponentDatabaseInfo *qdinfo =
				cdbcomponent_getComponentInfo(MASTER_CONTENT_ID);

		pg_ltoa(qdinfo->config->port, result);
		extvar->GP_MASTER_PORT = result;

		if (qdinfo->config->hostip != NULL)
			extvar->GP_MASTER_HOST = pstrdup(qdinfo->config->hostip);
		else
			extvar->GP_MASTER_HOST = pstrdup(qdinfo->config->hostname);
	}

	if (MyProcPort)
		extvar->GP_USER = MyProcPort->user_name;
	else
		extvar->GP_USER = "";

	extvar->GP_DATABASE = get_database_name(MyDatabaseId);
	extvar->GP_SEG_PG_CONF = ConfigFileName;	/* location of the segments
												 * pg_conf file  */
	extvar->GP_SEG_DATADIR = DataDir;	/* location of the segments
												 * datadirectory */
	sprintf(extvar->GP_DATE, "%04d%02d%02d",
			1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday);
	sprintf(extvar->GP_TIME, "%02d%02d%02d",
			tm->tm_hour, tm->tm_min, tm->tm_sec);

	/*
	 * read-only query don't have a valid distributed transaction ID, use
	 * "session id"-"command id" to identify the transaction.
	 */
	if (!getDistributedTransactionIdentifier(extvar->GP_XID))
		sprintf(extvar->GP_XID, "%u-%.10u", gp_session_id, gp_command_count);

	sprintf(extvar->GP_CID, "%x", QEDtxContextInfo.curcid);
	sprintf(extvar->GP_SN, "%x", scancounter);
	sprintf(extvar->GP_SEGMENT_ID, "%d", GpIdentity.segindex);
	sprintf(extvar->GP_SEG_PORT, "%d", PostPortNumber);
	sprintf(extvar->GP_SESSION_ID, "%d", gp_session_id);
	sprintf(extvar->GP_SEGMENT_COUNT, "%d", getgpsegmentCount());

	extvar->GP_QUERY_STRING = (char *)debug_query_string;

	if (NULL != params)
	{
		char	   *line_delim_str = get_eol_delimiter(params);

		line_delim_len = (int) strlen(line_delim_str);
		if (line_delim_len > 0)
		{
			encoded_delim = (char *) (palloc(line_delim_len * 2 + 1));
			base16_encode(line_delim_str, line_delim_len, encoded_delim);
		}
		else
		{
			line_delim_len = -1;
			encoded_delim = "";
		}
	}
	else
	{
		switch(eol_type)
		{
			case EOL_CR:
				encoded_delim = "0D";
				line_delim_len = 1;
				break;
			case EOL_NL:
				encoded_delim = "0A";
				line_delim_len = 1;
				break;
			case EOL_CRNL:
				encoded_delim = "0D0A";
				line_delim_len = 2;
				break;
			default:
				encoded_delim = "";
				line_delim_len = -1;
				break;
		}
	}
	extvar->GP_LINE_DELIM_STR = pstrdup(encoded_delim);
	sprintf(extvar->GP_LINE_DELIM_LENGTH, "%d", line_delim_len);
}

static void
base16_encode(char *raw, int len, char *encoded)
{
	const char *raw_bytes = raw;
	char	   *encoded_bytes = encoded;
	int			remaining = len;

	for (; remaining--; encoded_bytes += 2)
	{
		sprintf(encoded_bytes, "%02x", *(raw_bytes++));
	}
}

static char *
get_eol_delimiter(List *params)
{
	ListCell   *lc = params->head;

	while (lc)
	{
		if (pg_strcasecmp(((DefElem *) lc->data.ptr_value)->defname, "line_delim") == 0)
			return pstrdup(((Value *) ((DefElem *) lc->data.ptr_value)->arg)->val.str);
		lc = lc->next;
	}
	return pstrdup("");
}

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
url_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate, ExternalSelectDesc desc)
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
		return url_custom_fopen(url, forwrite, ev, pstate, desc);
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
url_fread(void *ptr,
          size_t size,
          URL_FILE *file,
          CopyState pstate)
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
