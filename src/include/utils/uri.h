/*-------------------------------------------------------------------------
 *
 * uri.h
 *	  Definitions for URI strings
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/uri.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef URI_H
#define URI_H

typedef enum UriProtocol
{
	URI_FILE,
	URI_FTP,
	URI_HTTP,
	URI_GPFDIST,
	URI_GPHDFS,
	URI_CUSTOM,
	URI_GPFDISTS
}	UriProtocol;

#define PROTOCOL_FILE		"file://"
#define PROTOCOL_FTP		"ftp://"
#define PROTOCOL_HTTP		"http://"
#define PROTOCOL_GPFDIST	"gpfdist://"
#define PROTOCOL_GPFDISTS	"gpfdists://"
#define PROTOCOL_GPHDFS		"gphdfs://"

/* 
 * sometimes we don't want to parse the whole URI but just take a peek at
 * which protocol it is. We can use these macros to do just that.
 */
#define IS_FILE_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_FILE, strlen(PROTOCOL_FILE)) == 0)
#define IS_HTTP_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_HTTP, strlen(PROTOCOL_HTTP)) == 0)
#define IS_GPFDIST_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_GPFDIST, strlen(PROTOCOL_GPFDIST)) == 0)
#define IS_GPFDISTS_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_GPFDISTS, strlen(PROTOCOL_GPFDISTS)) == 0) 
#define IS_FTP_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_FTP, strlen(PROTOCOL_FTP)) == 0)
#define IS_GPHDFS_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_GPHDFS, strlen(PROTOCOL_GPHDFS)) == 0)

typedef struct Uri
{
	UriProtocol protocol;
	char	   *hostname;
	int         port;
	char	   *path;
	char	   *customprotocol;
}	Uri;

extern Uri *ParseExternalTableUri(const char *uri);
extern void FreeExternalTableUri(Uri *uri);

#endif   /* URI_H */
