/*-------------------------------------------------------------------------
 *
 * url_curl.c
 *	  Core support for opening external relations via a URL with curl
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include <time.h>

#include "cdb/cdbsreh.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/guc.h"
#include "utils/resowner.h"
#include "utils/uri.h"
#ifdef USE_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif

/*
 * This struct encapsulates the libcurl resources that need to be explicitly
 * cleaned up on error. We use the resource owner mechanism to make sure
 * these are not leaked. When a ResourceOwner is released, our hook will
 * walk the list of open curlhandles, and releases any that were owned by
 * the released resource owner.
 */
typedef struct curlhandle_t
{
	CURL			*handle;		/* The curl handle */
#ifdef USE_ZSTD
	ZSTD_DCtx		*zstd_dctx;		/* The zstd decompression context */
	ZSTD_CCtx		*zstd_cctx;		/* The zstd compression context */
#endif
	struct curl_slist *x_httpheader;	/* list of headers */
	bool		in_multi_handle;	/* T, if the handle is in global
									 * multi_handle */

	ResourceOwner owner;	/* owner of this handle */
	struct curlhandle_t *next;
	struct curlhandle_t *prev;
} curlhandle_t;

/*
 * Private state for a web external table, implemented with libcurl.
 *
 * This struct encapsulates working state of an open curl-flavored external
 * table. This is allocated in a suitable MemoryContext, and will therefore
 * be freed automatically on abort.
 */
typedef struct
{
	URL_FILE	common;
	bool		for_write;		/* 'f' when we SELECT, 't' when we INSERT	 */

	curlhandle_t *curl;		/* resources tracked by resource owner */

	char	   *curl_url;

	int64		seq_number;

	struct
	{
		char	   *ptr;		/* palloc-ed buffer */
		int			max;
		int			bot,
					top;
	} in;

	struct
	{
		char	   	*ptr;		/* palloc-ed buffer */
		char		*cptr;
		int			max;
		int			bot,
					top;
	} out;

	int			still_running;	/* Is background url fetch still in progress */
	int			error,
				eof;			/* error & eof flags */
	int			gp_proto;
	
	int 			zstd;			/* if gpfdist zstd compress is enabled, it equals 1 */
	int 			lastsize;		/* Recording the compressed data size */
	
	char	   		*http_response;
	struct
	{
		int			datalen;	/* remaining datablock length */
	} block;

} URL_CURL_FILE;

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
 *  extssl_verifycert 	1
 *  extssl_verifyhost 	2
 *  extssl_no_verifycert 	0
 *  extssl_no_verifyhost 	0
 *  extssl_cert 		"gpfdists/client.crt"
 *  extssl_key 			"gpfdists/client.key"
 *  extssl_pass 		"?" 										
 *  extssl_crl 			NULL 									
 *  Misc Params
 *  extssl_libcurldebug 1 	
 */

const static int extssl_protocol  = CURL_SSLVERSION_TLSv1;
const static int extssl_verifycert = 1;
const static int extssl_verifyhost = 2;
const static int extssl_no_verifycert = 0;
const static int extssl_no_verifyhost = 0;
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

static void gp_proto0_write_done(URL_CURL_FILE *file);
static void extract_http_domain(char* i_path, char* o_domain, int dlen);
static char * make_url(const char *url, bool is_ipv6);
#ifdef USE_ZSTD
int decompress_zstd_data(ZSTD_DCtx* ctx, ZSTD_inBuffer* bin, ZSTD_outBuffer* bout);
#endif
/* we use a global one for convenience */
static CURLM *multi_handle = 0;

static int
fill_buffer(URL_CURL_FILE *curl, int want);

/*
 * A helper macro, to call curl_easy_setopt(), and ereport() if it fails.
 */
#define CURL_EASY_SETOPT(h, opt, val) \
	do { \
		int			e; \
\
		if ((e = curl_easy_setopt(h, opt, val)) != CURLE_OK) \
			elog(ERROR, "internal error: curl_easy_setopt \"%s\" error (%d - %s)", \
				 CppAsString(opt), e, curl_easy_strerror(e)); \
	} while(0)

/*
 * Linked list of open curl handles. These are allocated in TopMemoryContext,
 * and tracked by resource owners.
 */
static curlhandle_t *open_curl_handles;

static bool url_curl_resowner_callback_registered;


static curlhandle_t *
create_curlhandle(void)
{
	curlhandle_t *h;

	h = MemoryContextAlloc(TopMemoryContext, sizeof(curlhandle_t));
	h->handle = NULL;
	h->x_httpheader = NULL;
	h->in_multi_handle = false;

#ifdef USE_ZSTD
	h->zstd_dctx = NULL;
	h->zstd_cctx = NULL;
#endif

	h->owner = CurrentResourceOwner;
	h->prev = NULL;
	h->next = open_curl_handles;
	if (open_curl_handles)
		open_curl_handles->prev = h;
	open_curl_handles = h;

	return h;
}

static void
destroy_curlhandle(curlhandle_t *h)
{
	/* unlink from linked list first */
	if (h->prev)
		h->prev->next = h->next;
	else
		open_curl_handles = open_curl_handles->next;
	if (h->next)
		h->next->prev = h->prev;

	if (h->x_httpheader)
	{
		curl_slist_free_all(h->x_httpheader);
		h->x_httpheader = NULL;
	}

	if (h->handle)
	{
		/* If this handle was registered in the multi-handle, remove it */
		if (h->in_multi_handle)
		{
			CURLMcode e = curl_multi_remove_handle(multi_handle, h->handle);

			if (CURLM_OK != e)
				elog(LOG, "internal error curl_multi_remove_handle (%d - %s)", e, curl_easy_strerror(e));
			h->in_multi_handle = false;
		}

		/* cleanup */
		curl_easy_cleanup(h->handle);
		h->handle = NULL;
	}
#ifdef USE_ZSTD
	if (h->zstd_dctx) 
	{
		ZSTD_freeDCtx(h->zstd_dctx);
		h->zstd_dctx = NULL;
	}
	if (h->zstd_cctx) 
	{
		ZSTD_freeCCtx(h->zstd_cctx);
		h->zstd_cctx = NULL;
	}
#endif
	pfree(h);
}

/*
 * Close any open curl handles on abort.
 *
 * Note that this only releases the low-level curl objects, in the
 * curlhandle_t struct. The UTL_CURL_FILE struct itself is allocated
 * in a memory context, and will go away with the context.
 */
static void
url_curl_abort_callback(ResourceReleasePhase phase,
						bool isCommit,
						bool isTopLevel,
						void *arg)
{
	curlhandle_t *curr;
	curlhandle_t *next;

	if (phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	next = open_curl_handles;
	while (next)
	{
		curr = next;
		next = curr->next;

		if (curr->owner == CurrentResourceOwner)
		{
			if (isCommit)
				elog(LOG, "url_curl reference leak: %p still referenced", curr);

			destroy_curlhandle(curr);
		}
	}
}

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
    URL_CURL_FILE *url = (URL_CURL_FILE *) userp;
	char*		ptr = ptr_;
	int 		len = size * nmemb;
	int 		i;
	char 		buf[20];

	int proto_len = strlen("X-GP-PROTO"), zstd_len = strlen("X-GP-ZSTD");

	Assert(size == 1);

	/*
	 * parse the http response line (code and message) from
	 * the http header that we get. Basically it's the whole
	 * first line (e.g: "HTTP/1.0 400 time out"). We do this
	 * in order to capture any error message that comes from
	 * gpfdist, and later use it to report the error string in
	 * check_response() to the database user.
	 */
	if (url->http_response == 0)
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

			url->http_response = p;
		}
	}

	/*
	 * extract the GP-PROTO value from the HTTP header.
	 */
	if (len > proto_len && 0 == strncmp("X-GP-PROTO", ptr, proto_len))
	{
		ptr += proto_len;
		len -= proto_len;

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
			url->gp_proto = strtol(buf, 0, 0);
		}
	}

	if (len > zstd_len && 0 == strncmp("X-GP-ZSTD", ptr, zstd_len))
	{
		ptr += zstd_len;
		len -= zstd_len;

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
#ifdef USE_ZSTD
			url->zstd = strtol(buf, 0, 0);

			if (url->for_write && url->zstd)
			{	
				url->curl->zstd_cctx = ZSTD_createCCtx();
				// allocate out.cptr whose size equals to out.ptr
				url->out.cptr = (char *) palloc(writable_external_table_bufsize * 1024);
				url->lastsize = 0;
			}
			else if (url->zstd)
			{
				url->curl->zstd_dctx = ZSTD_createDCtx();
				url->lastsize = ZSTD_initDStream(url->curl->zstd_dctx);
			}
#endif
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
    URL_CURL_FILE *curl = (URL_CURL_FILE *) userp;
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

		/* if still insufficient space in buffer, enlarge it */
		if (curl->in.top + nbytes >= curl->in.max)
		{
			char *newbuf;

			n = curl->in.top - curl->in.bot + nbytes + 1024;
			newbuf = repalloc(curl->in.ptr, n);

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
check_response(URL_CURL_FILE *file, int *rc, char **response_string)
{
	long 		response_code;
	char*		effective_url = NULL;
	CURL* 		curl = file->curl->handle;
	char		buffer[30];

	/* get the response code from curl */
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code) != CURLE_OK)
	{
		*rc = 500;
		*response_string = pstrdup("curl_easy_getinfo failed");
		return -1;
	}
	*rc = response_code;
	snprintf(buffer, sizeof buffer, "Response Code=%d", (int)response_code);
	*response_string = pstrdup(buffer);

	if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url) != CURLE_OK)
		return -1;
	if (effective_url == NULL)
		effective_url = "";

	if (! (200 <= response_code && response_code < 300))
	{
		if (response_code == 0)
		{
			long 		oserrno = 0;
			static char	connmsg[64];

			/* get the os level errno, and string representation of it */
			if (curl_easy_getinfo(curl, CURLINFO_OS_ERRNO, &oserrno) == CURLE_OK)
			{
				if (oserrno == EHOSTUNREACH)
				{
					return oserrno;
				}
				if (oserrno != 0)
					snprintf(connmsg, sizeof connmsg, "error code = %d (%s)",
							 (int) oserrno, strerror((int)oserrno));
			}

			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("connection with gpfdist failed for \"%s\", effective url: \"%s\": %s; %s",
							file->common.url, effective_url,
							(oserrno != 0 ? connmsg : ""),
							(curl_Error_Buffer[0] != '\0' ? curl_Error_Buffer : ""))));
		}
		else if (response_code == FDIST_TIMEOUT)	// gpfdist server return timeout code
		{
			return FDIST_TIMEOUT;
		}
		else
		{
			/* we need to sleep 1 sec to avoid this condition:
			   1- seg X gets an error message from gpfdist
			   2- seg Y gets a 500 error
			   3- seg Y report error before seg X, and error message
			   in seg X is thrown away.
			*/
			pg_usleep(1000000);

			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("http response code %ld from gpfdist (%s): %s",
							response_code, file->common.url,
							file->http_response ? file->http_response : "?")));
		}
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
get_gpfdist_status(URL_CURL_FILE *file)
{
	CURL * status_handle = NULL;
	char status_url[256];
	char domain[HOST_NAME_SIZE] = {0};
	CURLcode e;

	extract_http_domain(file->common.url, domain, HOST_NAME_SIZE);
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

/* Return true to retry */
typedef bool (*perform_func)(URL_CURL_FILE *file);

static void
gp_perform_backoff_and_check_response(URL_CURL_FILE *file, perform_func perform)
{
	/* retry in case server return timeout error */
	unsigned int wait_time = 1;
	unsigned int retry_count = 0;
	/* retry at most 300s by default when any error happens */
	time_t start_time = time(NULL);
	time_t now;
	time_t end_time = start_time + gpfdist_retry_timeout;

	while (true)
	{
		if (!perform(file))
		{
			return;
		}
		/*
		 * Retry until end_time is reached
		 */
		now = time(NULL);
		if (now >= end_time)
		{
			elog(LOG, "abort writing data to gpfdist, wait_time = %d, duration = %ld, gpfdist_retry_timeout = %d",
				wait_time, now - start_time, gpfdist_retry_timeout);
			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("error when connecting to gpfdist %s, quit after %d tries",
							file->curl_url, retry_count+1)));
		}
		else
		{
			unsigned int for_wait = 0;
			wait_time = wait_time > MAX_TRY_WAIT_TIME ? MAX_TRY_WAIT_TIME : wait_time;
			/* For last retry before end_time */
			wait_time = wait_time > end_time - now ? end_time - now : wait_time;
			elog(LOG, "failed to send request to gpfdist (%s), will retry after %d seconds", file->curl_url, wait_time);
			while (for_wait++ < wait_time)
			{
				pg_usleep(1000000);
				CHECK_FOR_INTERRUPTS();
			}
			wait_time = wait_time + wait_time;
			retry_count++;
		}
	}
}

static bool multi_perform_work(URL_CURL_FILE *file)
{
	int 		response_code;
	char	   *response_string = NULL;
	int e;
	char *effective_url;

	if (CURLE_OK != (e = curl_multi_add_handle(multi_handle, file->curl->handle)))
	{
		if (CURLM_CALL_MULTI_PERFORM != e)
			elog(ERROR, "internal error: curl_multi_add_handle failed (%d - %s)",
				 e, curl_easy_strerror(e));
	}
	file->curl->in_multi_handle = true;

	while (CURLM_CALL_MULTI_PERFORM ==
		   (e = curl_multi_perform(multi_handle, &file->still_running)));
	if (e != CURLE_OK)
		elog(ERROR, "internal error: curl_multi_perform failed (%d - %s)",
			 e, curl_easy_strerror(e));
	/* read some bytes to make sure the connection is established */
	fill_buffer(file, 1);

	/* check the connection for GET request */
	int code = check_response(file, &response_code, &response_string);
	switch (code)
	{
		case 0:
			return false;
		case EHOSTUNREACH:
			curl_easy_getinfo(file->curl->handle, CURLINFO_EFFECTIVE_URL, &effective_url);
			elog(LOG, "gpfdist request failed on seg%d, error: %s, effective url %s",
				GpIdentity.segindex, strerror(EHOSTUNREACH), effective_url);
			curl_multi_remove_handle(multi_handle, file->curl->handle);
			curl_multi_cleanup(multi_handle);
			multi_handle = curl_multi_init();
			return true;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("could not open \"%s\" for reading", file->common.url),
					 errdetail("Unexpected response from gpfdist server: %d - %s",
							   response_code, response_string)));
	}
}

static bool easy_perform_work(URL_CURL_FILE *file)
{
	int 		response_code;
	char	   *response_string = NULL;
	/*
	 * Use backoff policy to call curl_easy_perform to fix following error
	 * when work load is high:
	 *	- 'could not connect to server'
	 *	- gpfdist return timeout (HTTP 408)
	 * By default it will wait at least gpfdist_retry_timeout seconds before abort.
	 */
	CURLcode e = curl_easy_perform(file->curl->handle);
	if (CURLE_OK != e)
	{
		elog(LOG, "%s response (%d - %s)", file->curl_url, e, curl_easy_strerror(e));
	}
	else
	{
		/* check the response from server */
		response_code = check_response(file, &response_code, &response_string);
		switch (response_code)
		{
			case 0:
				/* Success! */
				return false;
			case FDIST_TIMEOUT:
				elog(LOG, "%s timeout from gpfdist", file->curl_url);
				break;
			default:
				ereport(ERROR,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("error while getting response from gpfdist on %s (code %d, msg %s)",
								file->curl_url, response_code, response_string)));
		}
		if (response_string)
			pfree(response_string);
		response_string = NULL;
	}
	return true;
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
fill_buffer(URL_CURL_FILE *curl, int want)
{
	fd_set 	fdread;
	fd_set 	fdwrite;
	fd_set 	fdexcep;
	int 	maxfd = 0;
	struct 	timeval timeout;
	int 	nfds = 0, e = 0;
	int     timeout_count = 0;

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

		if (maxfd == 0)
		{
			elog(LOG, "curl_multi_fdset set maxfd = %d", maxfd);
			curl->still_running = 0;
			break;
		}
		/* When libcurl returns -1 in max_fd, it is because libcurl currently does something
		 * that isn't possible for your application to monitor with a socket and unfortunately
		 * you can then not know exactly when the current action is completed using select().
		 * You then need to wait a while before you proceed and call curl_multi_perform anyway
		 */
		if (maxfd == -1)
		{
			elog(DEBUG2, "curl_multi_fdset set maxfd = %d", maxfd);
			pg_usleep(100000);
			// to call curl_multi_perform
			nfds = 1;
		}
		else
		{
			nfds = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
		}
		if (nfds == -1)
		{
			int save_errno = errno;
			if (errno == EINTR || errno == EAGAIN)
			{
				elog(DEBUG2, "select failed on curl_multi_fdset (maxfd %d) (%d - %s)", maxfd,
					 save_errno, strerror(save_errno));
				continue;
			}
			elog(ERROR, "internal error: select failed on curl_multi_fdset (maxfd %d) (%d - %s)",
				 maxfd, save_errno, strerror(save_errno));
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
				get_gpfdist_status(curl);
				ereport(ERROR,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("segment has not received data from gpfdist for long time, cancelling the query.")));
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
				   (e = curl_multi_perform(multi_handle, &curl->still_running)));

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


static void
set_httpheader(URL_CURL_FILE *fcurl, const char *name, const char *value)
{
	struct curl_slist *new_httpheader;
	char		tmp[1024];

	if (strlen(name) + strlen(value) + 5 > sizeof(tmp))
		elog(ERROR, "set_httpheader name/value is too long. name = %s, value=%s",
			 name, value);

	snprintf(tmp, sizeof(tmp), "%s: %s", name, value);

	new_httpheader = curl_slist_append(fcurl->curl->x_httpheader, tmp);
	if (new_httpheader == NULL)
		elog(ERROR, "could not set curl HTTP header \"%s\" to \"%s\"", name, value);

	fcurl->curl->x_httpheader = new_httpheader;
}

static void
replace_httpheader(URL_CURL_FILE *fcurl, const char *name, const char *value)
{
	struct curl_slist *new_httpheader;
	char		tmp[1024];

	if (strlen(name) + strlen(value) + 5 > sizeof(tmp))
		elog(ERROR, "replace_httpheader name/value is too long. name = %s, value=%s", name, value);

	sprintf(tmp, "%s: %s", name, value);

	/* Find existing header, if any */
	struct curl_slist *p = fcurl->curl->x_httpheader;
	while (p != NULL)
	{
		if (!strncmp(name, p->data, strlen(name)))
		{
			/*
			 * NOTE: p->data is not palloc'd! It is originally allocated
			 * by curl_slist_append, so use plain malloc/free here as well.
			 */
			char	   *dupdata = strdup(tmp);

			if (dupdata == NULL)
				elog(ERROR, "out of memory");

			free(p->data);
			p->data = dupdata;
			return;
		}
		p = p->next;
	}

	/* No existing header, add a new one */

	new_httpheader = curl_slist_append(fcurl->curl->x_httpheader, tmp);
	if (new_httpheader == NULL)
		elog(ERROR, "could not append HTTP header \"%s\"", name);
	fcurl->curl->x_httpheader = new_httpheader;
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
 * make_url
 *				Address resolve a URL to contain only IP number
 *
 * Resolve the hostname in the URL to an IP number, and return a new URL with
 * the same scheme and parameters using the resolved IP address. If the passed
 * url is using an IP number, the return value will be a copy of the input.
 * The output is a palloced string, it's the callers responsibility to free it
 * when no longer needed. This function will error out in case a URL cannot be
 * formed, NULL or an empty string are never returned.
 */
static char *
make_url(const char *url, bool is_ipv6)
{
	char *authority_start = local_strstr(url, "//");
	char *authority_end;
	char *hostname_start;
	char *hostname_end;
	char hostname[HOST_NAME_SIZE];
	char *hostip = NULL;
	char portstr[9];
	int port = 80; /* default for http */
	bool  domain_resolved_to_ipv6 = false;
	StringInfoData buf;

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
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("<port> substring size must not exceed 8 characters")));

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
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("<port> substring size must not exceed 8 characters")));

			memcpy(portstr, hostname_end + 1, len);
			portstr[len] = '\0';
			port = atoi(portstr);
		}
	}

	if (!port)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("<port> substring must contain only digits")));	

	if (hostname_end - hostname_start >= sizeof(hostname))
		elog(ERROR, "hostname too long for url '%s'", url);

	memcpy(hostname, hostname_start, hostname_end - hostname_start);
	hostname[hostname_end - hostname_start] = 0;

	hostip = getDnsAddress(hostname, port, ERROR);

	if (hostip == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_CONNECTION_FAILURE),
				 errmsg("hostname cannot be resolved '%s'", url)));

	/*
	 * test for the case where the URL originally contained a domain name
	 * (so is_ipv6 was set to false) but the DNS resolution in getDnsAddress
	 * returned an IPv6 address so know we also have to put the square
	 * brackets [..] in the URL string.
	 */
	if (strchr(hostip, ':') != NULL && !is_ipv6)
		domain_resolved_to_ipv6 = true;

	initStringInfo(&buf);

	for (int i = 0; i < (hostname_start - url); i++)
		appendStringInfoChar(&buf, *(url + i));
	if (domain_resolved_to_ipv6)
		appendStringInfoChar(&buf, '[');
	appendStringInfoString(&buf, hostip);
	if (domain_resolved_to_ipv6)
		appendStringInfoChar(&buf, ']');
	appendStringInfoString(&buf, url + (strlen(hostname) + (hostname_start - url)));

	return buf.data;
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
	file = fopen(filename, "r");
	if (file)
	{
		fclose(file);
		return 1;
	}
	return 0;
}

URL_FILE *
url_curl_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate)
{
	URL_CURL_FILE *file;
	int         ip_mode;
	int 		e;
	bool		is_ipv6 = url_has_ipv6_format(url);
	char	   *tmp;

	/* Reset curl_Error_Buffer */
	curl_Error_Buffer[0] = '\0';

	Assert(IS_HTTP_URI(url) || IS_GPFDIST_URI(url) || IS_GPFDISTS_URI(url));

	if (!url_curl_resowner_callback_registered)
	{
		RegisterResourceReleaseCallback(url_curl_abort_callback, NULL);
		url_curl_resowner_callback_registered = true;
	}

	tmp = make_url(url, is_ipv6);

	file = (URL_CURL_FILE *) palloc0(sizeof(URL_CURL_FILE));
	file->common.type = CFTYPE_CURL;
	file->common.url = pstrdup(url);
	file->for_write = forwrite;
	file->curl = create_curlhandle();

	/*
	 * We need to call is_url_ipv6 for the case where inside make_url
	 * function a domain name was transformed to an IPv6 address.
	 */
	if (!is_ipv6)
		is_ipv6 = url_has_ipv6_format(tmp);

	if (!IS_GPFDISTS_URI(url))
		file->curl_url = tmp;
	else
	{
		/*
		 * SSL support addition
		 *
		 * negotiation will fail if verifyhost is on, so we *must*
		 * not resolve the hostname in this case. I have decided
		 * to not resolve it anyway and let libcurl do the work.
		 */
		file->curl_url = pstrdup(file->common.url);
		pfree(tmp);
	}

	if (IS_GPFDIST_URI(file->curl_url) || IS_GPFDISTS_URI(file->curl_url))
	{
		/* replace gpfdist:// with http:// or gpfdists:// with https://
		 * by overriding 'dist' with 'http' */
		unsigned int tmp_len = strlen(file->curl_url) + 1;
		memmove(file->curl_url, file->curl_url + 3, tmp_len - 3);
		memcpy(file->curl_url, "http", 4);
		pstate->header_line = 0;
	}

	/* initialize a curl session and get a libcurl handle for it */
	if (! (file->curl->handle = curl_easy_init()))
		elog(ERROR, "internal error: curl_easy_init failed");

	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_URL, file->curl_url);

	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_VERBOSE, 0L /* FALSE */);

	/* set callback for each header received from server */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_HEADERFUNCTION, header_callback);

	/* 'file' is the application variable that gets passed to header_callback */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_WRITEHEADER, file);

	/* set callback for each data block arriving from server to be written to application */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_WRITEFUNCTION, write_callback);

	/* 'file' is the application variable that gets passed to write_callback */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_WRITEDATA, file);

	if (!is_ipv6)
		ip_mode = CURL_IPRESOLVE_V4;
	else
		ip_mode = CURL_IPRESOLVE_V6;
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_IPRESOLVE, ip_mode);

	/*
	 * set up a linked list of http headers. start with common headers
	 * needed for read and write operations, and continue below with
	 * more specifics
	 */			
	Assert(file->curl->x_httpheader == NULL);

	/*
	 * support multihomed http use cases. see MPP-11874
	 */
	if (IS_HTTP_URI(url))
	{
		char domain[HOST_NAME_SIZE] = {0};

		extract_http_domain(file->common.url, domain, HOST_NAME_SIZE);
		set_httpheader(file, "Host", domain);
	}

	set_httpheader(file, "X-GP-XID", ev->GP_XID);
	set_httpheader(file, "X-GP-CID", ev->GP_CID);
	set_httpheader(file, "X-GP-SN", ev->GP_SN);
	set_httpheader(file, "X-GP-SEGMENT-ID", ev->GP_SEGMENT_ID);
	set_httpheader(file, "X-GP-SEGMENT-COUNT", ev->GP_SEGMENT_COUNT);
	set_httpheader(file, "X-GP-LINE-DELIM-STR", ev->GP_LINE_DELIM_STR);
	set_httpheader(file, "X-GP-LINE-DELIM-LENGTH", ev->GP_LINE_DELIM_LENGTH);
#ifdef USE_ZSTD
		set_httpheader(file, "X-GP-ZSTD", "1");
#endif

	if (forwrite)
	{
		// TIMEOUT for POST only, GET is single HTTP request,
		// probablity take long time.
		elog(LOG, "gpfdist_retry_timeout = %d", gpfdist_retry_timeout);
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_TIMEOUT, (long)gpfdist_retry_timeout);

		/*init sequence number*/
		file->seq_number = 1;

		/* write specific headers */
		set_httpheader(file, "X-GP-PROTO", "0");
		set_httpheader(file, "X-GP-SEQ", "1");
		set_httpheader(file, "Content-Type", "text/xml");
	}
	else
	{
		/* read specific - (TODO: unclear why some of these are needed) */
		set_httpheader(file, "X-GP-PROTO", "1");
		set_httpheader(file, "X-GP-MASTER_HOST", ev->GP_MASTER_HOST);
		set_httpheader(file, "X-GP-MASTER_PORT", ev->GP_MASTER_PORT);
		set_httpheader(file, "X-GP-CSVOPT", ev->GP_CSVOPT);
		set_httpheader(file, "X-GP_SEG_PG_CONF", ev->GP_SEG_PG_CONF);
		set_httpheader(file, "X-GP_SEG_DATADIR", ev->GP_SEG_DATADIR);
		set_httpheader(file, "X-GP-DATABASE", ev->GP_DATABASE);
		set_httpheader(file, "X-GP-USER", ev->GP_USER);
		set_httpheader(file, "X-GP-SEG-PORT", ev->GP_SEG_PORT);
		set_httpheader(file, "X-GP-SESSION-ID", ev->GP_SESSION_ID);
	}
		
	{
		/*
		 * MPP-13031
		 * copy #transform fragment, if present, into X-GP-TRANSFORM header
		 */
		char* p = local_strstr(file->common.url, "#transform=");
		if (p && p[11])
			set_httpheader(file, "X-GP-TRANSFORM", p + 11);
	}

	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_HTTPHEADER, file->curl->x_httpheader);

	if (!multi_handle)
	{
		if (! (multi_handle = curl_multi_init()))
			elog(ERROR, "internal error: curl_multi_init failed");
	}

	/*
	 * SSL configuration
	 */
	if (IS_GPFDISTS_URI(url))
	{
		Assert(PointerIsValid(DataDir));
		elog(LOG,"trying to load certificates from %s", DataDir);

		/* curl will save its last error in curlErrorBuffer */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_ERRORBUFFER, curl_Error_Buffer);

		/* cert is stored PEM coded in file... */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSLCERTTYPE, "PEM");

		/* set the cert for client authentication */
		if (extssl_cert != NULL)
		{
			memset(extssl_cer_full, 0, MAXPGPATH);
			snprintf(extssl_cer_full, MAXPGPATH, "%s/%s", DataDir, extssl_cert);

			if (!is_file_exists(extssl_cer_full))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open certificate file \"%s\": %m",
								extssl_cer_full)));

			CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSLCERT, extssl_cer_full);
		}

		/* set the key passphrase */
		if (extssl_pass != NULL)
			CURL_EASY_SETOPT(file->curl->handle, CURLOPT_KEYPASSWD, extssl_pass);

		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSLKEYTYPE,"PEM");

		/* set the private key (file or ID in engine) */
		if (extssl_key != NULL)
		{
			memset(extssl_key_full, 0, MAXPGPATH);
			snprintf(extssl_key_full, MAXPGPATH, "%s/%s", DataDir, extssl_key);

			if (!is_file_exists(extssl_key_full))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open private key file \"%s\": %m",
								extssl_key_full)));

			CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSLKEY, extssl_key_full);
		}

		/* set the file with the CA certificates, for validating the server */
		if (extssl_ca != NULL)
		{
			memset(extssl_cas_full, 0, MAXPGPATH);
			snprintf(extssl_cas_full, MAXPGPATH, "%s/%s", DataDir, extssl_ca);

			if (!is_file_exists(extssl_cas_full))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open private key file \"%s\": %m",
								extssl_cas_full)));

			CURL_EASY_SETOPT(file->curl->handle, CURLOPT_CAINFO, extssl_cas_full);
		}

		/* set cert verification */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSL_VERIFYPEER,
				(long)(verify_gpfdists_cert ? extssl_verifycert : extssl_no_verifycert));

		/* set host verification */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSL_VERIFYHOST,
				(long)(verify_gpfdists_cert ? extssl_verifyhost : extssl_no_verifyhost));

		/* set protocol */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSLVERSION, extssl_protocol);

		/* disable session ID cache */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_SSL_SESSIONID_CACHE, 0);

		/* set debug */
		if (CURLE_OK != (e = curl_easy_setopt(file->curl->handle, CURLOPT_VERBOSE, (long)extssl_libcurldebug)))
		{
			if (extssl_libcurldebug)
			{
				elog(INFO, "internal error: curl_easy_setopt CURLOPT_VERBOSE error (%d - %s)",
					 e, curl_easy_strerror(e));
			}
		}
	}

	/* Allocate input and output buffers. */
	file->in.ptr = palloc(1024);		/* 1 kB buffer initially */
	file->in.max = 1024;
	file->in.bot = file->in.top = 0;

	if (forwrite)
	{
		int			bufsize = writable_external_table_bufsize * 1024;

		file->out.ptr = (char *) palloc(bufsize);
		file->out.max = bufsize;
		file->out.bot = file->out.top = 0;
	}

	/*
	 * lets check our connection.
	 * start the fetch if we're SELECTing (GET request), or write an
	 * empty message if we're INSERTing (POST request)
	 */
	if (!forwrite)
	{
		gp_perform_backoff_and_check_response(file, multi_perform_work);
	}
	else
	{
		/* use empty message */
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_POSTFIELDS, "");
		CURL_EASY_SETOPT(file->curl->handle, CURLOPT_POSTFIELDSIZE, 0);

		/* post away and check response, retry if failed (timeout or * connect error) */
		gp_perform_backoff_and_check_response(file, easy_perform_work);
		file->seq_number++;
	}

	return (URL_FILE *) file;
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
url_curl_fclose(URL_FILE *fileg, bool failOnError, const char *relname)
{
	URL_CURL_FILE *file = (URL_CURL_FILE *) fileg;
	StringInfoData sinfo;

	initStringInfo(&sinfo);

	/*
	 * if WET, send a final "I'm done" request from this segment.
	 */
	if (file->for_write && file->curl->handle != NULL)
		gp_proto0_write_done(file);

	destroy_curlhandle(file->curl);
	file->curl = NULL;

	/* free any allocated buffer space */
	if (file->in.ptr)
	{
		pfree(file->in.ptr);
		file->in.ptr = NULL;
	}

	if (file->curl_url)
	{
		pfree(file->curl_url);
		file->curl_url = NULL;
	}

	if (file->out.ptr)
	{
		Assert(file->for_write);
		pfree(file->out.ptr);
		file->out.ptr = NULL;
	}

	if (file->out.cptr)
	{
		Assert(file->for_write);
		pfree(file->out.cptr);
		file->out.cptr = NULL;
	}

	file->gp_proto = 0;
	file->error = file->eof = 0;
	memset(&file->in, 0, sizeof(file->in));
	memset(&file->block, 0, sizeof(file->block));

	pfree(file->common.url);

	pfree(file);
}

bool
url_curl_feof(URL_FILE *file, int bytesread)
{
	URL_CURL_FILE *cfile = (URL_CURL_FILE *) file;

	return (cfile->eof != 0);
}


bool
url_curl_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	URL_CURL_FILE *cfile = (URL_CURL_FILE *) file;

	return (cfile->error != 0);
}

/*
 * gp_proto0_read
 *
 * get data from the server and handle it according to PROTO 0. In PROTO 0 we
 * expect the content of the file without any kind of meta info. Simple.
 */
static size_t
gp_proto0_read(char *buf, int bufsz, URL_CURL_FILE *file)
{
	int 		n = 0;

	fill_buffer(file, bufsz);

	/* check if there's data in the buffer - if not fill_buffer()
	 * either errored or EOF. For proto0, we cannot distinguish
	 * between error and EOF. */
	n = file->in.top - file->in.bot;
	if (n == 0 && !file->still_running)
		file->eof = 1;

	if (n > bufsz)
		n = bufsz;

	/* xfer data to caller */
	memcpy(buf, file->in.ptr + file->in.bot, n);
	file->in.bot += n;

	return n;
}

#ifdef USE_ZSTD
int 
decompress_zstd_data(ZSTD_DCtx* ctx, ZSTD_inBuffer* bin, ZSTD_outBuffer* bout)
{
	
	size_t ret;
	/* Ret indicates the number of bytes of next data frame to be decompressed.
	 * And if an error occur in ZSTD_decompressStream, ret will be a error number.
	 * If ZSTD_isError is true, the ret is a error number.
	 * The content of the error can be got by ZSTD_getErrorName..
	 */
	ret = ZSTD_decompressStream(ctx, bout, bin);

	if (ZSTD_isError(ret))
	{
		ereport(ERROR,
				(errcode(ERRCODE_CONNECTION_FAILURE),
				errmsg("ZSTD_decompressStream failed, error is %s", ZSTD_getErrorName(ret))));
	}
	return ret;
}

static int
compress_zstd_data(URL_CURL_FILE *file)
{
	return ZSTD_compressCCtx(file->curl->zstd_cctx, file->out.cptr, file->out.top, file->out.ptr, file->out.top, file->zstd);
}
#endif

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
gp_proto1_read(char *buf, int bufsz, URL_CURL_FILE *file, CopyState pstate, char *buf2)
{
	char type;
	int  n = 0, len = 0;

	/*
	 * Loop through and get all types of messages, until we get actual data,
	 * or until there's no more data. Then quit the loop to process it and
	 * return it.
	 */
	while (file->block.datalen == 0 && !file->eof)
	{
		/* need 5 bytes, 1 byte type + 4 bytes length */
		fill_buffer(file, 5);
		n = file->in.top - file->in.bot;

		if (n == 0)
			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("gpfdist error: server closed connection")));

		if (n < 5)
			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("gpfdist error: incomplete packet - packet len %d", n)));

		/* read type */
		type = file->in.ptr[file->in.bot++];

		/* read len */
		memcpy(&len, &file->in.ptr[file->in.bot], 4);
		len = ntohl(len);		/* change order */
		file->in.bot += 4;

		if (len < 0)
			elog(ERROR, "gpfdist error: bad packet type %d len %d",
				 type, len);

		/* Error */
		if (type == 'E')
		{
			fill_buffer(file, len);
			n = file->in.top - file->in.bot;

			if (n > len)
				n = len;

			if (n > 0)
			{
				/*
				 * cheat a little. swap last char and
				 * NUL-terminator. then print string (without last
				 * char) and print last char artificially
				 */
				char x = file->in.ptr[file->in.bot + n - 1];
				file->in.ptr[file->in.bot + n - 1] = 0;
				ereport(ERROR,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("gpfdist error - %s%c", &file->in.ptr[file->in.bot], x)));
			}

			elog(ERROR, "gpfdist error: please check gpfdist log messages.");

			return -1;
		}

		/* Filename */
		if (type == 'F')
		{
			if (buf != buf2)
			{
				file->in.bot -= 5;
				return 0;
			}
			if (len > 256)
				elog(ERROR, "gpfdist error: filename too long (%d)", len);

			if (-1 == fill_buffer(file, len))
				elog(ERROR, "gpfdist error: stream ends suddenly");

			/*
			 * If SREH is used we now update it with the actual file that the
			 * gpfdist server is reading. This is because SREH (or the client
			 * in general) doesn't know which file gpfdist is reading, since
			 * the original URL may include a wildcard or a directory listing.
			 */
			if (pstate->cdbsreh)
			{
				char fname[257];

				memcpy(fname, file->in.ptr + file->in.bot, len);
				fname[len] = 0;
				snprintf(pstate->cdbsreh->filename, sizeof pstate->cdbsreh->filename, "%s [%s]",
						 file->common.url, fname);
			}

			file->in.bot += len;
			Assert(file->in.bot <= file->in.top);
			continue;
		}

		/* Offset */
		if (type == 'O')
		{
			if (len != 8)
				elog(ERROR, "gpfdist error: offset not of length 8 (%d)", len);

			if (-1 == fill_buffer(file, len))
				elog(ERROR, "gpfdist error: stream ends suddenly");

			file->in.bot += 8;
			Assert(file->in.bot <= file->in.top);
			continue;
		}

		/* Line number */
		if (type == 'L')
		{
			int64 line_number;

			if (len != 8)
				elog(ERROR, "gpfdist error: line number not of length 8 (%d)", len);

			if (-1 == fill_buffer(file, len))
				elog(ERROR, "gpfdist error: stream ends suddenly");

			/*
			 * update the line number of the first line we're about to get from
			 * gpfdist. pstate will update the following lines when processing
			 * the data
			 */
			memcpy(&line_number, file->in.ptr + file->in.bot, len);
			line_number = local_ntohll(line_number);
			pstate->cur_lineno = line_number ? line_number : INT64_MIN;
			file->in.bot += 8;
			Assert(file->in.bot <= file->in.top);
			continue;
		}

		/* Data */
		if (type == 'D')
		{
			file->block.datalen = len;
			file->eof = (len == 0);
			break;
		}

		elog(ERROR, "gpfdist error: unknown meta type %d", type);
	}

	int left_bytes = file->in.top - file->in.bot;

	if (file->zstd)
	{
		/* 'lastsize' is the number of bytes required for next decompression.
		 * 'left_bytes' is the number of bytes remained in 'file->in.ptr'.
		 * If left_bytes is less than 'lastsize', the next decompression
		 * can't complete in a decompression operation. Thus, when 
		 * 'file->lastsize > left_bytes', we need more bytes and fill_buffer is called.
		 * 
		 * When the condition 'file->block.datalen == len' is met, a new
		 * request just start. In this case lastsize is an init value, and 
		 * cannot provide the information about how many bytes required
		 * to finish the first frame decompression. In this case, enough
		 * bytes(more than ZSTD_DStreamInSize() returning) should be filled
		 * into 'file->in.ptr' to ensure that the first decompression 
		 * completing.
		 */
		if (file->lastsize > left_bytes || file->block.datalen == len)
		{
#ifdef USE_ZSTD
			int wantsz = ZSTD_DStreamInSize() - left_bytes;
			fill_buffer(file, wantsz);
#endif
		}
	} 
	else 
	{
		/* read data block */
		if (bufsz > file->block.datalen)
			bufsz = file->block.datalen;

		fill_buffer(file, bufsz);
	}

	n = file->in.top - file->in.bot;

	/* if gpfdist closed connection prematurely or died catch it here */
	if (n == 0 && !file->eof)
	{
		file->error = 1;
		
		if (!file->still_running)
			ereport(ERROR,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					 errmsg("gpfdist server closed connection"),
					 errhint("The root cause is likely to be an overload of the ETL host or "
							 "a temporary network glitch between the database and the ETL host "
							 "causing the connection between the gpfdist and database to disconnect.")));
	}

	if (n > bufsz)
		n = bufsz;

#ifdef USE_ZSTD
	if (file->zstd && file->curl->zstd_dctx && !file->eof)
	{
		int ret;
		/* It is absolutely to put the decompression code in a loop.
		 * Since not every call of decompress_zstd_data will get data into bout.
		 * However, even thought there is no data in bout, the call of 
		 * decompress_zstd_data is neccersary for following decompression.
		 * If a empty buf is returned to gpdb, the error will occur. 
		 * So the loop ensures that we push forward the decompression until there 
		 * is data in bout.
		 */
		do
		{
			ZSTD_inBuffer bin = {file->in.ptr + file->in.bot, file->lastsize, 0};
			ZSTD_outBuffer bout = {buf, bufsz, 0};
			ret = decompress_zstd_data(file->curl->zstd_dctx, &bin, &bout);
			n = bout.pos; 
			file->in.bot += bin.pos;
			file->lastsize = ret;
			if (!ret)
			{
				file->lastsize = ZSTD_initDStream(file->curl->zstd_dctx);
				break;
			}		
		} while (n == 0);
	}
	else
	{
		memcpy(buf, file->in.ptr + file->in.bot, n);
		file->in.bot += n;
	}
#else
	memcpy(buf, file->in.ptr + file->in.bot, n);
	file->in.bot += n;
#endif
	file->block.datalen -= n;

	return n;
}

/*
 * gp_proto0_write
 *
 * use curl to write data to a the remote gpfdist server. We use
 * a push model with a POST request.
 */
static void
gp_proto0_write(URL_CURL_FILE *file, CopyState pstate)
{	
	char*		buf;
	int		nbytes;
#ifdef USE_ZSTD
	if(file->zstd){
		nbytes = compress_zstd_data(file);
		buf = file->out.cptr;
	}
	else
#endif
	{
		buf = file->out.ptr;
	 	nbytes = file->out.top;
	}

	if (nbytes == 0)
		return;

	/* post binary data */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_POSTFIELDS, buf);

	/* set the size of the postfields data */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_POSTFIELDSIZE, nbytes);

	/* set sequence number */
	char seq[128] = {0};
	snprintf(seq, sizeof(seq), INT64_FORMAT, file->seq_number);

	replace_httpheader(file, "X-GP-SEQ", seq);

	gp_perform_backoff_and_check_response(file, easy_perform_work);
	file->seq_number++;
}


/*
 * Send an empty POST request, with an added X-GP-DONE header.
 */
static void
gp_proto0_write_done(URL_CURL_FILE *file)
{
	set_httpheader(file, "X-GP-DONE", "1");

	/* use empty message */
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_POSTFIELDS, "");
	CURL_EASY_SETOPT(file->curl->handle, CURLOPT_POSTFIELDSIZE, 0);

	/* post away! */
	gp_perform_backoff_and_check_response(file, easy_perform_work);
}

static size_t
curl_fread(char *buf, int bufsz, URL_CURL_FILE *file, CopyState pstate)
{
	char*		p = buf;
	char*		q = buf + bufsz;
	int 		n;
	const int 	gp_proto = file->gp_proto;

	if (gp_proto != 0 && gp_proto != 1)
	{
		elog(ERROR, "unknown gp protocol %d", file->gp_proto);
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
curl_fwrite(char *buf, int nbytes, URL_CURL_FILE *file, CopyState pstate)
{
	if (!file->for_write)
		elog(ERROR, "cannot write to a read-mode external table");

	if (file->gp_proto != 0 && file->gp_proto != 1)
		elog(ERROR, "unknown gp protocol %d", file->gp_proto);

	/*
	 * if buffer is full (current item can't fit) - write it out to
	 * the server. if item still doesn't fit after we emptied the
	 * buffer, make more room.
	 */
	if (file->out.top + nbytes >= file->out.max)
	{
		/* item doesn't fit */
		if (file->out.top > 0)
		{
			/* write out existing data, empty the buffer */
			gp_proto0_write(file, pstate);
			file->out.top = 0;
		}
		
		/* does it still not fit? enlarge buffer */
		if (file->out.top + nbytes >= file->out.max)
		{
			int 	n = nbytes + 1024;
			char*	newbuf;

			newbuf = repalloc(file->out.ptr, n);
			if (!newbuf)
				elog(ERROR, "out of memory (curl_fwrite)");
			file->out.ptr = newbuf;
			
			if (file->zstd) 
			{
				newbuf = repalloc(file->out.cptr, n);
				if (!newbuf)
					elog(ERROR, "out of compress memory (curl_fwrite)");
				file->out.cptr = newbuf;
			}

			file->out.max = n;

			Assert(nbytes < file->out.max);
		}
	}

	/* copy buffer into file->buf */
	memcpy(file->out.ptr + file->out.top, buf, nbytes);
	file->out.top += nbytes;

	return nbytes;
}

size_t
url_curl_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	URL_CURL_FILE *cfile = (URL_CURL_FILE *) file;

	/* get data (up size) from the http/gpfdist server */
	return curl_fread(ptr, size, cfile, pstate);
}

size_t
url_curl_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	URL_CURL_FILE *cfile = (URL_CURL_FILE *) file;

	/* write data to the gpfdist server via curl */
	return curl_fwrite(ptr, size, cfile, pstate);
}

/*
 * flush all remaining buffered data waiting to be written out to external source
 */
void
url_curl_fflush(URL_FILE *file, CopyState pstate)
{
	gp_proto0_write((URL_CURL_FILE *) file, pstate);
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
url_curl_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate)
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
