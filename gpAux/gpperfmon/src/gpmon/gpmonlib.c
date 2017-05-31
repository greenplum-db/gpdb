#undef GP_VERSION
#include "postgres_fe.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include "gpmonlib.h"
#include "apr_queue.h"
#include "apr_atomic.h"
#include "apr_lib.h"
#include "assert.h"
#include "time.h"

#if APR_IS_BIGENDIAN
#define local_htonll(n)  (n)
#define local_ntohll(n)  (n)
#else
#define local_htonll(n)  ((((apr_uint64_t) htonl(n)) << 32LL) | htonl((n) >> 32LL))
#define local_ntohll(n)  ((((apr_uint64_t) ntohl(n)) << 32LL) | (apr_uint32_t) ntohl(((apr_uint64_t)n) >> 32LL))
#endif

extern apr_thread_mutex_t *logfile_mutex;

#define LOCK_STDOUT if (logfile_mutex) { apr_thread_mutex_lock(logfile_mutex); }
#define UNLOCK_STDOUT if (logfile_mutex) { apr_thread_mutex_unlock(logfile_mutex); }
#define META_LEN 100
#define READ_BUF_SIZE 100

inline void gp_smon_to_mmon_set_header(gp_smon_to_mmon_packet_t* pkt, apr_int16_t pkttype)
{
	pkt->header.pkttype = pkttype;
	pkt->header.magic = GPMON_MAGIC;
	pkt->header.version = GPMON_PACKET_VERSION;
	return;
}

/*Helper function to get the size of the union packet*/
inline size_t get_size_by_pkttype_smon_to_mmon(apr_int16_t pkttype)
{
	switch (pkttype) {
		case GPMON_PKTTYPE_HELLO:
			return(sizeof(gpmon_hello_t));
		case GPMON_PKTTYPE_METRICS:
			return(sizeof(gpmon_metrics_t));
		case GPMON_PKTTYPE_QLOG:
			return(sizeof(gpmon_qlog_t));
		case GPMON_PKTTYPE_QEXEC:
			return(sizeof(qexec_packet_t));
		case GPMON_PKTTYPE_SEGINFO:
			return(sizeof(gpmon_seginfo_t));
		case GPMON_PKTTYPE_QUERY_HOST_METRICS:
			return(sizeof(gpmon_qlog_t));
		case GPMON_PKTTYPE_FSINFO:
			return(sizeof(gpmon_fsinfo_t));
		case GPMON_PKTTYPE_QUERYSEG:
			return(sizeof(gpmon_query_seginfo_t));
	}

	return 0;
}

apr_status_t gpmon_ntohpkt(apr_int32_t magic, apr_int16_t version, apr_int16_t pkttype)
{
	static apr_int64_t last_err_sec = 0;

	if (magic != GPMON_MAGIC)
	{
		apr_int64_t now = time(NULL);
		if (now - last_err_sec >= GPMON_PACKET_ERR_LOG_TIME)
		{
			last_err_sec = now;
			gpmon_warning(FLINE, "bad packet (magic number mismatch)");
		}
		return APR_EINVAL;
	}

	if (version != GPMON_PACKET_VERSION)
	{
		apr_int64_t now = time(NULL);
		if (now - last_err_sec >= GPMON_PACKET_ERR_LOG_TIME)
		{
			last_err_sec = now;
			gpmon_warning(FLINE, "bad packet (version %d, expected %d)", version, GPMON_PACKET_VERSION);
		}
		return APR_EINVAL;
	}

    if (! (GPMON_PKTTYPE_NONE < pkttype && pkttype < GPMON_PKTTYPE_MAX))
	{
		apr_int64_t now = time(NULL);
		if (now - last_err_sec >= GPMON_PACKET_ERR_LOG_TIME)
		{
			last_err_sec = now;
			gpmon_warning(FLINE, "bad packet (unexpected packet type %d)", pkttype);
		}
		return APR_EINVAL;
	}

	return 0;
}


#define GPMONLIB_DATETIME_BUFSIZE_LOCAL 100
static char* datetime(void)
{
	static char buf[GPMONLIB_DATETIME_BUFSIZE_LOCAL];
	time_t now;
	now = time(NULL);
	return gpmon_datetime(now, buf);
}



int gpmon_print(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

	LOCK_STDOUT

    fprintf(stdout, "%s|:-LOG: ", datetime());
    vfprintf(stdout, fmt, ap);
    fflush(stdout);

	UNLOCK_STDOUT

    return 0;
}


int gpmon_fatal(const char* fline, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	LOCK_STDOUT
	fprintf(stdout, "%s|:-FATAL: [INTERNAL ERROR %s] ", datetime(), fline);
	vfprintf(stdout, fmt, ap);
	fprintf(stdout, "\n          ... exiting\n");
	UNLOCK_STDOUT

	fprintf(stderr, "%s|:-FATAL: [INTERNAL ERROR %s] ", datetime(), fline);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n          ... exiting\n");

	exit(1);
	return 0;
}

int gpsmon_fatal(const char* fline, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stdout, "%s|:-FATAL: [INTERNAL ERROR %s] ", datetime(), fline);
	vfprintf(stdout, fmt, ap);
	fprintf(stdout, "\n          ... exiting\n");
	exit(1);
	return 0;
}



int gpmon_fatalx(const char* fline, int e, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	LOCK_STDOUT
	fprintf(stdout, "%s|:-FATAL: [INTERNAL ERROR %s] ", datetime(), fline);
	vfprintf(stdout, fmt, ap);
	if (e)
	{
		char msg[512];
		fprintf(stdout, "\n\terror %d (%s)", e, apr_strerror(e, msg, sizeof(msg)));
	}
	fprintf(stdout, "\n\t... exiting\n");
	UNLOCK_STDOUT

	fprintf(stderr, "%s|:-FATAL: [INTERNAL ERROR %s] ", datetime(), fline);
	vfprintf(stderr, fmt, ap);
	if (e)
	{
		char msg[512];
		fprintf(stderr, "\n\terror %d (%s)", e, apr_strerror(e, msg, sizeof(msg)));
	}
	fprintf(stderr, "\n\t... exiting\n");

	exit(1);
	return 0;
}


int gpsmon_fatalx(const char* fline, int e, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stdout, "%s|:-FATAL: [INTERNAL ERROR %s] ", datetime(), fline);
	vfprintf(stdout, fmt, ap);
	if (e)
	{
		char msg[512];
		fprintf(stdout, "\n\terror %d (%s)", e, apr_strerror(e, msg, sizeof(msg)));
	}
	fprintf(stdout, "\n\t... exiting\n");
	exit(1);
	return 0;
}




int gpmon_warning(const char* fline, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

	LOCK_STDOUT
    fprintf(stdout, "%s|:-WARNING: [%s] ", datetime(), fline);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
	UNLOCK_STDOUT

    return 0;
}

int gpmon_warningx(const char* fline, int e, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	LOCK_STDOUT
	fprintf(stdout, "%s|:-WARNING: [%s] ", datetime(), fline);
	vfprintf(stdout, fmt, ap);
	if (e) {
		char msg[512];
		fprintf(stdout, "\n\terror %d (%s)", e, apr_strerror(e, msg, sizeof(msg)));
	}
	fprintf(stdout, "\n");
	UNLOCK_STDOUT

	return 0;
}

void gpmon_print_file(const char* header_line, FILE* fp)
{
	char buffer[READ_BUF_SIZE];
	int len;

	LOCK_STDOUT
	fprintf(stdout, "%s|:-WARNING: [%s] \n", datetime(), header_line);
	while ((len = fread(buffer, 1, sizeof(buffer) - 1, fp)) != 0)
	{
		buffer[len] = 0;
		fprintf(stdout, "%s", buffer);
	}
	fprintf(stdout, "\n");

	UNLOCK_STDOUT
}

const char* gpmon_qlog_status_string(int gpmon_qlog_status)
{
    switch (gpmon_qlog_status) {
    case GPMON_QLOG_STATUS_SILENT: return "silent";
    case GPMON_QLOG_STATUS_SUBMIT: return "submit";
    case GPMON_QLOG_STATUS_START: return "start";
    case GPMON_QLOG_STATUS_DONE: return "done";
    case GPMON_QLOG_STATUS_ERROR: return "abort";
    case GPMON_QLOG_STATUS_CANCELING: return "canceling";
    }
    return "unknown";
}


/* remove whitespaces from front & back of s */
char* gpmon_trim(char* s)
{
    char* p = s;
    char* q = s + strlen(s);
    for ( ; p < q && apr_isspace(*p); p++);
    for ( ; p < q && apr_isspace(q[-1]); q--);
    *q = 0;
    return p;
}


/* datetime, e.g. 2004-02-14  23:50:02 */
char* gpmon_datetime(time_t t, char str[GPMON_DATE_BUF_SIZE])
{
	struct tm tm =  { 0 };

	str[0] = 0;

	if (!localtime_r(&t, &tm))
	{
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "localtime_r failed");
		return str;
	}

	snprintf(str, GPMON_DATE_BUF_SIZE, "%04d-%02d-%02d %02d:%02d:%02d",
	    1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec);

    	return str;
}

/* datetime, e.g. 2004-02-14  23:50:10
   round to lowest 5 second interval */
char* gpmon_datetime_rounded(time_t t, char str[GPMON_DATE_BUF_SIZE])
{
	struct tm tm =  { 0 };

	str[0] = 0;

	if (!localtime_r(&t, &tm))
	{
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "localtime_r failed");
		return str;
	}

	snprintf(str, GPMON_DATE_BUF_SIZE, "%04d-%02d-%02d %02d:%02d:%02d",
	1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, ((tm.tm_sec/5)*5));

	return str;
}

/* get status from query text file */
apr_int32_t get_query_status(apr_int32_t tmid, apr_int32_t ssid,
							 apr_int32_t ccnt)
{
	char fname[GPMON_DIR_MAX_PATH];
	FILE *fp;
	apr_int32_t status = GPMON_QLOG_STATUS_INVALID;

	snprintf(fname, GPMON_DIR_MAX_PATH, "%sq%d-%d-%d.txt", GPMON_DIR, tmid, ssid, ccnt);

	fp = fopen(fname, "r");
	if (!fp)
		return GPMON_QLOG_STATUS_INVALID;

	if (0 != fseek(fp, -1, SEEK_END))
	{
		fclose(fp);
		return GPMON_QLOG_STATUS_INVALID;
	}
	fscanf(fp, "%d", &status);
	fclose(fp);
	return status;
}

/* get query text from query text file */
char *get_query_text(apr_int32_t tmid, apr_int32_t ssid, apr_int32_t ccnt, apr_pool_t *pool)
{
	char meta[META_LEN] = {0};
	signed int qrylen = 0;
	char fname[GPMON_DIR_MAX_PATH] = {0};
	const char *META_FMT = "%d qtext";
	const char *META_QTEXT = "qtext\n";

	snprintf(fname, GPMON_DIR_MAX_PATH, "%sq%d-%d-%d.txt", GPMON_DIR, tmid, ssid, ccnt);

	FILE *fp = fopen(fname, "r");
	if (!fp)
	{
		TR0(("Warning: Open file %s failed\n", fname));
		return NULL;
	}

	// get query text length
	char *readPtr = fgets(meta, META_LEN, fp);
	int metaLen = strlen(meta);
	if (readPtr != meta || metaLen <= strlen(META_QTEXT) || 0 != strcmp(META_QTEXT, meta + metaLen - strlen(META_QTEXT)))
	{
		fclose(fp);
		TR0(("Warning: Invalid format in file '%s'\n", fname));
		return NULL;
	}

	int count = sscanf(meta, META_FMT, &qrylen);
	if (count != 1 || qrylen < 0)
	{
		fclose(fp);
		TR0(("Warning: Invalid format in file '%s', line 1: '%s'\n", fname, meta));
		return NULL;
	}

	if (qrylen > MAX_QUERY_COMPARE_LENGTH)
	{
		TR0(("Warning: Query length is very big %d\n", qrylen));
		qrylen = MAX_QUERY_COMPARE_LENGTH;
	}

	char *query = apr_palloc(pool, qrylen + 1);
	if (query == NULL)
	{
		fclose(fp);
		TR0(("Warning: Out of memory when allocating query text\n"));
		return NULL;
	}

	// read query text
	int readlen = fread(query, 1, qrylen, fp);
	if (readlen != qrylen)
	{
		fclose(fp);
		TR0(("Warning: Failed to read query text in file: '%s', query text length %d, read length %d.\n", fname, qrylen, readlen));
		return NULL;
	}
	query[readlen] = '\0';

	fclose(fp);

	return query;
}

int gpmon_recursive_mkdir(char* work_dir)
{
    char *pdir = work_dir;
    while (*pdir)
    {
        if (*pdir == '/' && (pdir != work_dir))
        {
            *pdir = 0;
            if (-1 == mkdir(work_dir, 0700) && EEXIST != errno)
            {
				fprintf(stderr, "Performance Monitor - mkdir '%s' failed", work_dir);
				perror("Performance Monitor -");
                return 1;
            }
            *pdir = '/';
        }
        pdir++;
    }

    if (-1 == mkdir(work_dir, 0700) && EEXIST != errno)
    {
		fprintf(stderr, "Performance Monitor - mkdir '%s' failed", work_dir);
		perror("Performance Monitor -");
        return 1;
    }

    return 0;
}


/*
 * Create apr_pool_t with parent as well as a new allocator which belongs to
 * itself so that when calling apr_pool_destroy, the free memory inside this
 * pool will be returned to OS. (MPP-23751)
 */
apr_status_t apr_pool_create_alloc(apr_pool_t ** newpool, apr_pool_t *parent)
{
	apr_status_t rv;
	apr_allocator_t *allocator;
	if ((rv = apr_allocator_create(&allocator)) != APR_SUCCESS)
	{
		return rv;
	}
	if ((rv = apr_pool_create_ex(newpool, parent, NULL, allocator)) != APR_SUCCESS)
	{
		apr_allocator_destroy(allocator);
		return rv;
	}
	// This function is only for internal use, so newpool can't be NULL.
	apr_allocator_owner_set(allocator, *newpool);

	return APR_SUCCESS;
}

void advance_connection_hostname(host_t* host)
{
	// for connections we should only be connecting 1 time
	// if the smon fails we may have to reconnect but this event is rare
	// we try 3 times on each hostname and then switch to another
	host->connection_hostname.counter++;

	if (host->connection_hostname.counter > 3)
	{
		if (host->connection_hostname.current->next)
		{
			// try the next hostname
			host->connection_hostname.current = host->connection_hostname.current->next;
		}
		else
		{
			// restart at the head of address list
			host->connection_hostname.current = host->addressinfo_head;
		}
		host->connection_hostname.counter = 1;
	}
}

char* get_connection_hostname(host_t* host)
{
	return host->connection_hostname.current->address;
}

char* get_connection_ip(host_t* host)
{
	return host->connection_hostname.current->ipstr;
}

bool get_connection_ipv6_status(host_t* host)
{
	return host->connection_hostname.current->ipv6;
}

double subtractTimeOfDay(struct timeval* begin, struct timeval* end)
{
    double seconds;

    if (end->tv_usec < begin->tv_usec)
    {
        end->tv_usec += 1000000;
        end->tv_sec -= 1;
    }

    seconds = end->tv_usec - begin->tv_usec;
    seconds /= 1000000.0;

    seconds += (end->tv_sec - begin->tv_sec);
    return seconds;
}

