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

#define GPMON_NTOHS(x) x = ntohs(x)
#define GPMON_NTOHL(x) x = ntohl(x)
#define GPMON_NTOHLL(x) x = local_ntohll(x)

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

const char* gpdb_getnodestatus(PerfmonNodeStatus status)
{
	const char* name = "";
	switch(status)
	{
		case PMNS_Initialize:
			name = "Initialize";
		break;
		case PMNS_Executing:
			name = "Executing";
		break;
		case PMNS_Finished:
			name = "Finished";
		break;
		default:
		break;
	}

	return name;
}

/* Node Types */
#define QENODE_TYPE_APPEND "Append"
#define QENODE_TYPE_APPENDONLY_SCAN "Append-only Scan"
#define QENODE_TYPE_ASSERTOP "Assert"
#define QENODE_TYPE_AGGREGATE "Aggregate"
#define QENODE_TYPE_GROUPAGGREGATE "GroupAggregate"
#define QENODE_TYPE_HASHAGGREGATE "HashAggregate"
#define QENODE_TYPE_APPENDONLY_COLUMNAR_SCAN "Append-Only Columnar Scan"
#define QENODE_TYPE_BITMAPAND "BitmapAnd"
#define QENODE_TYPE_BITMAPOR "BitmapOr"
#define QENODE_TYPE_BITMAP_APPENDONLY_SCAN "Bitmap Append-Only Scan"
#define QENODE_TYPE_BITMAP_HEAP_SCAN "Bitmap Heap Scan"
#define QENODE_TYPE_BITMAP_TABLE_SCAN "Bitmap Table Scan"
#define QENODE_TYPE_BITMAP_INDEX_SCAN "Bitmap Index Scan"
#define QENODE_TYPE_DML "DML"
#define QENODE_TYPE_TABLE_SCAN "Table Scan"
#define QENODE_TYPE_EXTERNAL_SCAN "External Scan"
#define QENODE_TYPE_FUNCTION_SCAN "Function Scan"
#define QENODE_TYPE_GROUP "Group"
#define QENODE_TYPE_HASH "Hash"
#define QENODE_TYPE_HASH_JOIN "Hash Join"
#define QENODE_TYPE_HASH_LEFT_JOIN "Hash Left Join"
#define QENODE_TYPE_HASH_LEFT_ANTI_SEMI_JOIN "Hash Left Anti Semi Join"
#define QENODE_TYPE_HASH_FULL_JOIN "Hash Full Join"
#define QENODE_TYPE_HASH_RIGHT_JOIN "Hash Right Join"
#define QENODE_TYPE_HASH_EXISTS_JOIN "Hash EXISTS Join"
#define QENODE_TYPE_HASH_REVERSE_IN_JOIN "Hash Reverse In Join"
#define QENODE_TYPE_HASH_UNIQUE_INNER_JOIN "Hash Unique Inner Join"
#define QENODE_TYPE_HASH_UNIQUE_OUTER_JOIN "Hash Unique Outer Join"
#define QENODE_TYPE_INDEX_SCAN "Index Scan"
#define QENODE_TYPE_LIMIT "Limit"
#define QENODE_TYPE_MATERIALIZE "Materialize"
#define QENODE_TYPE_MERGE_JOIN "Merge Join"
#define QENODE_TYPE_MERGE_LEFT_JOIN "Merge Left Join"
#define QENODE_TYPE_MERGE_LEFT_ANTI_SEMI_JOIN "Merge Left Anti Semi Join"
#define QENODE_TYPE_MERGE_FULL_JOIN "Merge Full Join"
#define QENODE_TYPE_MERGE_RIGHT_JOIN "Merge Right Join"
#define QENODE_TYPE_MERGE_EXISTS_JOIN "Merge EXISTS Join"
#define QENODE_TYPE_MERGE_REVERSE_IN_JOIN "Merge Reverse In Join"
#define QENODE_TYPE_MERGE_UNIQUE_OUTER_JOIN "Merge Unique Outer Join"
#define QENODE_TYPE_MERGE_UNIQUE_INNER_JOIN "Merge Unique Inner Join"
#define QENODE_TYPE_REDISTRIBUTE_MOTION "Redistribute Motion"
#define QENODE_TYPE_BROADCAST_MOTION "Broadcast Motion"
#define QENODE_TYPE_GATHER_MOTION "Gather Motion"
#define QENODE_TYPE_EXPLICIT_REDISTRIBUTE_MOTION "Explicit Redistribute Motion"
#define QENODE_TYPE_NESTED_LOOP "Nested Loop"
#define QENODE_TYPE_NESTED_LOOP_LEFT_JOIN "Nested Loop Left Join"
#define QENODE_TYPE_NESTED_LOOP_LEFT_ANTI_SEMI_JOIN "Nested Loop Left Anti Semi Join"
#define QENODE_TYPE_NESTED_LOOP_FULL_JOIN "Nested Loop Full Join"
#define QENODE_TYPE_NESTED_LOOP_RIGHT_JOIN "Nested Loop Right Join"
#define QENODE_TYPE_NESTED_LOOP_EXISTS_JOIN "Nested Loop EXISTS Join"
#define QENODE_TYPE_NESTED_LOOP_REVERSE_IN_JOIN "Nested Loop Reverse In Join"
#define QENODE_TYPE_NESTED_LOOP_UNIQUE_OUTER_JOIN "Nested Loop Unique Outer Join"
#define QENODE_TYPE_NESTED_LOOP_UNIQUE_INNER_JOIN "Nested Loop Unique Inner Join"
#define QENODE_TYPE_PARTITIONSELECTOR "Partition Selector"
#define QENODE_TYPE_RESULT "Result"
#define QENODE_TYPE_REPEAT "Repeat"
#define QENODE_TYPE_ROWTRIGGER "Row Trigger"
#define QENODE_TYPE_SEQ_SCAN "Seq Scan"
#define QENODE_TYPE_SEQUENCE "Sequence"
#define QENODE_TYPE_SETOP "SetOp"
#define QENODE_TYPE_SETOP_INTERSECT "SetOp Intersect"
#define QENODE_TYPE_SETOP_INTERSECT_ALL "SetOp Intersect All"
#define QENODE_TYPE_SETOP_EXCEPT "SetOp Except"
#define QENODE_TYPE_SETOP_EXCEPT_ALL "SetOp Except All"
#define QENODE_TYPE_SHARED_SCAN "Shared Scan"
#define QENODE_TYPE_SORT "Sort"
#define QENODE_TYPE_SPLITUPDATE "Split Update"
#define QENODE_TYPE_SUBQUERY_SCAN "Subquery Scan"
#define QENODE_TYPE_TABLE_FUNCTION_SCAN "Table Function Scan"
#define QENODE_TYPE_DYNAMICINDEX_SCAN "Dynamic Index Scan"
#define QENODE_TYPE_DYNAMICTABLE_SCAN "Dynamic Table Scan"
#define QENODE_TYPE_TID_SCAN "Tid Scan"
#define QENODE_TYPE_UNIQUE "Unique"
#define QENODE_TYPE_VALUES_SCAN "Values Scan"
#define QENODE_TYPE_WINDOW "Window"

/* Units for node measurements */
#define QENODE_UNIT_ROWS "Rows"
#define QENODE_UNIT_TUPLES "Tuples"
#define QENODE_UNIT_INPUTS "Inputs"
#define QENODE_UNIT_RESCANS "Rescans"
#define QENODE_UNIT_BYTES "Bytes"
#define QENODE_UNIT_BATCHES "Batches"
#define QENODE_UNIT_PASSES "Passes"
#define QENODE_UNIT_PAGES "Pages"
#define QENODE_UNIT_RESTORES "Restores"
#define QENODE_UNIT_TIME "Microseconds"
#define QENODE_UNIT_PACKETS "Packets"

/* node measurement types */
#define QENODE_MEASURE_ROWS_IN "Rows in"
#define QENODE_MEASURE_APPEND_CURRENT_INPUT_SOURCE "Append Current Input Source"
#define QENODE_MEASURE_APPENDONLY_SCAN_RESCAN "Append-only Scan Rescan"
#define QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_TUPLES "Aggregate Total Spill Tuples"
#define QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_BYTES "Aggregate Total Spill Bytes"
#define QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_BATCHES "Aggregate Total Spill Batches"
#define QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_PASS "Aggregate Total Spill Pass"
#define QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_READ_TUPLES "Aggregate Current Spill Pass Read Tuples"
#define QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_READ_BYTES "Aggregate Current Spill Pass Read Bytes"
#define QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_TUPLES "Aggregate Current Spill Pass Tuples"
#define QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_BYTES "Aggregate Current Spill Pass Bytes"
#define QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_BATCHES "Aggregate Current Spill Pass Batches"
#define QENODE_MEASURE_APPENDONLY_COLUMNAR_SCAN_RESCAN "Append-Only Columnar Scan Rescan"
#define QENODE_MEASURE_BITMAP_APPEDONLY_SCAN_PAGES "Bitmap Apped-Only Scan Pages"
#define QENODE_MEASURE_BITMAP_APPENDONLY_SCAN_RESCAN "Bitmap Append-Only Scan Rescan"
#define QENODE_MEASURE_BITMAP_HEAP_SCAN_PAGES "Bitmap Heap Scan Pages"
#define QENODE_MEASURE_BITMAP_HEAP_SCAN_RESCAN "Bitmap Heap Scan Rescan"
#define QENODE_MEASURE_BITMAP_TABLE_SCAN_PAGES "Bitmap Table Scan Pages"
#define QENODE_MEASURE_BITMAP_TABLE_SCAN_RESCAN "Bitmap Table Scan Rescan"
#define QENODE_MEASURE_BITMAP_INDEX_SCAN_RESCAN "Bitmap Index Scan Rescan"
#define QENODE_MEASURE_DYNAMICINDEX_SCAN_RESCAN "Dynamic Index Scan Rescan"
#define QENODE_MEASURE_DYNAMICTABLE_SCAN_RESCAN "Dynamic Table Scan Rescan"
#define QENODE_MEASURE_EXTERNAL_SCAN_RESCAN "External Scan Rescan"
#define QENODE_MEASURE_HASH_SPILL_BATCHES "Hash Spill Batches"
#define QENODE_MEASURE_HASH_SPILL_TUPLES "Hash Spill Tuples"
#define QENODE_MEASURE_HASH_SPILL_BYTES "Hash Spill Bytes"
#define QENODE_MEASURE_HASH_JOIN_SPILL_BATCHES "Hash Join Spill Batches"
#define QENODE_MEASURE_HASH_JOIN_SPILL_TUPLES "Hash Join Spill Tuples"
#define QENODE_MEASURE_HASH_JOIN_SPILL_BYTES "Hash Join Spill Bytes"
#define QENODE_MEASURE_INDEX_SCAN_RESTORE_POS "Index Scan Restore Pos"
#define QENODE_MEASURE_INDEX_SCAN_RESCAN "Index Scan Rescan"
#define QENODE_MEASURE_MATERIALIZE_RESCAN "Materialize Rescan"
#define QENODE_MEASURE_MERGE_JOIN_INNER_TUPLES "Merge Join Inner Tuples"
#define QENODE_MEASURE_MERGE_JOIN_OUTER_TUPLES "Merge Join Outer Tuples"
#define QENODE_MEASURE_NESTED_LOOP_INNER_TUPLES "Nested Loop Inner Tuples"
#define QENODE_MEASURE_NESTED_LOOP_OUTER_TUPLES "Nested Loop Outer Tuples"
#define QENODE_MEASURE_SEQ_SCAN_PAGE_STATS "Seq Scan Page Stats"
#define QENODE_MEASURE_SEQ_SCAN_RESTORE_POS "Seq Scan Restore Pos"
#define QENODE_MEASURE_SEQ_SCAN_RESCAN "Seq Scan Rescan"
#define QENODE_MEASURE_SHARED_SCAN_RESTORE_POS "Shared Scan Restore Pos"
#define QENODE_MEASURE_SHARED_SCAN_RESCAN "Shared Scan Rescan"
#define QENODE_MEASURE_SORT_MEMORY_USAGE "Sort Memory Usage"
#define QENODE_MEASURE_SORT_SPILL_TUPLES "Sort Spill Tuples"
#define QENODE_MEASURE_SORT_SPILL_BYTES "Sort Spill Bytes"
#define QENODE_MEASURE_SORT_SPILL_PASS "Sort Spill Pass"
#define QENODE_MEASURE_SORT_CURRENT_SPILL_PASS_TUPLES "Sort Current Spill Pass Tuples"
#define QENODE_MEASURE_SORT_CURRENT_SPILL_PASS_BYTES "Sort Current Spill Pass Bytes"
#define QENODE_MEASURE_SUBQUERY_SCAN_RESCAN "Subquery Scan Rescan"
#define QENODE_MEASURE_TABLE_SCAN_PAGE_STATS "Table Scan Page Stats"
#define QENODE_MEASURE_TABLE_SCAN_RESTORE_POS "Table Scan Restore Pos"
#define QENODE_MEASURE_TABLE_SCAN_RESCAN "Table Scan Rescan"
#define QENODE_MEASURE_MOTION_BYTES_SENT "Motion Bytes Sent"
#define QENODE_MEASURE_MOTION_TOTAL_ACK_TIME "Motion Total Ack Time"
#define QENODE_MEASURE_MOTION_AVG_ACK_TIME "Motion Average Ack Time"
#define QENODE_MEASURE_MOTION_MAX_ACK_TIME "Motion Max Ack Time"
#define QENODE_MEASURE_MOTION_MIN_ACK_TIME "Motion Min Ack Time"
#define QENODE_MEASURE_MOTION_COUNT_RESENT "Motion Count Resent"
#define QENODE_MEASURE_MOTION_MAX_RESENT "Motion Max Resent"
#define QENODE_MEASURE_MOTION_BYTES_RECEIVED "Motion Bytes Received"
#define QENODE_MEASURE_MOTION_COUNT_DROPPED "Motion Count Dropped"

#define QENODE_AGG_METRICS  \
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_TUPLES, QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_BYTES, QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_BATCHES, QENODE_MEASURE_AGGREGATE_TOTAL_SPILL_PASS, QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_READ_TUPLES, QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_READ_BYTES, QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_TUPLES, QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_BYTES, QENODE_MEASURE_AGGREGATE_CURRENT_SPILL_PASS_BATCHES}
#define QENODE_AGG_UNITS  \
		{QENODE_UNIT_ROWS, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES, QENODE_UNIT_BATCHES, QENODE_UNIT_PASSES, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES, QENODE_UNIT_BATCHES }

#define QENODE_HASHJOIN_METRICS \
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_HASH_JOIN_SPILL_BATCHES, QENODE_MEASURE_HASH_JOIN_SPILL_TUPLES, QENODE_MEASURE_HASH_JOIN_SPILL_BYTES}
#define QENODE_HASHJOIN_UNITS \
		{QENODE_UNIT_ROWS, QENODE_UNIT_BATCHES, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES}

#define QENODE_MERGEJOIN_METRICS \
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_MERGE_JOIN_INNER_TUPLES, QENODE_MEASURE_MERGE_JOIN_OUTER_TUPLES}
#define QENODE_MERGEJOIN_UNITS \
		{QENODE_UNIT_ROWS, QENODE_UNIT_TUPLES, QENODE_UNIT_TUPLES}

#define QENODE_NESTEDLOOP_METRICS \
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_NESTED_LOOP_INNER_TUPLES, QENODE_MEASURE_NESTED_LOOP_OUTER_TUPLES}
#define QENODE_NESTEDLOOP_UNITS \
		{QENODE_UNIT_ROWS, QENODE_UNIT_TUPLES, QENODE_UNIT_TUPLES}

#define QENODE_MOTION_METRICS \
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_MOTION_BYTES_SENT, QENODE_MEASURE_MOTION_TOTAL_ACK_TIME, QENODE_MEASURE_MOTION_AVG_ACK_TIME, QENODE_MEASURE_MOTION_MAX_ACK_TIME, QENODE_MEASURE_MOTION_MIN_ACK_TIME, QENODE_MEASURE_MOTION_COUNT_RESENT, QENODE_MEASURE_MOTION_MAX_RESENT, QENODE_MEASURE_MOTION_BYTES_RECEIVED, QENODE_MEASURE_MOTION_COUNT_DROPPED}
#define QENODE_MOTION_UNITS \
		{QENODE_UNIT_ROWS, QENODE_UNIT_BYTES, QENODE_UNIT_TIME, QENODE_UNIT_TIME, QENODE_UNIT_TIME, QENODE_UNIT_TIME, QENODE_UNIT_PACKETS, QENODE_UNIT_PACKETS, QENODE_UNIT_BYTES, QENODE_UNIT_PACKETS }

/* this struct is used for lookups to find strings
   the struct is 166 bytes -- this many bytes will be added to the process times number of node types */
typedef struct QenodeNamesDescriptor
{
	PerfmonNodeType type;  /* this is for code correctness to vefify a QenodeNamesDescriptor is in the correct place in lookup table */
	const char* nodename;
	const char* measurenames[GPMON_QEXEC_M_COUNT];
	const char* measureunits[GPMON_QEXEC_M_COUNT];
	unsigned int number_metrics;
} QenodeNamesDescriptor;


/* the index into this array is the PerfmonNodeType enum
   the positions in this array must match the positions in the enum
   we do asserts to validate that there is no mistake in the positions */
QenodeNamesDescriptor nodetype_string_lookup[PMNT_MAXIMUM_ENUM] =
{
	{PMNT_Invalid, NULL, { NULL }, {NULL }},

	{PMNT_Append, QENODE_TYPE_APPEND,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_APPEND_CURRENT_INPUT_SOURCE  },
		{QENODE_UNIT_ROWS, QENODE_UNIT_INPUTS },
		2
	},
	{PMNT_AppendOnlyScan, QENODE_TYPE_APPENDONLY_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_APPENDONLY_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS},
		2
	},
	{PMNT_AssertOp, QENODE_TYPE_ASSERTOP,
		{QENODE_MEASURE_ROWS_IN},
		{QENODE_UNIT_ROWS}
	},
	{PMNT_Aggregate, QENODE_TYPE_AGGREGATE,
		QENODE_AGG_METRICS,
		QENODE_AGG_UNITS,
		1
	},
	{PMNT_GroupAggregate, QENODE_TYPE_GROUPAGGREGATE,
		QENODE_AGG_METRICS,
		QENODE_AGG_UNITS,
		1
	},
	{PMNT_HashAggregate, QENODE_TYPE_HASHAGGREGATE,
		QENODE_AGG_METRICS,
		QENODE_AGG_UNITS,
		1
	},
	{PMNT_AppendOnlyColumnarScan, QENODE_TYPE_APPENDONLY_COLUMNAR_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_APPENDONLY_COLUMNAR_SCAN_RESCAN},
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS},
		2
	},
	{PMNT_BitmapAnd, QENODE_TYPE_BITMAPAND,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_BitmapOr, QENODE_TYPE_BITMAPOR,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_BitmapAppendOnlyScan, QENODE_TYPE_BITMAP_APPENDONLY_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_BITMAP_APPEDONLY_SCAN_PAGES, QENODE_MEASURE_BITMAP_APPENDONLY_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_PAGES, QENODE_UNIT_RESCANS },
		3
	},
	{PMNT_BitmapHeapScan, QENODE_TYPE_BITMAP_HEAP_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_BITMAP_HEAP_SCAN_PAGES, QENODE_MEASURE_BITMAP_HEAP_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_PAGES, QENODE_UNIT_RESCANS },
		3
	},
	{PMNT_BitmapTableScan, QENODE_TYPE_BITMAP_TABLE_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_BITMAP_TABLE_SCAN_PAGES, QENODE_MEASURE_BITMAP_TABLE_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_PAGES, QENODE_UNIT_RESCANS },
		3
	},
	{PMNT_BitmapIndexScan, QENODE_TYPE_BITMAP_INDEX_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_BITMAP_INDEX_SCAN_RESCAN  },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS },
		2
	},
	{PMNT_DML, QENODE_TYPE_DML,
		{QENODE_MEASURE_ROWS_IN},
		{QENODE_UNIT_ROWS}
	},
	{PMNT_DynamicIndexScan, QENODE_TYPE_DYNAMICINDEX_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_DYNAMICINDEX_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS}
	},
	{PMNT_DynamicTableScan, QENODE_TYPE_DYNAMICTABLE_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_DYNAMICTABLE_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS}
	},
	{PMNT_ExternalScan, QENODE_TYPE_EXTERNAL_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_EXTERNAL_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS },
		2
	},
	{PMNT_FunctionScan, QENODE_TYPE_FUNCTION_SCAN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Group, QENODE_TYPE_GROUP,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Hash, QENODE_TYPE_HASH,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_HASH_SPILL_BATCHES, QENODE_MEASURE_HASH_SPILL_TUPLES, QENODE_MEASURE_HASH_SPILL_BYTES},
		{QENODE_UNIT_ROWS, QENODE_UNIT_BATCHES, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES },
		4
	},
	{PMNT_HashJoin, QENODE_TYPE_HASH_JOIN,
		QENODE_HASHJOIN_METRICS,
		QENODE_HASHJOIN_UNITS,
		1
	},
	{PMNT_HashLeftJoin, QENODE_TYPE_HASH_LEFT_JOIN,
		QENODE_HASHJOIN_METRICS,
		QENODE_HASHJOIN_UNITS,
		1
	},
	{PMNT_HashLeftAntiSemiJoin, QENODE_TYPE_HASH_LEFT_ANTI_SEMI_JOIN,
		QENODE_HASHJOIN_METRICS,
		QENODE_HASHJOIN_UNITS,
		1
	},
	{PMNT_HashFullJoin, QENODE_TYPE_HASH_FULL_JOIN,
		QENODE_HASHJOIN_METRICS,
		QENODE_HASHJOIN_UNITS,
		1
	},
	{PMNT_HashRightJoin, QENODE_TYPE_HASH_RIGHT_JOIN,
		QENODE_HASHJOIN_METRICS,
		QENODE_HASHJOIN_UNITS,
		1
	},
	{PMNT_HashExistsJoin, QENODE_TYPE_HASH_EXISTS_JOIN,
		QENODE_HASHJOIN_METRICS,
		QENODE_HASHJOIN_UNITS,
		1
	},
	{PMNT_HashReverseInJoin, QENODE_TYPE_HASH_REVERSE_IN_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_HashUniqueOuterJoin, QENODE_TYPE_HASH_UNIQUE_OUTER_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_HashUniqueInnerJoin, QENODE_TYPE_HASH_UNIQUE_INNER_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_IndexScan, QENODE_TYPE_INDEX_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_INDEX_SCAN_RESTORE_POS, QENODE_MEASURE_INDEX_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESTORES, QENODE_UNIT_RESCANS},
		3
	},
	{PMNT_Limit, QENODE_TYPE_LIMIT,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Materialize, QENODE_TYPE_MATERIALIZE,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_MATERIALIZE_RESCAN},
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS },
		2
	},
	{PMNT_MergeJoin, QENODE_TYPE_MERGE_JOIN,
		QENODE_MERGEJOIN_METRICS,
		QENODE_MERGEJOIN_UNITS,
		1
	},
	{PMNT_MergeLeftJoin, QENODE_TYPE_MERGE_LEFT_JOIN,
		QENODE_MERGEJOIN_METRICS,
		QENODE_MERGEJOIN_UNITS,
		1
	},
	{PMNT_MergeLeftAntiSemiJoin, QENODE_TYPE_MERGE_LEFT_ANTI_SEMI_JOIN,
		QENODE_MERGEJOIN_METRICS,
		QENODE_MERGEJOIN_UNITS,
		1
	},
	{PMNT_MergeFullJoin, QENODE_TYPE_MERGE_FULL_JOIN,
		QENODE_MERGEJOIN_METRICS,
		QENODE_MERGEJOIN_UNITS,
		1
	},
	{PMNT_MergeRightJoin, QENODE_TYPE_MERGE_RIGHT_JOIN,
		QENODE_MERGEJOIN_METRICS,
		QENODE_MERGEJOIN_UNITS,
		1
	},
	{PMNT_MergeExistsJoin, QENODE_TYPE_MERGE_EXISTS_JOIN,
		QENODE_MERGEJOIN_METRICS,
		QENODE_MERGEJOIN_UNITS,
		1
	},
	{PMNT_MergeReverseInJoin, QENODE_TYPE_MERGE_REVERSE_IN_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_MergeUniqueOuterJoin, QENODE_TYPE_MERGE_UNIQUE_OUTER_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_MergeUniqueInnerJoin, QENODE_TYPE_MERGE_UNIQUE_INNER_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_RedistributeMotion, QENODE_TYPE_REDISTRIBUTE_MOTION,
		QENODE_MOTION_METRICS,
		QENODE_MOTION_UNITS,
		1
	},
	{PMNT_BroadcastMotion, QENODE_TYPE_BROADCAST_MOTION,
		QENODE_MOTION_METRICS,
		QENODE_MOTION_UNITS,
		1
	},
	{PMNT_GatherMotion, QENODE_TYPE_GATHER_MOTION,
		QENODE_MOTION_METRICS,
		QENODE_MOTION_UNITS,
		1
	},
	{PMNT_ExplicitRedistributeMotion, QENODE_TYPE_EXPLICIT_REDISTRIBUTE_MOTION,
		QENODE_MOTION_METRICS,
		QENODE_MOTION_UNITS,
		1
	},
	{PMNT_NestedLoop, QENODE_TYPE_NESTED_LOOP,
		QENODE_NESTEDLOOP_METRICS,
		QENODE_NESTEDLOOP_UNITS,
		1
	},
	{PMNT_NestedLoopLeftJoin, QENODE_TYPE_NESTED_LOOP_LEFT_JOIN,
		QENODE_NESTEDLOOP_METRICS,
		QENODE_NESTEDLOOP_UNITS,
		1
	},
	{PMNT_NestedLoopLeftAntiSemiJoin, QENODE_TYPE_NESTED_LOOP_LEFT_ANTI_SEMI_JOIN,
		QENODE_NESTEDLOOP_METRICS,
		QENODE_NESTEDLOOP_UNITS,
		1
	},
	{PMNT_NestedLoopFullJoin, QENODE_TYPE_NESTED_LOOP_FULL_JOIN,
		QENODE_NESTEDLOOP_METRICS,
		QENODE_NESTEDLOOP_UNITS,
		1
	},
	{PMNT_NestedLoopRightJoin, QENODE_TYPE_NESTED_LOOP_RIGHT_JOIN,
		QENODE_NESTEDLOOP_METRICS,
		QENODE_NESTEDLOOP_UNITS,
		1
	},
	{PMNT_NestedLoopExistsJoin, QENODE_TYPE_NESTED_LOOP_EXISTS_JOIN,
		QENODE_NESTEDLOOP_METRICS,
		QENODE_NESTEDLOOP_UNITS,
		1
	},
	{PMNT_NestedLoopReverseInJoin, QENODE_TYPE_NESTED_LOOP_REVERSE_IN_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_NestedLoopUniqueOuterJoin, QENODE_TYPE_NESTED_LOOP_UNIQUE_OUTER_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_NestedLoopUniqueInnerJoin, QENODE_TYPE_NESTED_LOOP_UNIQUE_INNER_JOIN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_PartitionSelector, QENODE_TYPE_PARTITIONSELECTOR,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Result, QENODE_TYPE_RESULT,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Repeat, QENODE_TYPE_REPEAT,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_RowTrigger,
		QENODE_TYPE_ROWTRIGGER,
		{QENODE_MEASURE_ROWS_IN},
		{QENODE_UNIT_ROWS}
	},
	{PMNT_SeqScan, QENODE_TYPE_SEQ_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_SEQ_SCAN_PAGE_STATS, QENODE_MEASURE_SEQ_SCAN_RESTORE_POS, QENODE_MEASURE_SEQ_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_PAGES, QENODE_UNIT_RESTORES, QENODE_UNIT_RESCANS },
		4
	},
	{PMNT_Sequence, QENODE_TYPE_SEQUENCE,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS }
	},
	{PMNT_SetOp, QENODE_TYPE_SETOP,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_SetOpIntersect, QENODE_TYPE_SETOP_INTERSECT,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_SetOpIntersectAll, QENODE_TYPE_SETOP_INTERSECT_ALL,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_SetOpExcept, QENODE_TYPE_SETOP_EXCEPT,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_SetOpExceptAll, QENODE_TYPE_SETOP_EXCEPT_ALL,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_SharedScan, QENODE_TYPE_SHARED_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_SHARED_SCAN_RESTORE_POS, QENODE_MEASURE_SHARED_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESTORES, QENODE_UNIT_RESCANS },
		3
	},
	{PMNT_Sort, QENODE_TYPE_SORT,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_SORT_MEMORY_USAGE, QENODE_MEASURE_SORT_SPILL_TUPLES, QENODE_MEASURE_SORT_SPILL_BYTES, QENODE_MEASURE_SORT_SPILL_PASS, QENODE_MEASURE_SORT_CURRENT_SPILL_PASS_TUPLES, QENODE_MEASURE_SORT_CURRENT_SPILL_PASS_BYTES},
		{QENODE_UNIT_ROWS, QENODE_UNIT_BYTES, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES, QENODE_UNIT_PASSES, QENODE_UNIT_TUPLES, QENODE_UNIT_BYTES },
		7
	},
	{PMNT_SplitUpdate, QENODE_TYPE_SPLITUPDATE,
		{QENODE_MEASURE_ROWS_IN},
		{QENODE_UNIT_ROWS}
	},
	{PMNT_SubqueryScan, QENODE_TYPE_SUBQUERY_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_SUBQUERY_SCAN_RESCAN},
		{QENODE_UNIT_ROWS, QENODE_UNIT_RESCANS},
		2
	},
	{PMNT_TableFunctionScan, QENODE_TYPE_TABLE_FUNCTION_SCAN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_TableScan, QENODE_TYPE_TABLE_SCAN,
		{QENODE_MEASURE_ROWS_IN, QENODE_MEASURE_TABLE_SCAN_PAGE_STATS, QENODE_MEASURE_TABLE_SCAN_RESTORE_POS, QENODE_MEASURE_TABLE_SCAN_RESCAN },
		{QENODE_UNIT_ROWS, QENODE_UNIT_PAGES, QENODE_UNIT_RESTORES, QENODE_UNIT_RESCANS }
	},
	{PMNT_TidScan, QENODE_TYPE_TID_SCAN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Unique, QENODE_TYPE_UNIQUE ,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_ValuesScan, QENODE_TYPE_VALUES_SCAN,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
	{PMNT_Window, QENODE_TYPE_WINDOW,
		{QENODE_MEASURE_ROWS_IN },
		{QENODE_UNIT_ROWS },
		1
	},
};

/* this is a debug function to validate the string lookup table datastructures
   it should be run one time only at startup */
apr_status_t gpdb_debug_string_lookup_table()
{
	int i;
	for (i = PMNT_Invalid + 1; i < PMNT_MAXIMUM_ENUM; ++i)
	{
		QenodeNamesDescriptor* desc = &nodetype_string_lookup[i];
		int j;

		if (desc->type != i)
		{
			gpmon_warning(FLINE, "gpmmon nodetype_string_lookup out of sync at type %d with value %d", i, desc->type);
			return APR_NOTFOUND;
		}
		if (!desc->nodename)
		{
			gpmon_warning(FLINE, "gpmmon missing nodetype description for type: %d\n", desc->type);
			return APR_NOTFOUND;
		}

		for (j = 0; j < GPMON_QEXEC_M_COUNT; ++j)
		{
			if (!desc->measurenames[j])
			{
				break;
			}
			if (!desc->measureunits[j])
			{
				gpmon_warning(FLINE, "gpmmon nodetype %s missing units for measurement: %d\n", desc->nodename, j);
				return APR_NOTFOUND;
			}

			/* enable this for debugging help only */
			/* gpmon_print("gpmmon %s \"%s\" \"%s\"\n", desc->nodename, desc->measurenames[j], desc->measureunits[j]);  */
		}
		if (!j)
		{
			gpmon_warning(FLINE, "gpmmon missing node measurements for nodetype %s\n", desc->nodename);
			return APR_NOTFOUND;
		}
	}

	return APR_SUCCESS;
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

unsigned int gpdb_getnode_number_metrics(PerfmonNodeType type)
{
	QenodeNamesDescriptor* desc;
	if( PMNT_MAXIMUM_ENUM <= type)
	{
		TR0( ("gpdb_getnode_number_metrics: Error node type out of range: %d >= %d\n", type, PMNT_MAXIMUM_ENUM));
		return 0;
	}
	desc = &nodetype_string_lookup[type];
	return desc->number_metrics;
}

const char* gpdb_getnodename(PerfmonNodeType type)
{
	QenodeNamesDescriptor* desc;
	if( PMNT_MAXIMUM_ENUM <= type)
	{
		TR0( ("gpdb_getnodename: Error node type out of range: %d >= %d\n", type, PMNT_MAXIMUM_ENUM));
		return 0;
	}
	desc = &nodetype_string_lookup[type];
	return desc->nodename;
}

apr_status_t gpdb_getnode_metricinfo(PerfmonNodeType type, apr_byte_t metricnum, const char** name, const char** unit)
{
	QenodeNamesDescriptor* desc;
	if( PMNT_MAXIMUM_ENUM <= type)
	{
		TR0( ("gpdb_getnode_metricinfo: Error node type out of range: %d >= %d\n", type, PMNT_MAXIMUM_ENUM));
		return APR_BADARG;
	}
	desc = &nodetype_string_lookup[type];
	if (!desc->measurenames[metricnum])
	{
		*name = NULL;
		*unit = NULL;
		return APR_NOTFOUND;
	}
	else
	{
		*name = desc->measurenames[metricnum];
		*unit = desc->measureunits[metricnum];

		assert(desc->measureunits[metricnum]);

		return APR_SUCCESS;
	}
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

