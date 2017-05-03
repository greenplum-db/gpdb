#ifndef GPMONLIB_H
#define GPMONLIB_H

#undef GP_VERSION
#include "postgres_fe.h"

#include "apr_general.h"
#include "apr_time.h"
#include "event.h"
#include "gpmon/gpmon.h"

#define BATCH 8
#define DEFAULT_GPMMON_LOGDIR "gpperfmon/logs"

#ifndef WIN32
typedef int SOCKET;
#endif

#define GPMON_DBUSER "gpmon"

#define GPMON_PACKET_ERR_LOG_TIME 60

#define FMT64 APR_INT64_T_FMT
#define FMTU64 APR_UINT64_T_FMT
#define FLINE __FILE__ ":" APR_STRINGIFY(__LINE__)
#define CHECKMEM(x)  if (x) ; else gpmon_fatal(FLINE, "out of memory")
#define ASSERT(x)    if (x) ; else gpmon_fatal(FLINE, "Check condition:%s failed", #x)

extern int verbose;
/* TODO: REMOVE */
//extern int very_verbose;
#define TR0(x) gpmon_print x
#define TR1(x) if (verbose == 1) gpmon_print x
#define TR2(x) if (verbose == 2) gpmon_print x
#define TR1_FILE(x) if (verbose == 1) gpmon_print_file x

/* Architecture specific limits for metrics */
#if defined(osx104_x86) || defined(osx105_x86) || defined(rhel4_x86_32) || defined(rhel5_x86_32)
	#define GPSMON_METRIC_MAX 0xffffffffUL
#elif defined(rhel4_x86_64) || defined(rhel5_x86_64) || defined(rhel7_x86_64) || defined(rhel6_x86_64) || defined(sol10_x86_64) || defined(suse10_x86_64)
	#define GPSMON_METRIC_MAX 0xffffffffffffffffULL
#else
	#define GPSMON_METRIC_MAX 0xffffffffUL
#endif

#define GPMON_DATE_BUF_SIZE 24


/* fatal & warning messages */
extern int gpmon_print(const char* fmt, ...);
extern int gpmon_fatal(const char* fline, const char* fmt, ...);
extern int gpmon_fatalx(const char* fline, int e, const char* fmt, ...);
extern int gpmon_warning(const char* fline, const char* fmt, ...);
extern int gpmon_warningx(const char* fline, int e, const char* fmt, ...);
extern void gpmon_print_file(const char* header_line, FILE* fp);

// fatal messages for smon -- go to stdout only
extern int gpsmon_fatal(const char* fline, const char* fmt, ...);
extern int gpsmon_fatalx(const char* fline, int e, const char* fmt, ...);

/* convert packets to host order */
extern apr_status_t gpmon_ntohpkt(apr_int32_t magic, apr_int16_t version, apr_int16_t pkttype);

/* get the size of the union packet for smon_to_mon packets*/
extern size_t get_size_by_pkttype_smon_to_mmon(apr_int16_t pkttype);

/* strings */
extern char* gpmon_trim(char* s);

/* file manip */
extern int gpmon_recursive_mkdir(char* work_dir);

/* datetime, e.g. 2004-02-14  23:50:02 */
extern char* gpmon_datetime(time_t t, char str[GPMON_DATE_BUF_SIZE]);

/* version that rounds to lowest 5 sec interval */
extern char* gpmon_datetime_rounded(time_t t, char str[GPMON_DATE_BUF_SIZE]);

/* utility */
extern apr_int32_t get_query_status(apr_int32_t tmid, apr_int32_t ssid, apr_int32_t ccnt);
extern char *get_query_text(apr_int32_t tmid, apr_int32_t ssid, apr_int32_t ccnt, apr_pool_t *pool);

#define DEFAULT_PATH_TO_HADOOP_HOST_FILE "/etc/gphd/gphdmgr/conf/clusterinfo.txt"
#define PATH_TO_HADOOP_SMON_LOGS "/var/log/gphd/smon"

#define MIN_MESSAGES_PER_INTERVAL (1)
#define MAX_MESSAGES_PER_INTERVAL (50)
#define MINIMUM_MESSAGE_INTERVAL  (1) // in minutes
#define MAXIMUM_MESSAGE_INTERVAL  (10080) //one week

#define MAX_QUERY_COMPARE_LENGTH  (1024 * 1024 * 10)
/*
* The below is a simple divide macro that does division rounding up if you get .5 or greater or down if you get below .5 without
* doing any floating point math.  It can be used instead of the round math.h functions to avoid floating point math and crazy casting.
* This is an example of how it works: 9/5 = 1.8 so you want the answer to be 2
* the macro does the following: It first does the basic division which always rounds down: (9/5) = 1.  Then based on the remainder it
* figures out whether or not to add 1.  Mod is used to calculate the remainder (9 mod 5) = 4.  Then it needs to calculate whether
* 4/5 is greater or equal to .5 : it does this by calculating whether or not the remainder is greater than or equal to half the value of
* 5.  This is done by dividing  5/2 and rounding appropriately (this is done by adding the remainder (5 mod 2) which is either 1 or 0). 5/2
* rounded is 3 which is less than the remainder 4, so it rounds up and adds 1 getting 2 as the answer.
* example 2, for 9/7 = 1.29 so you want the answer to be 1
* step 1: (((9)/(7)) + ((((9)%(7))>=((((7)/2))+((7)%2)))?1:0))
* step 2: 1 + ((2>=(3+1))?1:0))  step 3: 1 + ((2>=(4)?1:0))   step 4: 1 + 0  step 5: 1
*
* NOTE: THIS IS WELL TESTED; DO NOT CHANGE!!!
*/
#define ROUND_DIVIDE(numerator, denominator) (((numerator)/(denominator)) + ((((numerator)%(denominator))>=((((denominator)/2))+((denominator)%2)))?1:0))




/* gpmmon options */
typedef struct mmon_options_t
{
	int argc;
	const char* const *argv;
	const char* pname;
	char* gpdb_port;
	char* conf_file;
	char* log_dir;
	char* smon_log_dir;
	char* smon_hadoop_swonly_clusterfile;
	char* smon_hadoop_swonly_logdir;
	char* smon_hadoop_swonly_binfile;
	char* smdw_aliases;
	apr_uint64_t max_log_size;
	int max_fd; /* this is the max fd value we ever seen */
	int v;
	int q;
	int m;
	int d;
	int qamode;
	int harvest_interval;
	apr_uint64_t tail_buffer_max;
	int console;
	int warning_disk_space_percentage;
	int error_disk_space_percentage;
	time_t disk_space_interval; // interval in seconds
	unsigned int max_disk_space_messages_per_interval;
	int iterator_aggregate;
	int partition_age;  		// in month 
	bool ignore_qexec_packet;
} mmon_options_t;

typedef struct addressinfo_holder_t addressinfo_holder_t;
struct addressinfo_holder_t                                                                        
{
	char* address;  // an alternate host name to access this host                                                                           
	char* ipstr; // the ipstring assoicated with this address
	bool ipv6;
	struct addressinfo_holder_t* next;
};

typedef struct multi_interface_holder_t multi_interface_holder_t;
struct multi_interface_holder_t
{
	addressinfo_holder_t* current; 
	unsigned int counter;
};

#define CONM_INTERVAL (16)
#define CONM_LOOP_LAUNCH_FRAME (1)
#define CONM_LOOP_BROKEN_FRAME (9)
#define CONM_LOOP_HANG_FRAME   (12)

#define GPSMON_TIMEOUT_NONE     (0)
#define GPSMON_TIMEOUT_RESTART  (1)
#define GPSMON_TIMEOUT_DETECTED (2)

/* segment host */ 
typedef struct host_t
{
	apr_thread_mutex_t *mutex; 
	int sock; /* socket connected to gpsmon on this host */ 
	int eflag; /* flag: socket has error */
	struct event* event; /* points to _event if set */
	struct event _event; 

	char* hostname; 

	addressinfo_holder_t* addressinfo_head; 
	addressinfo_holder_t* addressinfo_tail; 

	// there are 2 of these so we don't need to mutex
	multi_interface_holder_t connection_hostname;	

	apr_uint32_t address_count; 
	char* smon_bin_location;
	char* data_dir;
	unsigned char is_master; /* 1 if host is the same host where the master runs */ 
	unsigned char is_hdm; 
	unsigned char is_hdw; 
	unsigned char is_hbw; 
	unsigned char is_hdc;
	unsigned char is_etl; 
	char ever_connected; /* set to non-zero after first connection attempt */
	char connect_timeout;
	apr_int32_t pid;
} host_t;

#define QEXEC_MAX_ROW_BUF_SIZE (2048)

typedef struct qexec_packet_data_t
{
	gpmon_qexeckey_t 	key;
	apr_uint64_t 		rowsout;
	apr_uint64_t		_cpu_elapsed; /* CPU elapsed for iter */
	apr_uint64_t 		measures_rows_in;
	apr_uint16_t		size_of_line; //the size of the string plus the null terminator
} qexec_packet_data_t;

typedef struct qexec_packet_t
{
	qexec_packet_data_t data;
	char* 				line;

} qexec_packet_t;

typedef struct gp_smon_to_mmon_header_t {
	/* if you modify this, do not forget to edit gpperfmon/src/gpmon/gpmonlib.c:gpmon_ntohpkt() */
	apr_int16_t pkttype;
	apr_int32_t magic;
	apr_int16_t version;
} gp_smon_to_mmon_header_t;

typedef struct gp_smon_to_mmon_packet_t {
	gp_smon_to_mmon_header_t header;
	union {
		gpmon_hello_t   hello;
		gpmon_metrics_t metrics;
		gpmon_qlog_t    qlog;
		qexec_packet_t  qexec_packet;
		gpmon_seginfo_t seginfo;
		gpmon_fsinfo_t fsinfo;
		gpmon_query_seginfo_t queryseg;
	} u;
} gp_smon_to_mmon_packet_t;

char* get_connection_hostname(host_t* host);
char* get_connection_ip(host_t* host);
bool get_connection_ipv6_status(host_t* host);
void advance_connection_hostname(host_t* host);

double subtractTimeOfDay(struct timeval* begin, struct timeval* end);

/* Set header*/
extern void gp_smon_to_mmon_set_header(gp_smon_to_mmon_packet_t* pkt, apr_int16_t pkttype);

unsigned int gpdb_getnode_number_metrics(PerfmonNodeType type);
const char* gpdb_getnodename(PerfmonNodeType type);
const char* gpdb_getnodestatus(PerfmonNodeStatus status);
apr_status_t gpdb_getnode_metricinfo(PerfmonNodeType type, apr_byte_t metricnum, const char** name, const char** unit);
apr_status_t gpdb_debug_string_lookup_table(void);
apr_status_t apr_pool_create_alloc(apr_pool_t ** newpool, apr_pool_t *parent);
void gpdb_get_single_string_from_query(const char* QUERY, char** resultstring, apr_pool_t* pool);
#endif /* GPMONLIB_H */
