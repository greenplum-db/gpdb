/*-------------------------------------------------------------------------
 *
 * url_curl.c
 *	  Core support for opening external relations via a URL with curl
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 *
 * IDENTIFICATION
 *	  src/backend/access/external/url_curl.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/url.h"

#ifdef USE_CURL

#include <arpa/inet.h>

#include <curl/curl.h>

#include "cdb/cdbsreh.h"
#include "cdb/cdbutil.h"
#include "miscadmin.h"
#include "utils/guc.h"
#include "utils/uri.h"

#if BYTE_ORDER == BIG_ENDIAN
#define local_htonll(n)  (n)
#define local_ntohll(n)  (n)
#else
#define local_htonll(n)  ((((uint64) htonl(n)) << 32LL) | htonl((n) >> 32LL))
#define local_ntohll(n)  ((((uint64) ntohl(n)) << 32LL) | (uint32) ntohl(((uint64)n) >> 32LL))
#endif

#define HOST_NAME_SIZE 100
#define FDIST_TIMEOUT  408
#define MAX_TRY_WAIT_TIME 64

/*
 * SSL support GUCs - should be added soon. Until then we will use stubs
 *
 *  SSL Params
 *	extssl_protocol  CURL_SSLVERSION_TLSv1 				
 *  extssl_cipher 	 TLS_RSA_WITH_AES_128_CBC_SHA
 *  extssl_verifycert 	1
 *  extssl_verifyhost 	2 										
 *  extssl_cert 		"gpfdists/client.crt"
 *  extssl_key 			"gpfdists/client.key"
 *  extssl_pass 		"?" 										
 *  extssl_crl 			NULL 									
 *  Misc Params
 *  extssl_libcurldebug 1 	
 */

static int extssl_protocol  = CURL_SSLVERSION_TLSv1;
const char* extssl_cipher = "AES128-SHA";
static int extssl_verifycert = 1;
static int extssl_verifyhost = 2;
const char* extssl_cert = "gpfdists/client.crt";
const char* extssl_key = "gpfdists/client.key";
const char* extssl_ca = "gpfdists/root.crt";
const char* extssl_pass = NULL;
const char* extssl_crl = NULL;
static int extssl_libcurldebug = 1;
char extssl_key_full[MAXPGPATH] = {0};
char extssl_cer_full[MAXPGPATH] = {0};
char extssl_cas_full[MAXPGPATH] = {0};

/* Will hold the last curl error					*/
/* Currently it is in use only for SSL connection,	*/
/* but we should consider using it always			*/
static char curl_Error_Buffer[CURL_ERROR_SIZE];

static bool gp_proto0_write_done(URL_FILE *file);
static void extract_http_domain(char* i_path, char* o_domain, int dlen);

/* we use a global one for convenience */
static CURLM *multi_handle = 0;

/*
 * header_callback
 *
 * when a header arrives from the server curl calls this routine. In here we
 * extract the information we are interested in from the header, and store it
 * in the passed in callback argument (URL_FILE *) which lives in our
 * application.
 */
static size_t
header_callback(void *ptr_, size_t size, size_t nmemb, void *userp)
{
    URL_FILE*	url = (URL_FILE *)userp;
	char*		ptr = ptr_;
	int 		len = size * nmemb;
	int 		i;
	char 		buf[20];

	Assert(size == 1);

	/*
	 * parse the http response line (code and message) from
	 * the http header that we get. Basically it's the whole
	 * first line (e.g: "HTTP/1.0 400 time out"). We do this
	 * in order to capture any error message that comes from
	 * gpfdist, and later use it to report the error string in
	 * check_response() to the database user.
	 */
	if (url->u.curl.http_response == 0)
	{
		int 	n = nmemb;
		char* 	p;

		if (n > 0 && 0 != (p = palloc(n+1)))
		{
			memcpy(p, ptr, n);
			p[n] = 0;

			if (n > 0 && (p[n-1] == '\r' || p[n-1] == '\n'))
				p[--n] = 0;

			if (n > 0 && (p[n-1] == '\r' || p[n-1] == '\n'))
				p[--n] = 0;

			url->u.curl.http_response = p;
		}
	}

	/*
	 * extract the GP-PROTO value from the HTTP header.
	 */
	if (len > 10 && *ptr == 'X' && 0 == strncmp("X-GP-PROTO", ptr, 10))
	{
		ptr += 10;
		len -= 10;

		while (len > 0 && (*ptr == ' ' || *ptr == '\t'))
		{
			ptr++;
			len--;
		}

		if (len > 0 && *ptr == ':')
		{
			ptr++;
			len--;

			while (len > 0 && (*ptr == ' ' || *ptr == '\t'))
			{
				ptr++;
				len--;
			}

			for (i = 0; i < sizeof(buf) - 1 && i < len; i++)
				buf[i] = ptr[i];

			buf[i] = 0;
			url->u.curl.gp_proto = strtol(buf, 0, 0);
		}
	}

	return size * nmemb;
}


/*
 * write_callback
 *
 * when data arrives from gpfdist server and curl is ready to write it
 * to our application, it calls this routine. In here we will store the
 * data in the application variable (URL_FILE *)file which is the passed
 * in the forth argument as a part of the callback settings.
 *
 * we return the number of bytes written to the application buffer
 */
static size_t
write_callback(char *buffer, size_t size, size_t nitems, void *userp)
{
    URL_FILE*	file = (URL_FILE *)userp;
	curlctl_t*	curl = &file->u.curl;
	const int 	nbytes = size * nitems;
	int 		n;

	/*
	 * if insufficient space in buffer make more space
	 */
	if (curl->in.top + nbytes >= curl->in.max)
	{
		/* compact ? */
		if (curl->in.bot)
		{
			n = curl->in.top - curl->in.bot;
			memmove(curl->in.ptr, curl->in.ptr + curl->in.bot, n);
			curl->in.bot = 0;
			curl->in.top = n;
		}

		/* if still insufficient space in buffer, then do realloc */
		if (curl->in.top + nbytes >= curl->in.max)
		{
			char *newbuf;

			n = curl->in.top - curl->in.bot + nbytes + 1024;
			newbuf = realloc(curl->in.ptr, n);

			if (!newbuf)
				elog(ERROR, "out of memory (curl write_callback)");

			curl->in.ptr = newbuf;
			curl->in.max = n;

			Assert(curl->in.top + nbytes < curl->in.max);
		}
	}

	/* enough space. copy buffer into curl->buf */
	memcpy(curl->in.ptr + curl->in.top, buffer, nbytes);
	curl->in.top += nbytes;

	return nbytes;
}

/*
 * check_response
 *
 * If got an HTTP response with an error code from the server (gpfdist), report
 * the error code and message it to the database user and abort operation.
 */
static int
check_response(URL_FILE *file, int *rc, const char **response_string, bool do_close)
{
	long 		response_code;
	char*		effective_url = NULL;
	CURL* 		curl = file->u.curl.handle;
	static char buffer[30];

	/* get the response code from curl */
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code) != CURLE_OK)
	{
		*rc = 500;
		*response_string = "curl_easy_getinfo failed";
		return -1;
	}
	*rc = response_code;
	snprintf(buffer, sizeof buffer, "Response Code=%d", (int)response_code);
	*response_string = buffer;

	if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url) != CURLE_OK)
		return -1;

	if (! (200 <= response_code && response_code < 300))
	{
		char *url_dup = pstrdup(file->url);
		char *effective_url_dup = effective_url == NULL ? "" : pstrdup(effective_url);
		if (response_code == 0)
		{
			long 		oserrno = 0;
			static char	connmsg[64];
			
			/* get the os level errno, and string representation of it */
			if (curl_easy_getinfo(curl, CURLINFO_OS_ERRNO, &oserrno) == CURLE_OK)
			{
				if (oserrno != 0)
					snprintf(connmsg, sizeof connmsg, "error code = %d (%s)",
							 (int) oserrno, strerror((int)oserrno));
			}

			if (do_close)
				url_fclose(file, false, "");

			ereport(ERROR, (errcode(ERRCODE_CONNECTION_FAILURE),
							errmsg("connection with gpfdist failed for %s. "
									"effective url: %s. %s\n%s", url_dup, effective_url_dup,
									(oserrno != 0 ? connmsg : ""),
									(curl_Error_Buffer[0] != '\0' ? curl_Error_Buffer : ""))));
		}
		else if (response_code == FDIST_TIMEOUT)	// gpfdist server return timeout code
		{
			return FDIST_TIMEOUT;
		}
		else
		{
			const char *http_response = pstrdup(file->u.curl.http_response);

			/* we need to sleep 1 sec to avoid this condition:
			   1- seg X gets an error message from gpfdist
			   2- seg Y gets a 500 error
			   3- seg Y report error before seg X, and error message
			   in seg X is thrown away.
			*/
			pg_usleep(1000000);
			if (do_close)
				url_fclose(file, false, "");

			elog(ERROR, "http response code %ld from gpfdist (%s): %s",
				 response_code, url_dup,
				 http_response ? http_response : "?");
		}
		pfree(url_dup);
		pfree(effective_url_dup);
	}

	return 0;
}

// callback for request /gpfdist/status for debugging purpose.
static size_t
log_http_body(char *buffer, size_t size, size_t nitems, void *userp)
{
	char body[256] = {0};
	int  nbytes = size * nitems;
	int  len = sizeof(body) - 1 > nbytes ? nbytes : sizeof(body) - 1;

	memcpy(body, buffer, len);

	elog(LOG, "gpfdist/status: %s", body);

	return nbytes;
}

// GET /gpfdist/status to get gpfdist status.
static void
get_gpfdist_status(URL_FILE *file)
{
	CURL * status_handle = NULL;
	char status_url[256];
	char domain[HOST_NAME_SIZE] = {0};
	CURLcode e;

	if (file == NULL)
	{
		elog(LOG, "get_gpfdist_status: file = NULL");
		return;
	}

	if (file->url == NULL)
	{
		elog(LOG, "get_gpfdist_status: file->url = NULL");
		return;
	}

	extract_http_domain(file->url, domain, HOST_NAME_SIZE);
	snprintf(status_url, sizeof(status_url), "http://%s/gpfdist/status", domain);

	do
	{
		if (! (status_handle = curl_easy_init()))
		{
		    elog(LOG, "internal error: get_gpfdist_status.curl_easy_init failed");
			break;
		}
		if (CURLE_OK != (e = curl_easy_setopt(status_handle, CURLOPT_TIMEOUT, 60L)))
		{
		    elog(LOG, "internal error: get_gpfdist_status.curl_easy_setopt CURLOPT_TIMEOUT error (%d - %s)",
		         e, curl_easy_strerror(e));
			break;
		}
		if (CURLE_OK != (e = curl_easy_setopt(status_handle, CURLOPT_URL, status_url)))
		{
		    elog(LOG, "internal error: get_gpfdist_status.curl_easy_setopt CURLOPT_URL error (%d - %s)",
		         e, curl_easy_strerror(e));
			break;
		}
		if (CURLE_OK != (e = curl_easy_setopt(status_handle, CURLOPT_WRITEFUNCTION, log_http_body)))
		{
			elog(LOG, "internal error: get_gpfdist_status.curl_easy_setopt CURLOPT_WRITEFUNCTION error (%d - %s)",
				 e, curl_easy_strerror(e));
			break;
		}
		if (CURLE_OK != (e = curl_easy_perform(status_handle)))
		{
			elog(LOG, "send status request failed: %s", curl_easy_strerror(e));
		}
	} while (0);

	curl_easy_cleanup(status_handle);
}

/**
 * Send curl request and check response.
 * If failed, will retry multiple times.
 * Return true if succeed, false otherwise.
 */
static bool
gp_curl_easy_perform_backoff_and_check_response(URL_FILE *file)
{
	int 		response_code;
	const char*	response_string;

	/* retry in case server return timeout error */
	unsigned int wait_time = 1;
	unsigned int retry_count = 0;
	/* retry at most twice(300 seconds * 2) when CURLE_OPERATION_TIMEDOUT happens */
	unsigned int timeout_count = 0;

	while (true)
	{
		/*
		 * Use backoff policy to call curl_easy_perform to fix following error
		 * when work load is high:
		 *	- 'could not connect to server'
		 *	- gpfdist return timeout (HTTP 408)
		 * By default it will wait at most 127 seconds before abort.
		 * 1 + 2 + 4 + 8 + 16 + 32 + 64 = 127
		 */
		CURLcode e = curl_easy_perform(file->u.curl.handle);
		if (CURLE_OK != e)
		{
			elog(WARNING, "%s error (%d - %s)", file->u.curl.curl_url, e, curl_easy_strerror(e));
			if (CURLE_OPERATION_TIMEDOUT == e)
			{
				timeout_count++;
			}
		}
		else
		{
			/* check the response from server */                                              	
			response_code = check_response(file, &response_code, &response_string, false);
			switch (response_code)
			{
				case 0:
					return true;
				case FDIST_TIMEOUT:
					break;
				default:
					elog(WARNING, "error while getting response from gpfdist on %s (code %d, msg %s)",
							file->u.curl.curl_url, response_code, response_string);   	
					return false;
			}
		}
                                                                            	
		if (wait_time > MAX_TRY_WAIT_TIME || timeout_count >= 2)
		{
			elog(WARNING, "quit after %d tries", retry_count+1);
			return false;
		}
		else
		{
			elog(WARNING, "failed to send request to gpfdist (%s), will retry after %d seconds", file->u.curl.curl_url, wait_time);
			unsigned int for_wait = 0;
			while (for_wait++ < wait_time)
			{
				pg_usleep(1000000);
				CHECK_FOR_INTERRUPTS();
			}
			wait_time = wait_time + wait_time;
			retry_count++;
			continue;
		}
	}

	/* not supposed to be here*/
	return false;
}


/*
 * fill_buffer
 *
 * Attempt to fill the read buffer up to requested number of bytes.
 * We first check if we already have the number of bytes that we
 * want already in the buffer (from write_callback), and we do
 * a select on the socket only if we don't have enough.
 *
 * return 0 if successful; raises ERROR otherwise.
 */
static int
fill_buffer(URL_FILE *file, int want)
{
	fd_set 	fdread;
	fd_set 	fdwrite;
	fd_set 	fdexcep;
	int 	maxfd = 0;
	struct 	timeval timeout;
	int 	nfds = 0, e = 0;
	int     timeout_count = 0;
	curlctl_t* curl = &file->u.curl;

	/* elog(NOTICE, "= still_running %d, bot %d, top %d, want %d",
	   file->u.curl.still_running, curl->in.bot, curl->in.top, want);
	*/

	/* attempt to fill buffer */
	while (curl->still_running && curl->in.top - curl->in.bot < want)
	{
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		CHECK_FOR_INTERRUPTS();

		/* set a suitable timeout to fail on */
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		/* get file descriptors from the transfers */
		if (0 != (e = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd)))
		{
			elog(ERROR, "internal error: curl_multi_fdset failed (%d - %s)",
						e, curl_easy_strerror(e));
		}

		if (maxfd <= 0)
		{
			elog(LOG, "curl_multi_fdset set maxfd = %d", maxfd);
			curl->still_running = 0;
			break;
		}
		nfds = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

		if (nfds == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
			{
				elog(DEBUG2, "select failed on curl_multi_fdset (maxfd %d) (%d - %s)", maxfd, errno, strerror(errno));
				continue;
			}
			elog(ERROR, "internal error: select failed on curl_multi_fdset (maxfd %d) (%d - %s)",
				 maxfd, errno, strerror(errno));
		}
		else if (nfds == 0)
		{
			// timeout
			timeout_count++;

			if (timeout_count % 12 == 0)
			{
				elog(LOG, "segment has not received data from gpfdist for about 1 minute, waiting for %d bytes.",
					 (want - (curl->in.top - curl->in.bot)));
			}

			if (readable_external_table_timeout != 0 && timeout_count * 5 > readable_external_table_timeout)
			{
				elog(LOG, "bot = %d, top = %d, want = %d, maxfd = %d, nfds = %d, e = %d, "
						  "still_running = %d, for_write = %d, error = %d, eof = %d, datalen = %d",
						  curl->in.bot, curl->in.top, want, maxfd, nfds, e, curl->still_running,
						  curl->for_write, curl->error, curl->eof, curl->block.datalen);
				get_gpfdist_status(file);
				elog(ERROR, "segment has not received data from gpfdist for long time, cancelling the query.");
				break;
			}
		}
		else if (nfds > 0)
		{
			/* timeout or readable/writable sockets */
			/* note we *could* be more efficient and not wait for
			 * CURLM_CALL_MULTI_PERFORM to clear here and check it on re-entry
			 * but that gets messy */
			while (CURLM_CALL_MULTI_PERFORM ==
				   (e = curl_multi_perform(multi_handle, &file->u.curl.still_running)));

			if (e != 0)
			{
				elog(ERROR, "internal error: curl_multi_perform failed (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}
		else
		{
			elog(ERROR, "select return unexpected result");
		}

		/* elog(NOTICE, "- still_running %d, bot %d, top %d, want %d",
		   file->u.curl.still_running, curl->in.bot, curl->in.top, want);
		*/
	}

	if (curl->still_running == 0)
	{
		elog(LOG, "quit fill_buffer due to still_running = 0, bot = %d, top = %d, want = %d, "
				"for_write = %d, error = %d, eof = %d, datalen = %d, maxfd = %d, nfds = %d, e = %d",
				curl->in.bot, curl->in.top, want, curl->for_write, curl->error,
				curl->eof, curl->block.datalen, maxfd, nfds, e);
	}


	return 0;
}


static int
set_httpheader(URL_FILE *fcurl, const char *name, const char *value)
{
	char tmp[1024];

	if (strlen(name) + strlen(value) + 5 > sizeof(tmp))
	{
		elog(WARNING, "set_httpheader name/value is too long. name = %s, value=%s", name, value);
		return -1;
	}

	sprintf(tmp, "%s: %s", name, value);
	fcurl->u.curl.x_httpheader = curl_slist_append(fcurl->u.curl.x_httpheader, tmp);
	if (fcurl->u.curl.x_httpheader == NULL)
		return -1;

	return 0;
}



static int
replace_httpheader(URL_FILE *fcurl, const char *name, const char *value)
{	
	char tmp[1024];
	if (strlen(name) + strlen(value) + 5 > sizeof(tmp))
	{
		elog(WARNING, "replace_httpheader name/value is too long. name = %s, value=%s", name, value);
		return -1;
	}
	sprintf(tmp, "%s: %s", name, value);

	struct curl_slist *p = fcurl->u.curl.x_httpheader;	
	while (p != NULL)
	{
		if (!strncmp(name, p->data, strlen(name)))
		{
			char *dupdata = strdup(tmp);			
			if (dupdata == NULL)
			{
				elog(WARNING, "replace_httpheader duplicate string name/value failed. name = %s, value=%s", name, value);
				return -1;
			}
			else
			{
				free(p->data);
				p->data = dupdata;
			}	
			return 0;
		}
		p = p->next;
	}	

	fcurl->u.curl.x_httpheader = curl_slist_append(fcurl->u.curl.x_httpheader, tmp);
	if (fcurl->u.curl.x_httpheader == NULL)
		return -1;

	return 0;
}

static char *
local_strstr(const char *str1, const char *str2)
{	
	char *cp = (char *) str1;
	char *s1, *s2;

	if ( !*str2 )
		return((char *)str1);

	while (*cp)
    {
		s1 = cp;
		s2 = (char *) str2;

		while (*s1 && (*s1==*s2))
			s1++, s2++;

		if (!*s2)
			return(cp);

		cp++;
	}

	return(NULL);
}

/*
 * This function purpose is to make sure that the URL string contains a
 * numerical IP address.  The input URL is in the parameter url. The output
 * result URL is in the output parameter - buf.  When parameter - url already
 * contains a numerical ip, then output parameter - buf will be a copy of url.
 * For this case calling getDnsAddress method inside make_url, will serve the
 * purpose of IP validation.  But when parameter - url will contain a domain
 * name, then the domain name substring will be changed to a numerical ip
 * address in the buf output parameter.
 *
 * Returns the length of the converted URL string, excluding null-terminator.
 */
static int
make_url(const char *url, char *buf, bool is_ipv6)
{
	char *authority_start = local_strstr(url, "//");
	char *authority_end;
	char *hostname_start;
	char *hostname_end;
	char hostname[HOST_NAME_SIZE];
	char *hostip = NULL;
	char portstr[9];
	int len;
	char *p;
	int port = 80; /* default for http */
	bool  domain_resolved_to_ipv6 = false;

	if (!authority_start)
		elog(ERROR, "illegal url '%s'", url);

	authority_start += 2;
	authority_end = strchr(authority_start, '/');
	if (!authority_end)
		authority_end = authority_start + strlen(authority_start);

	hostname_start = strchr(authority_start, '@');
	if (!(hostname_start && hostname_start < authority_end))
		hostname_start = authority_start;

	if (is_ipv6) /* IPV6 */
	{
		int len;

		hostname_end = strchr(hostname_start, ']');
		if (hostname_end == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
					 errmsg("unexpected IPv6 format %s", url)));
		hostname_end += 1;

		if (hostname_end[0] == ':')
		{
			/* port number exists in this url. get it */
			len = authority_end - hostname_end;
			if (len > 8)
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("<port> substring size must not exceed %d characters", 8)));

			memcpy(portstr, hostname_end + 1, len);
			portstr[len] = '\0';
			port = atoi(portstr);
		}

		/* skippping the brackets */
		hostname_end -= 1;
		hostname_start += 1;
	}
	else
	{
		hostname_end = strchr(hostname_start, ':');
		if (!(hostname_end && hostname_end < authority_end))
		{
			hostname_end = authority_end;
		}
		else
		{
			/* port number exists in this url. get it */
			int len = authority_end - hostname_end;
			if (len > 8)
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("<port> substring size must not exceed %d characters", 8)));

			memcpy(portstr, hostname_end + 1, len);
			portstr[len] = '\0';
			port = atoi(portstr);
		}
	}

	if (!port)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("<port> substring must contain only digits")));	

	if (hostname_end - hostname_start >= sizeof(hostname))
		elog(ERROR, "hostname too long for url '%s'", url);

	memcpy(hostname, hostname_start, hostname_end - hostname_start);
	hostname[hostname_end - hostname_start] = 0;

	hostip = getDnsAddress(hostname, port, ERROR);

	/*
	 * test for the case where the URL originaly contained a domain name
	 * (so is_ipv6 was set to false) but the DNS resolution in getDnsAddress
	 * returned an IPv6 address so know we also have to put the square
	 * brackets [..] in the URL string.
	 */
	if (strchr(hostip, ':') != NULL && !is_ipv6)
		domain_resolved_to_ipv6 = true;

	if (!buf)
	{
		int len = strlen(url) - strlen(hostname) + strlen(hostip);
		if (domain_resolved_to_ipv6)
			len += 2; /* for the square brackets */
		return len;
	}

	p = buf;
	len = hostname_start - url;
	strncpy(p, url, len);
	p += len;
	url += len;

	len = strlen(hostname);
	url += len;

	len = strlen(hostip);
	if (domain_resolved_to_ipv6)
	{
		*p = '[';
		p++;
	}
	strncpy(p, hostip, len);
	p += len;
	if (domain_resolved_to_ipv6)
	{
		*p = ']';
		p++;
	}

	strcpy(p, url);
	p += strlen(url);

	return p - buf;
}

/*
 * extract_http_domain
 *
 * extracts the domain string from a http url
 */
static void
extract_http_domain(char *i_path, char *o_domain, int dlen)
{
	int domsz, cpsz;
	char* p_st = (char*)local_strstr(i_path, "//");
	p_st = p_st + 2;
	char* p_en = strchr(p_st, '/');
	
	domsz = p_en - p_st;
	cpsz = ( domsz < dlen ) ? domsz : dlen;
	memcpy(o_domain, p_st, cpsz);
}

static bool
url_has_ipv6_format (char *url)
{
	bool is6 = false;
	char *ipv6 = local_strstr(url, "://[");
	
	if ( ipv6 )
		ipv6 = strchr(ipv6, ']');
	if ( ipv6 )
		is6 = true;
		
	return is6;
}

static int
is_file_exists(const char* filename)
{
	FILE* file;
    if ((file = fopen(filename, "r")) > 0)
    {
        fclose(file);
        return 1;
    }
    return 0;
}

URL_FILE *
url_curl_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate, int *response_code, const char **response_string)
{
	URL_FILE* file = alloc_url_file(url);
	int			sz;
	int         ip_mode;
	int 		e;
	bool		is_ipv6 = url_has_ipv6_format(file->url);
	/* Reset curl_Error_Buffer */
	curl_Error_Buffer[0] = '\0';

	Assert (IS_HTTP_URI(url) || IS_GPFDIST_URI(url) || IS_GPFDISTS_URI(url));

	sz = make_url(file->url, NULL, is_ipv6);

	file->type = CFTYPE_CURL; /* marked as URL */

	if (sz < 0)
	{
		const char *url_cpy = pstrdup(file->url);
			
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "illegal URL: %s", url_cpy);
	}

	if (!IS_GPFDISTS_URI(url))
	{
		file->u.curl.curl_url = (char *)calloc(sz + 1, 1);
		if (file->u.curl.curl_url == NULL)
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "out of memory");
		}
		file->u.curl.for_write = forwrite;
			
		make_url(file->url, file->u.curl.curl_url, is_ipv6);
		/*
		 * We need to call is_url_ipv6 for the case where inside make_url function
		 * a domain name was transformed to an IPv6 address.
		 */
		if ( !is_ipv6 )
			is_ipv6 = url_has_ipv6_format(file->u.curl.curl_url);
	}
	else
	{
		/*
		 * SSL support addition
		 *
		 * negotiation will fail if verifyhost is on, so we *must*
		 * not resolve the hostname in this case. I have decided
		 * to not resolve it anyway and let libcurl do the work.
		 */
		char* tmp_resolved;
		int url_len = strlen(file->url);
		file->u.curl.curl_url = (char*)calloc(url_len + 1, 1);
		if (file->u.curl.curl_url == NULL)
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "out of memory");
		}
		memcpy(file->u.curl.curl_url, file->url, url_len);
		file->u.curl.for_write = forwrite;

		tmp_resolved = palloc0(sz + 1);
		make_url(file->url, tmp_resolved, is_ipv6);
			
		/* keep the same ipv6 logic here */
		if ( !is_ipv6 )
			is_ipv6 = url_has_ipv6_format(tmp_resolved);
	}
		
	if (IS_GPFDIST_URI(file->u.curl.curl_url) || IS_GPFDISTS_URI(file->u.curl.curl_url))
	{
		/* replace gpfdist:// with http:// or gpfdists:// with https://
		 * by overriding 'dist' with 'http' */
		unsigned int tmp_len = strlen(file->u.curl.curl_url) + 1;	
		memmove(file->u.curl.curl_url, file->u.curl.curl_url + 3, tmp_len - 3);
		memcpy(file->u.curl.curl_url, "http", 4);
		pstate->header_line = 0;
	}

	/* initialize a curl session and get a libcurl handle for it */
	elog(LOG, "curl_easy_init: %s", file->url);
	if (! (file->u.curl.handle = curl_easy_init()))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_init failed");
	}
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_URL, file->u.curl.curl_url)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_URL error (%d - %s)",
			 e, curl_easy_strerror(e));
	}
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_VERBOSE, 0L /* FALSE */)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_VERBOSE error (%d - %s)",
			 e, curl_easy_strerror(e));
	}
	/* set callback for each header received from server */
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_HEADERFUNCTION, header_callback)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_HEADERFUNCTION error (%d - %s)",
			 e, curl_easy_strerror(e));
	}
	/* 'file' is the application variable that gets passed to header_callback */
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_WRITEHEADER, file)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_WRITEHEADER error (%d - %s)",
			 e, curl_easy_strerror(e));
	}
	/* set callback for each data block arriving from server to be written to application */
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_WRITEFUNCTION, write_callback)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_WRITEFUNCTION error (%d - %s)",
			 e, curl_easy_strerror(e));
	}
	/* 'file' is the application variable that gets passed to write_callback */
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_WRITEDATA, file)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_WRITEDATA error (%d - %s)",
			 e, curl_easy_strerror(e));
	}

	if ( !is_ipv6 )
		ip_mode = CURL_IPRESOLVE_V4;
	else
		ip_mode = CURL_IPRESOLVE_V6;
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_IPRESOLVE, ip_mode)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_IPRESOLVE error (%d - %s)",
			 e, curl_easy_strerror(e));
	}
		
	/*
	 * set up a linked list of http headers. start with common headers
	 * needed for read and write operations, and continue below with
	 * more specifics
	 */			
	file->u.curl.x_httpheader = NULL;
		
	/*
	 * support multihomed http use cases. see MPP-11874
	 */
	if (IS_HTTP_URI(url))
	{
		char domain[HOST_NAME_SIZE] = {0};
		extract_http_domain(file->url, domain, HOST_NAME_SIZE);
		if (set_httpheader(file, "Host", domain))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: some header value is too long");
		}
	}

	if (set_httpheader(file, "X-GP-XID", ev->GP_XID) ||
		set_httpheader(file, "X-GP-CID", ev->GP_CID) ||
		set_httpheader(file, "X-GP-SN", ev->GP_SN) ||
		set_httpheader(file, "X-GP-SEGMENT-ID", ev->GP_SEGMENT_ID) ||
		set_httpheader(file, "X-GP-SEGMENT-COUNT", ev->GP_SEGMENT_COUNT) ||
		set_httpheader(file, "X-GP-LINE-DELIM-STR", ev->GP_LINE_DELIM_STR) ||
		set_httpheader(file, "X-GP-LINE-DELIM-LENGTH", ev->GP_LINE_DELIM_LENGTH))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: some header value is too long");
	}

	if(forwrite)
	{
		// TIMEOUT for POST only, GET is single HTTP request,
		// probablity take long time.
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_TIMEOUT, 300L)))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_TIMEOUT error (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		/*init sequence number*/
		file->seq_number = 1;

		/* write specific headers */
		if(set_httpheader(file, "X-GP-PROTO", "0") ||
		   set_httpheader(file, "X-GP-SEQ", "1") ||
		   set_httpheader(file, "Content-Type", "text/xml"))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: some header value is too long");
		}
	}
	else
	{
		/* read specific - (TODO: unclear why some of these are needed) */
		if(set_httpheader(file, "X-GP-PROTO", "1") ||
		   set_httpheader(file, "X-GP-MASTER_HOST", ev->GP_MASTER_HOST) ||
		   set_httpheader(file, "X-GP-MASTER_PORT", ev->GP_MASTER_PORT) ||
		   set_httpheader(file, "X-GP-CSVOPT", ev->GP_CSVOPT) ||
		   set_httpheader(file, "X-GP_SEG_PG_CONF", ev->GP_SEG_PG_CONF) ||
		   set_httpheader(file, "X-GP_SEG_DATADIR", ev->GP_SEG_DATADIR) ||
		   set_httpheader(file, "X-GP-DATABASE", ev->GP_DATABASE) ||
		   set_httpheader(file, "X-GP-USER", ev->GP_USER) ||
		   set_httpheader(file, "X-GP-SEG-PORT", ev->GP_SEG_PORT) ||
		   set_httpheader(file, "X-GP-SESSION-ID", ev->GP_SESSION_ID))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: some header value is too long");
		}
	}
		
	{
		/*
		 * MPP-13031
		 * copy #transform fragment, if present, into X-GP-TRANSFORM header
		 */
		char* p = local_strstr(file->url, "#transform=");
		if (p && p[11])
		{
			if (set_httpheader(file, "X-GP-TRANSFORM", p+11))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "internal error: X-GP-TRANSFORM header value is too long");
			}
		}
	}

	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_HTTPHEADER, file->u.curl.x_httpheader)))
	{
		url_fclose(file, false, pstate->cur_relname);
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_HTTPHEADER error (%d - %s)",
			 e, curl_easy_strerror(e));
	}

	if (!multi_handle)
	{
		if (! (multi_handle = curl_multi_init()))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_multi_init failed");
		}
	}

	/*
	 * SSL configuration
	 */
	if (IS_GPFDISTS_URI(url))
	{
		Insist(PointerIsValid(DataDir));
		elog(LOG,"trying to load certificates from %s", DataDir);

		/* curl will save its last error in curlErrorBuffer */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_ERRORBUFFER, curl_Error_Buffer)))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_ERRORBUFFER error (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		/* cert is stored PEM coded in file... */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSLCERTTYPE, "PEM")))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSLCERTTYPE error (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		/* set the cert for client authentication */
		if (extssl_cert != NULL)
		{
			memset(extssl_cer_full, 0, MAXPGPATH);
			snprintf(extssl_cer_full, MAXPGPATH, "%s/%s", DataDir, extssl_cert);

			if (!is_file_exists(extssl_cer_full))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "file %s doesn't exists", extssl_cer_full);
			}

			if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSLCERT, extssl_cer_full)))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSLKEY error (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}

		/* set the key passphrase */
		if (extssl_pass != NULL)
		{
			if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_KEYPASSWD, extssl_pass)))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "internal error: curl_easy_setopt CURLOPT_KEYPASSWD error (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}

		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSLKEYTYPE,"PEM")))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSLKEYTYPE error (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		/* set the private key (file or ID in engine) */
		if (extssl_key != NULL)
		{
			memset(extssl_key_full, 0, MAXPGPATH);
			snprintf(extssl_key_full, MAXPGPATH, "%s/%s", DataDir,extssl_key);

			if (!is_file_exists(extssl_key_full))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "file %s doesn't exists", extssl_cer_full);
			}

			if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSLKEY, extssl_key_full)))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSLKEY error (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}

		/* set the file with the certs vaildating the server */
		if (extssl_ca != NULL)
		{
			memset(extssl_cas_full, 0, MAXPGPATH);
			snprintf(extssl_cas_full, MAXPGPATH, "%s/%s", DataDir, extssl_ca);

			if (!is_file_exists(extssl_cas_full))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "file %s doesn't exists", extssl_cer_full);
			}

			if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_CAINFO, extssl_cas_full)))
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "internal error: curl_easy_setopt CURLOPT_CAINFO error (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}

		/* set cert verification */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSL_VERIFYPEER, (long)extssl_verifycert)))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSL_VERIFYPEER error (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		/* set host verification */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSL_VERIFYHOST, (long)extssl_verifyhost)))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSL_VERIFYHOST error (%d - %s)",
				 e, curl_easy_strerror(e));
		}
		/* set ciphersuite */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSL_CIPHER_LIST, extssl_cipher)))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSL_CIPHER_LIST error (%d - %s)",
				 e, curl_easy_strerror(e));
		}
		/* set protocol */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_SSLVERSION, extssl_protocol)))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_SSLVERSION error (%d - %s)",
				 e, curl_easy_strerror(e));
		}
		/* set debug */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_VERBOSE, (long)extssl_libcurldebug)))
		{
			if (extssl_libcurldebug)
			{
				elog(INFO, "internal error: curl_easy_setopt CURLOPT_VERBOSE error (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}
	}

	/*
	 * lets check our connection.
	 * start the fetch if we're SELECTing (GET request), or write an
	 * empty message if we're INSERTing (POST request)
	 */
	if (!forwrite)
	{
		if (CURLE_OK != (e = curl_multi_add_handle(multi_handle, file->u.curl.handle)))
		{
			if (CURLM_CALL_MULTI_PERFORM != e)
			{
				url_fclose(file, false, pstate->cur_relname);
				elog(ERROR, "internal error: curl_multi_add_handle failed (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}

		while (CURLM_CALL_MULTI_PERFORM ==
			   (e = curl_multi_perform(multi_handle, &file->u.curl.still_running)));

		if (e != CURLE_OK)
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_multi_perform failed (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		/* read some bytes to make sure the connection is established */
		fill_buffer(file, 1);

		/* check the connection for GET request*/
		if (check_response(file, response_code, response_string, true))
		{
			return NULL;
		}
	}
	else
	{
		/* use empty message */
		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_POSTFIELDS, "")))
		{
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_POSTFIELDS error (%d - %s)",
				 e, curl_easy_strerror(e));
		}

		if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_POSTFIELDSIZE, 0)))
		{
			const char *curl_url = pstrdup(file->u.curl.curl_url);
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "internal error: curl_easy_setopt CURLOPT_POSTFIELDSIZE %s error (%d - %s)",
				 curl_url,
				 e, curl_easy_strerror(e));
		}

		/* post away and check response, retry if failed (timeout or * connect error) */
		if (gp_curl_easy_perform_backoff_and_check_response(file))
		{
			file->seq_number++;
		}
		else
		{
			int64 seq_number = file->seq_number;
			const char *curl_url = pstrdup(file->u.curl.curl_url);
			url_fclose(file, false, pstate->cur_relname);
			elog(ERROR, "error when sending OPEN request (SEQ:" INT64_FORMAT ") to gpfdist %s. error (%d - %s)",
				 seq_number,
				 curl_url,
				 e, curl_easy_strerror(e));
			return NULL;
		}
	}

	return file;
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
url_curl_fclose(URL_FILE *file, bool failOnError, const char *relname)
{
	bool    retVal = true;
	StringInfoData sinfo;

	initStringInfo(&sinfo);

	/*
	 * if WET, send a final "I'm done" request from this segment.
	 */
	if (file->u.curl.for_write && file->u.curl.handle != NULL)
		retVal = gp_proto0_write_done(file);

	if (file->u.curl.x_httpheader)
	{
		curl_slist_free_all(file->u.curl.x_httpheader);
		file->u.curl.x_httpheader = NULL;
	}

	/* make sure the easy handle is not in the multi handle anymore */
	if (file->u.curl.handle)
	{
		/* Currently assuming that we will not have any writing curl handlers in the multi handle */
		if (!file->u.curl.for_write)
		{
			CURLMcode e = curl_multi_remove_handle(multi_handle, file->u.curl.handle);
			if (CURLM_OK != e)
				elog(WARNING, "internal error curl_multi_remove_handle (%d - %s)", e, curl_easy_strerror(e));
		}

		/* cleanup */
		elog(LOG, "curl_easy_cleanup: %s", file->url);
		curl_easy_cleanup(file->u.curl.handle);
		file->u.curl.handle = NULL;
	}

	/* free any allocated buffer space */
	if (file->u.curl.in.ptr)
	{
		free(file->u.curl.in.ptr);
		file->u.curl.in.ptr = NULL;
	}

	if (file->u.curl.curl_url)
	{
		free(file->u.curl.curl_url);
		file->u.curl.curl_url = NULL;
	}

	if (file->u.curl.out.ptr)
	{
		Assert(file->u.curl.for_write);
		pfree(file->u.curl.out.ptr);
		file->u.curl.out.ptr = NULL;
	}

	file->u.curl.gp_proto = 0;
	file->u.curl.error = file->u.curl.eof = 0;
	memset(&file->u.curl.in, 0, sizeof(file->u.curl.in));
	memset(&file->u.curl.block, 0, sizeof(file->u.curl.block));

	free(file);
	if (!retVal)
		elog(ERROR, "Error when closing writable external table");
}

bool
url_curl_feof(URL_FILE *file, int bytesread)
{
	return (file->u.curl.eof != 0);
}


bool
url_curl_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	return (file->u.curl.error != 0);
}

/*
 * gp_proto0_read
 *
 * get data from the server and handle it according to PROTO 0. In PROTO 0 we
 * expect the content of the file without any kind of meta info. Simple.
 */
static size_t
gp_proto0_read(char *buf, int bufsz, URL_FILE* file)
{
	int 		n = 0;
	curlctl_t* 	curl = &file->u.curl;

	fill_buffer(file, bufsz);

	/* check if there's data in the buffer - if not fill_buffer()
	 * either errored or EOF. For proto0, we cannot distinguish
	 * between error and EOF. */
	n = curl->in.top - curl->in.bot;
	if (n == 0 && !curl->still_running)
		curl->eof = 1;

	if (n > bufsz)
		n = bufsz;

	/* xfer data to caller */
	memcpy(buf, curl->in.ptr + curl->in.bot, n);
	curl->in.bot += n;

	return n;
}

/*
 * gp_proto1_read
 *
 * get data from the server and handle it according to PROTO 1. In this protocol
 * each data block is tagged by meta info like this:
 * byte 0: type (can be 'F'ilename, 'O'ffset, 'D'ata, 'E'rror, 'L'inenumber)
 * byte 1-4: length. # bytes of following data block. in network-order.
 * byte 5-X: the block itself.
 */
static size_t
gp_proto1_read(char *buf, int bufsz, URL_FILE *file, CopyState pstate, char *buf2)
{
	char type;
	int  n, len;
	curlctl_t* curl = &file->u.curl;

	/*
	 * Loop through and get all types of messages, until we get actual data,
	 * or until there's no more data. Then quit the loop to process it and
	 * return it.
	 */
	while (curl->block.datalen == 0 && !curl->eof)
	{
		/* need 5 bytes, 1 byte type + 4 bytes length */
		fill_buffer(file, 5);
		n = curl->in.top - curl->in.bot;

		if (n == 0)
		{
			elog(ERROR, "gpfdist error: server closed connection.\n");
			return -1;
		}

		if (n < 5)
		{
			elog(ERROR, "gpfdist error: incomplete packet - packet len %d\n", n);
			return -1;
		}

		/* read type */
		type = curl->in.ptr[curl->in.bot++];

		/* read len */
		memcpy(&len, &curl->in.ptr[curl->in.bot], 4);
		len = ntohl(len);		/* change order */
		curl->in.bot += 4;

		if (len < 0)
		{
			elog(ERROR, "gpfdist error: bad packet type %d len %d",
				 type, len);
			return -1;
		}

		/* Error */
		if (type == 'E')
		{
			fill_buffer(file, len);
			n = curl->in.top - curl->in.bot;

			if (n > len)
				n = len;

			if (n > 0)
			{
				/*
				 * cheat a little. swap last char and
				 * NUL-terminator. then print string (without last
				 * char) and print last char artificially
				 */
				char x = curl->in.ptr[curl->in.bot + n - 1];
				curl->in.ptr[curl->in.bot + n - 1] = 0;
				elog(ERROR, "gpfdist error - %s%c", &curl->in.ptr[curl->in.bot], x);

				return -1;
			}

			elog(ERROR, "gpfdist error: please check gpfdist log messages.");

			return -1;
		}

		/* Filename */
		if (type == 'F')
		{
			if (buf != buf2)
			{
				curl->in.bot -= 5;
				return 0;
			}
			if (len > 256)
			{
				elog(ERROR, "gpfdist error: filename too long (%d)", len);
				return -1;
			}
			if (-1 == fill_buffer(file, len))
			{
				elog(ERROR, "gpfdist error: stream ends suddenly");
				return -1;
			}

			/*
			 * If SREH is used we now update it with the actual file that the
			 * gpfdist server is reading. This is because SREH (or the client
			 * in general) doesn't know which file gpfdist is reading, since
			 * the original URL may include a wildcard or a directory listing.
			 */
			if (pstate->cdbsreh)
			{
				char fname[257];

				memcpy(fname, curl->in.ptr + curl->in.bot, len);
				fname[len] = 0;
				snprintf(pstate->cdbsreh->filename, sizeof pstate->cdbsreh->filename,"%s [%s]", pstate->filename, fname);
			}

			curl->in.bot += len;
			Assert(curl->in.bot <= curl->in.top);
			continue;
		}

		/* Offset */
		if (type == 'O')
		{
			if (len != 8)
			{
				elog(ERROR, "gpfdist error: offset not of length 8 (%d)", len);
				return -1;
			}
			if (-1 == fill_buffer(file, len))
			{
				elog(ERROR, "gpfdist error: stream ends suddenly");
				return -1;
			}

			curl->in.bot += 8;
			Assert(curl->in.bot <= curl->in.top);
			continue;
		}

		/* Line number */
		if (type == 'L')
		{
			int64 line_number;

			if (len != 8)
			{
				elog(ERROR, "gpfdist error: line number not of length 8 (%d)", len);
				return -1;
			}
			if (-1 == fill_buffer(file, len))
			{
				elog(ERROR, "gpfdist error: stream ends suddenly");
				return -1;
			}

			/*
			 * update the line number of the first line we're about to get from
			 * gpfdist. pstate will update the following lines when processing
			 * the data
			 */
			memcpy(&line_number, curl->in.ptr + curl->in.bot, len);
			line_number = local_ntohll(line_number);
			pstate->cur_lineno = line_number ? line_number - 1 : INT64_MIN;
			curl->in.bot += 8;
			Assert(curl->in.bot <= curl->in.top);
			continue;
		}

		/* Data */
		if (type == 'D')
		{
			curl->block.datalen = len;
			curl->eof = (len == 0);
			break;
		}

		elog(ERROR, "gpfdist error: unknown meta type %d", type);
		return -1;
	}

	/* read data block */
	if (bufsz > curl->block.datalen)
		bufsz = curl->block.datalen;

	fill_buffer(file, bufsz);
	n = curl->in.top - curl->in.bot;

	/* if gpfdist closed connection prematurely or died catch it here */
	if (n == 0 && !curl->eof)
	{
		curl->error = 1;
		
		if(!curl->still_running)
			elog(ERROR, "gpfdist server closed connection. \nThis is not a "
			     "GPDB defect.  \nThe root cause is an overload of the ETL host or "
			     "a temporary network glitch between the database and the ETL host "
			     "causing the connection between the gpfdist and database to disconnect.\n");
	}

	if (n > bufsz)
		n = bufsz;

	memcpy(buf, curl->in.ptr + curl->in.bot, n);

	curl->in.bot += n;
	curl->block.datalen -= n;
	return n;
}

/*
 * gp_proto0_write
 *
 * use curl to write data to a the remote gpfdist server. We use
 * a push model with a POST request.
 */
static void
gp_proto0_write(URL_FILE *file, CopyState pstate)
{
	curlctl_t*	curl = &file->u.curl;
	char*		buf = curl->out.ptr;
	int		nbytes = curl->out.top;
	int 		e;

	if (nbytes == 0)
		return;
	
	/* post binary data */
	if (CURLE_OK != (e = curl_easy_setopt(curl->handle, CURLOPT_POSTFIELDS, buf)))
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_POSTFIELDS error (%d - %s)",
			 e, curl_easy_strerror(e));

	 /* set the size of the postfields data */
	if (CURLE_OK != (e = curl_easy_setopt(curl->handle, CURLOPT_POSTFIELDSIZE, nbytes)))
		elog(ERROR, "internal error: curl_easy_setopt CURLOPT_POSTFIELDSIZE error (%d - %s)",
			 e, curl_easy_strerror(e));

	/* set sequence number */
	char seq[128] = {0};
	snprintf(seq, sizeof(seq), INT64_FORMAT, file->seq_number);
	if (replace_httpheader(file, "X-GP-SEQ", seq)) {
		elog(ERROR, "internal error: set X-GP-SEQ failed");
	}

	if (gp_curl_easy_perform_backoff_and_check_response(file))
	{
		file->seq_number++;
	} else {
		elog(ERROR, "error when writing data to gpfdist %s. error (%d - %s)",
			 file->u.curl.curl_url,
			 e, curl_easy_strerror(e));
	}
}


/*
 * Send an empty POST request, with an added X-GP-DONE header.
 */
static bool
gp_proto0_write_done(URL_FILE *file)
{
	int 	e;

	if (set_httpheader(file, "X-GP-DONE", "1")){
		elog(WARNING, "internal error: set X-GP-DONE header failed");
		return false;
	}

	/* use empty message */
	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_POSTFIELDS, "")))
	{
		elog(WARNING, "internal error: curl_easy_setopt CURLOPT_POSTFIELDS %s error (%d - %s)",
			 file->u.curl.curl_url,
			 e, curl_easy_strerror(e));
		return false;
	}

	if (CURLE_OK != (e = curl_easy_setopt(file->u.curl.handle, CURLOPT_POSTFIELDSIZE, 0)))
	{
		elog(WARNING, "internal error: curl_easy_setopt CURLOPT_POSTFIELDSIZE %s error (%d - %s)",
			 file->u.curl.curl_url,
			 e, curl_easy_strerror(e));
		return false;
	}

	/* post away! */
	if (!gp_curl_easy_perform_backoff_and_check_response(file))
	{
		return false;
	}
	
	return true;
}

static size_t
curl_fread(char *buf, int bufsz, URL_FILE* file, CopyState pstate)
{
	curlctl_t*	curl = &file->u.curl;
	char*		p = buf;
	char*		q = buf + bufsz;
	int 		n;
	const int 	gp_proto = curl->gp_proto;

	if (gp_proto != 0 && gp_proto != 1)
	{
		elog(ERROR, "unknown gp protocol %d", curl->gp_proto);
		return 0;
	}

	for (; p < q; p += n)
	{
		if (gp_proto == 0)
			n = gp_proto0_read(p, q - p, file);
		else
			n = gp_proto1_read(p, q - p, file, pstate, buf);

		if (n <= 0)
			break;
	}

	return p - buf;
}

static size_t
curl_fwrite(char *buf, int nbytes, URL_FILE* file, CopyState pstate)
{
	curlctl_t*	curl = &file->u.curl;

	if (curl->gp_proto != 0 && curl->gp_proto != 1)
	{
		elog(ERROR, "unknown gp protocol %d", curl->gp_proto);
		return 0;
	}

	/*
	 * allocate data buffer if not done already
	 */
	if (!curl->out.ptr)
	{
		const int bufsize = writable_external_table_bufsize * 1024 * sizeof(char);
		MemoryContext oldcontext = CurrentMemoryContext;
		
		MemoryContextSwitchTo(CurTransactionContext); /* TODO: is there a better cxt to use? */
		curl->out.ptr = (char *)palloc(bufsize);
		curl->out.max = bufsize;
		curl->out.bot = curl->out.top = 0;
		MemoryContextSwitchTo(oldcontext);
	}
	
	/*
	 * if buffer is full (current item can't fit) - write it out to
	 * the server. if item still doesn't fit after we emptied the
	 * buffer, make more room.
	 */
	if (curl->out.top + nbytes >= curl->out.max)
	{
		/* item doesn't fit */
		if (curl->out.top > 0)
		{
			/* write out existing data, empty the buffer */
			gp_proto0_write(file, pstate);
			curl->out.top = 0;
		}
		
		/* does it still not fit? enlarge buffer */
		if (curl->out.top + nbytes >= curl->out.max)
		{
			int 	n = nbytes + 1024;
			char*	newbuf;
			MemoryContext oldcontext = CurrentMemoryContext;

			MemoryContextSwitchTo(CurTransactionContext); /* TODO: is there a better cxt to use? */
			newbuf = repalloc(curl->out.ptr, n);
			MemoryContextSwitchTo(oldcontext);

			if (!newbuf)
				elog(ERROR, "out of memory (curl_fwrite)");

			curl->out.ptr = newbuf;
			curl->out.max = n;

			Assert(nbytes < curl->out.max);
		}
	}

	/* copy buffer into curl->buf */
	memcpy(curl->out.ptr + curl->out.top, buf, nbytes);
	curl->out.top += nbytes;
	
	return nbytes;
}

size_t
url_curl_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	/* get data (up to nmemb * size) from the http/gpfdist server */
	return curl_fread(ptr, size, file, pstate);
}

size_t
url_curl_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	/* write data to the gpfdist server via curl */
	return curl_fwrite(ptr, size, file, pstate);
}

/*
 * flush all remaining buffered data waiting to be written out to external source
 */
void
url_curl_fflush(URL_FILE *file, CopyState pstate)
{
	gp_proto0_write(file, pstate);
}



#else /* USE_CURL */


/* Dummy versions of all the url_curl_* functions, when built without libcurl. */

static void
curl_not_compiled_error(void)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("unsupported external table URI"),
			 errdetail("This functionality requires the server to be built with libcurl support."),
			 errhint("You need to rebuild the server using --with-libcurl.")));
}

URL_FILE *
url_curl_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate, int *response_code, const char **response_string)
{
	curl_not_compiled_error();
	return NULL; /* keep compiler quiet */
}
void
url_curl_fclose(URL_FILE *file, bool failOnError, const char *relname)
{
	curl_not_compiled_error();
}
bool url_curl_feof(URL_FILE *file, int bytesread)
{
	curl_not_compiled_error();
	return false; /* keep compiler quiet */
}
bool url_curl_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	curl_not_compiled_error();
	return false; /* keep compiler quiet */
}
size_t url_curl_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	curl_not_compiled_error();
	return 0; /* keep compiler quiet */
}
size_t url_curl_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	curl_not_compiled_error();
	return 0; /* keep compiler quiet */
}
void url_curl_fflush(URL_FILE *file, CopyState pstate)
{
	curl_not_compiled_error();
}

#endif /* USE_CURL */
