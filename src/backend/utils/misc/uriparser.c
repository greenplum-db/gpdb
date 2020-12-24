/*-------------------------------------------------------------------------
 *
 * uriparser.c
 *	  Functions for parsing URI strings
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/uriparser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/uri.h"

#include <ctype.h>
#include <arpa/inet.h>	 /* inet_ntoa() */ 

/*
 * ParseExternalTableUri
 * 
 * This routines converts a string to a supported external 
 * table URI object. It is also used to validate the URI format.
 */
Uri *
ParseExternalTableUri(const char *uri_str)
{
	Uri		   *uri = (Uri *) palloc0(sizeof(Uri));
	char	   *start,
			   *end;
	int			protocol_len,
				len;

 	uri->port = -1;
 	uri->hostname = NULL;
 	uri->path = NULL;
 	uri->customprotocol = NULL;
 	
	/*
	 * parse protocol
	 */
	if (IS_FILE_URI(uri_str))
	{
		uri->protocol = URI_FILE;
		protocol_len = strlen(PROTOCOL_FILE);
	}
	else if (IS_FTP_URI(uri_str))
	{
		uri->protocol = URI_FTP;
		protocol_len = strlen(PROTOCOL_FTP);
	}
	else if (IS_HTTP_URI(uri_str))
	{
		uri->protocol = URI_HTTP;
		protocol_len = strlen(PROTOCOL_HTTP);
	}
	else if (IS_GPFDIST_URI(uri_str))
	{
		uri->protocol = URI_GPFDIST;
		protocol_len = strlen(PROTOCOL_GPFDIST);
	}
	else if (IS_GPFDISTS_URI(uri_str))
	{
		uri->protocol = URI_GPFDISTS;
		protocol_len = strlen(PROTOCOL_GPFDISTS);
	}

	else /* not recognized. treat it as a custom protocol */
	{
		char *post_protocol = strstr(uri_str, "://");
		
		if(!post_protocol)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid URI \'%s\' : undefined structure", uri_str)));
		}
		else
		{
			protocol_len = post_protocol - uri_str;
			uri->customprotocol = (char *) palloc (protocol_len + 1);
			strncpy(uri->customprotocol, uri_str, protocol_len);
			uri->customprotocol[protocol_len] = '\0';
			uri->protocol = URI_CUSTOM;
			return uri; /* we let the user parse it himself later on */
		}
		
		/* this is a non existing protocol */
		
		protocol_len = 0; /* shut compiler up */
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid URI \'%s\' : undefined protocol", uri_str)));
	}

	/* make sure there is more to the uri string */
	if (strlen(uri_str) <= protocol_len)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid URI \'%s\' : missing host name and path", uri_str)));

	/*
	 * parse host name
	 */
	start = (char *) uri_str + protocol_len;

	if (*start == '/')			/* format "prot:///" ? (no hostname) */
	{
		/* the default is "localhost" */
		uri->hostname = pstrdup("localhost");

		end = start;
	}
	else
	{
		end = strchr(start, '/');

		if (end == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid URI \'%s\' : missing host name or path", uri_str)));
		}
		else
		{
			char   *colon, *p;

			len = end - start;

			/*
			 * host
			 */
			uri->hostname = (char *) palloc(len + 1);
			strncpy(uri->hostname, start, len);
			uri->hostname[len] = '\0';
			
			/*
			 * MPP-13617, if we have an ipv6 address in the URI hostname 
			 * (e.g. [2620:0:170:610::11]:8080/path/data.txt ) then we 
			 * we start our search for the :port after the closing ].
			 */
			p = strchr(uri->hostname, ']');
			if (p) 
			{
				colon = strchr(p, ':');

				/*
				 * Eliminate the [ ] from the hostname.  
				 * note we don't change the uri->hostname pointer because we pfree() it later
				 */
				*p = '\0';
				for (p = strchr(uri->hostname, '['); p && *p; p++)
				{
					p[0] = p[1];
				}
			}
			else
			{
				colon = strchr(uri->hostname, ':');
			}

			/*
			 * port
			 */
			if (colon) 
			{
				int portlen = 0;
				
				uri->port = atoi(colon + 1);
				portlen = strlen(colon);
				
				/* now truncate ":<port>" from hostname */
				uri->hostname[len - portlen] = '\0';

				*colon = 0;
			}
			else
			{
				uri->port = -1; /* no port was indicated. will use default if needed */
			}
		}
	}

	/*
	 * We continue from the trailing host '/' since the
	 * path is an absolute path. Our previous ending point
	 * is the beginning of the file path, until the end of
	 * the uri string.
	 */
	start = end;

	len = strlen(start);

	/* make sure there is more to the uri string */
	if (len <= 1)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid URI \'%s\' : missing path", uri_str)));

	uri->path = (char *) palloc(len + 1);
	strcpy(uri->path, start);
	uri->path[len] = '\0';

	return uri;
}

void FreeExternalTableUri(Uri *uri)
{
	if (uri->hostname)
		pfree(uri->hostname);
	if (uri->path)
		pfree(uri->path);
	if (uri->customprotocol)
		pfree(uri->customprotocol);
	
	pfree(uri);
}
