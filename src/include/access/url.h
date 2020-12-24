/*-------------------------------------------------------------------------
 *
 * url.h
 *    routines for external table access to urls.
 *    to the qExec processes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * src/include/access/url.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef URL_H
#define URL_H

#include "access/extprotocol.h"

#include "commands/copy.h"

/*
 * Different "flavors", or implementations, of external tables.
 */
enum fcurl_type_e
{
	CFTYPE_NONE = 0,
	CFTYPE_FILE = 1,
	CFTYPE_CURL = 2,
	CFTYPE_EXEC = 3,
	CFTYPE_CUSTOM = 4
};

/*
 * Common state of all different implementations external tables. The
 * implementation-specific url_*_fopen() function returns a palloc'd
 * struct that begins with this common part.
 */
typedef struct URL_FILE
{
	enum fcurl_type_e type;     /* type of handle */
	char *url;

	/* implementation-specific fields follow. */

} URL_FILE;

typedef struct extvar_t
{
	char* GP_MASTER_HOST;
	char* GP_MASTER_PORT;
	char* GP_DATABASE;
	char* GP_USER;
	char* GP_SEG_PG_CONF;   /*location of the segments pg_conf file*/
	char* GP_SEG_DATADIR;   /*location of the segments datadirectory*/
	char GP_DATE[9];		/* YYYYMMDD */
	char GP_TIME[7];		/* HHMMSS */
	char GP_XID[TMGIDSIZE];		/* global transaction id */
	char GP_CID[10];		/* command id */
	char GP_SN[10];		/* scan number */
	char GP_SEGMENT_ID[11];  /*segments content id*/
	char GP_SEG_PORT[11];
	char GP_SESSION_ID[11];  /* session id */
	char GP_SEGMENT_COUNT[11]; /* total number of (primary) segs in the system */
 	char GP_CSVOPT[13]; /* "m.x...q...h." former -q, -h and -x options for gpfdist.*/

 	/* EOL vars */
 	char* GP_LINE_DELIM_STR;
	char GP_LINE_DELIM_LENGTH[11];
	char *GP_QUERY_STRING;
} extvar_t;


/* an EXECUTE string will always be prefixed like this */
#define EXEC_URL_PREFIX "execute:"

extern void external_set_env_vars(extvar_t *extvar, char *uri, bool csv, char *escape,
								  char *quote, bool header, uint32 scancounter);
extern void external_set_env_vars_ext(extvar_t *extvar, char *uri, bool csv, char *escape,
									  char *quote, int eol_type, bool header,
									  uint32 scancounter, List *params);

/* exported functions */
extern URL_FILE *url_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate, ExternalSelectDesc desc);
extern void url_fclose(URL_FILE *file, bool failOnError, const char *relname);
extern bool url_feof(URL_FILE *file, int bytesread);
extern bool url_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen);
extern size_t url_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate);
extern size_t url_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate);
extern void url_fflush(URL_FILE *file, CopyState pstate);

/* prototypes for functions in url_execute.c */
extern char *make_command(const char *cmd, extvar_t *ev);

/* implementation-specific functions. */
extern URL_FILE *url_curl_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate);
extern void url_curl_fclose(URL_FILE *file, bool failOnError, const char *relname);
extern bool url_curl_feof(URL_FILE *file, int bytesread);
extern bool url_curl_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen);
extern size_t url_curl_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate);
extern size_t url_curl_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate);
extern void url_curl_fflush(URL_FILE *file, CopyState pstate);

extern URL_FILE *url_file_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate);
extern void url_file_fclose(URL_FILE *file, bool failOnError, const char *relname);
extern bool url_file_feof(URL_FILE *file, int bytesread);
extern bool url_file_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen);
extern size_t url_file_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate);

extern URL_FILE *url_execute_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate);
extern void url_execute_fclose(URL_FILE *file, bool failOnError, const char *relname);
extern bool url_execute_feof(URL_FILE *file, int bytesread);
extern bool url_execute_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen);
extern size_t url_execute_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate);
extern size_t url_execute_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate);

extern URL_FILE *url_custom_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate, ExternalSelectDesc desc);
extern void url_custom_fclose(URL_FILE *file, bool failOnError, const char *relname);
extern bool url_custom_feof(URL_FILE *file, int bytesread);
extern bool url_custom_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen);
extern size_t url_custom_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate);
extern size_t url_custom_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate);

/* GUC */
extern int readable_external_table_timeout;
extern int write_to_gpfdist_timeout;

#endif
