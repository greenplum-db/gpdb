#ifndef GPMON_H
#define GPMON_H

extern void gpmon_init(void);

extern int64 gpmon_tick;
typedef struct gpmon_packet_t gpmon_packet_t;
typedef struct gpmon_qlogkey_t gpmon_qlogkey_t;
typedef struct gpmon_qlog_t gpmon_qlog_t;
typedef struct gpmon_qexec_t gpmon_qexec_t;
typedef struct gpmon_hello_t gpmon_hello_t;
typedef struct gpmon_metrics_t gpmon_metrics_t;
typedef struct gpmon_seginfo_t gpmon_seginfo_t;
typedef struct gpmon_fsinfokey_t gpmon_fsinfokey_t;
typedef struct gpmon_fsinfo_t gpmon_fsinfo_t;
typedef struct gpmon_query_seginfo_key_t gpmon_query_seginfo_key_t;
typedef struct gpmon_query_seginfo_t gpmon_query_seginfo_t;

/*
 * this dir sits in $MASTER_DATA_DIRECTORY. always include the
 * suffix /
 */
#define GPMON_DIR   "./gpperfmon/data/"
#define GPMON_LOG   "./gpperfmon/logs/"
#define GPMON_ALERT_LOG_STAGE    "alert_log_stage"
#define GPMON_DIR_MAX_PATH 100
#define GPMON_DB    "gpperfmon"

#define GPMON_FSINFO_MAX_PATH 255
#define GPMON_UNKNOWN "Unknown"

/*
this is enough space for 2 names plus a . between the names plus a null char at the end of the string
for example SCHEMA.RELATION\0
*/
#define SCAN_REL_NAME_BUF_SIZE (NAMEDATALEN*2)


/* ------------------------------------------------------------------
         INTERFACE
   ------------------------------------------------------------------ */

extern void gpmon_qlog_packet_init(gpmon_packet_t *gpmonPacket);
extern void gpmon_qlog_query_submit(gpmon_packet_t *gpmonPacket);
extern void gpmon_qlog_query_text(const gpmon_packet_t *gpmonPacket,
		const char *queryText,
		const char *appName,
		const char *resqName,
		const char *resqPriority);
extern void gpmon_qlog_query_start(gpmon_packet_t *gpmonPacket);
extern void gpmon_qlog_query_end(gpmon_packet_t *gpmonPacket);
extern void gpmon_qlog_query_error(gpmon_packet_t *gpmonPacket);
extern void gpmon_qlog_query_canceling(gpmon_packet_t *gpmonPacket);
extern void gpmon_send(gpmon_packet_t*);
extern void gpmon_gettmid(int32*);

/* ------------------------------------------------------------------
         FSINFO
   ------------------------------------------------------------------ */

struct gpmon_fsinfokey_t
{
	char fsname [GPMON_FSINFO_MAX_PATH];
	char hostname[NAMEDATALEN];
};

struct gpmon_fsinfo_t
{
	gpmon_fsinfokey_t key;

	int64 bytes_used;
	int64 bytes_available;
	int64 bytes_total;
};

/* ------------------------------------------------------------------
         METRICS
   ------------------------------------------------------------------ */
struct gpmon_metrics_t
{
	/* if you modify this, do not forget to edit gpperfmon/src/gpmon/gpmonlib.c:gpmon_ntohpkt() */

	char hname[NAMEDATALEN];
	struct
	{
		uint64 total, used, actual_used, actual_free;
	} mem;

	struct
	{
		uint64 total, used, page_in, page_out;
	} swap;

	struct
	{
		float user_pct, sys_pct, idle_pct;
	} cpu;

	struct
	{
		float value[3];
	} load_avg;

	struct
	{
		uint64 ro_rate, wo_rate, rb_rate, wb_rate;
	} disk;

	struct
	{
		uint64 rp_rate, wp_rate, rb_rate, wb_rate;
	} net;
};


/* ------------------------------------------------------------------
         QLOG
   ------------------------------------------------------------------ */

struct gpmon_qlogkey_t {
    /* if you modify this, do not forget to edit gpperfmon/src/gpmon/gpmonlib.c:gpmon_ntohpkt() */
	int32 tmid;  /* transaction time */
    int32 ssid; /* session id */
    int32 ccnt; /* command count */
};

/* ------------------------------------------------------------------
         QUERY SEGINFO
   ------------------------------------------------------------------ */
struct gpmon_query_seginfo_key_t
{
	gpmon_qlogkey_t		qkey;
	int16		segid; /* segment id */
};

struct gpmon_query_seginfo_t
{
	gpmon_query_seginfo_key_t	key;
	/*
	 * final rowsout for segid = -1 and sliceid = 1, otherwise -1
	 * if not exist for this segment.
	 */
	int64				final_rowsout;
	uint64				sum_cpu_elapsed;
	uint64				sum_measures_rows_in;
};

/* process metrics ... filled in by gpsmon */
typedef struct gpmon_proc_metrics_t gpmon_proc_metrics_t;
struct gpmon_proc_metrics_t {
    uint32 fd_cnt;		/* # opened files / sockets etc */
    float        cpu_pct;	/* cpu usage % */
    struct {
		uint64 size, resident, share;
    } mem;
};


#define GPMON_QLOG_STATUS_INVALID -1
#define GPMON_QLOG_STATUS_SILENT   0
#define GPMON_QLOG_STATUS_SUBMIT   1
#define GPMON_QLOG_STATUS_START    2
#define GPMON_QLOG_STATUS_DONE     3
#define GPMON_QLOG_STATUS_ERROR    4
#define GPMON_QLOG_STATUS_CANCELING 5

#define GPMON_NUM_SEG_CPU 10

struct gpmon_qlog_t
{
	gpmon_qlogkey_t key;
	char        user[NAMEDATALEN];
	char        db[NAMEDATALEN];
	int32 tsubmit, tstart, tfin;
	int32 status;		/* GPMON_QLOG_STATUS_XXXXXX */
	int32 cost;
	int64 cpu_elapsed; /* CPU elapsed for query */
	gpmon_proc_metrics_t p_metrics;
};


/* ------------------------------------------------------------------
         QEXEC
   ------------------------------------------------------------------ */

typedef struct gpmon_qexec_hash_key_t {
	int16 segid;	/* segment id */
	int32 pid; 	/* process id */
	int16 nid;	/* plan node id */
}gpmon_qexec_hash_key_t;

/* XXX According to CK.
 * QE will NOT need to touch anything begin with _
 */
typedef struct gpmon_qexeckey_t {
    /* if you modify this, do not forget to edit gpperfmon/src/gpmon/gpmonlib.c:gpmon_ntohpkt() */
    int32 tmid;  /* transaction time */
    int32 ssid; /* session id */
    int16 ccnt;	/* command count */
    gpmon_qexec_hash_key_t hash_key;
}gpmon_qexeckey_t;


enum {
	GPMON_QEXEC_M_ROWSIN = 0,
#if 0
	GPMON_QEXEC_M_BYTESIN,
	GPMON_QEXEC_M_BYTESOUT,
#endif
	GPMON_QEXEC_M_NODE_START,
	GPMON_QEXEC_M_COUNT = 10,
	GPMON_QEXEC_M_DBCOUNT = 16
};

/* there is an array of 10 measurements in the array inside a gpmon packet
   however the gpperfmon DB will contain 16 measurements.
   long term we need a more extensible solution. short term we limit size of packets
   to what is actually used to conserve resources
*/

struct gpmon_qexec_t {
	/* if you modify this, do not forget to edit gpperfmon/src/gpmon/gpmonlib.c:gpmon_ntohpkt() */
	gpmon_qexeckey_t key;
	int32  		pnid;	/* plan parent node id */
	char		_hname[NAMEDATALEN];
	uint16		nodeType; 					/* using enum PerfmonNodeType */
	uint8		status;    /* node status using PerfmonNodeStatus */
	int32 		tstart, tduration; /* start (wall clock) time and duration.  XXX Leave as 0 for now. */
	uint64 		p_mem, p_memmax;   /* XXX Aset instrumentation.  Leave as 0 for now. */
	uint64		_cpu_elapsed; /* CPU elapsed for iter */
	gpmon_proc_metrics_t 	_p_metrics;
	uint64 		rowsout, rowsout_est;
	uint64 		measures[GPMON_QEXEC_M_COUNT];
	char		relation_name[SCAN_REL_NAME_BUF_SIZE];
};

/*
 * Segment-related statistics
 */
struct gpmon_seginfo_t {
	int32 dbid; 							// dbid as in gp_segment_configuration
	char hostname[NAMEDATALEN];					// hostname without NIC extension
	uint64 dynamic_memory_used;			// allocated memory in bytes
	uint64 dynamic_memory_available;		// available memory in bytes,
};

/* ------------------------------------------------------------------
         HELLO
   ------------------------------------------------------------------ */

struct gpmon_hello_t {
    int64 signature;
    int32 pid; /* pid of gpsmon */
};



#define GPMON_MAGIC     0x78ab928d

/*
 *  When updating the format of the packets update these variables
 *  in order to make sure that the communication between the GPDB
 *  and perfmon is compatible
 */
#define GPMON_PACKET_VERSION   5
#define GPMMON_PACKET_VERSION_STRING "gpmmon packet version 5\n"

enum gpmon_pkttype_t {
    GPMON_PKTTYPE_NONE = 0,
    GPMON_PKTTYPE_HELLO = 1,
    GPMON_PKTTYPE_METRICS = 2,
    GPMON_PKTTYPE_QLOG = 3,
    GPMON_PKTTYPE_QEXEC = 4,
    GPMON_PKTTYPE_SEGINFO = 5,
    GPMON_PKTTYPE_QUERY_HOST_METRICS = 7, // query metrics update from a segment such as CPU per query
    GPMON_PKTTYPE_FSINFO = 8,
    GPMON_PKTTYPE_QUERYSEG = 9,

    GPMON_PKTTYPE_MAX
};



struct gpmon_packet_t {
    /* if you modify this, do not forget to edit gpperfmon/src/gpmon/gpmonlib.c:gpmon_ntohpkt() */
    int32 magic;
    int16 version;
    int16 pkttype;
    union {
		gpmon_hello_t   hello;
		gpmon_metrics_t metrics;
		gpmon_qlog_t    qlog;
		gpmon_qexec_t   qexec;
		gpmon_seginfo_t seginfo;
		gpmon_fsinfo_t fsinfo;
    } u;
};


extern const char* gpmon_qlog_status_string(int gpmon_qlog_status);

/* when adding a node type for perfmon display be sure to also update the corresponding structures in
   in gpperfmon/src/gpmon/gpmonlib.c */

typedef enum PerfmonNodeType
{
    PMNT_Invalid = 0,
    PMNT_Append,
    PMNT_AppendOnlyScan,
    PMNT_AssertOp,
    PMNT_Aggregate,
    PMNT_GroupAggregate,
    PMNT_HashAggregate,
    PMNT_AppendOnlyColumnarScan,
    PMNT_BitmapAnd,
    PMNT_BitmapOr,
    PMNT_BitmapAppendOnlyScan,
    PMNT_BitmapHeapScan,
    PMNT_BitmapTableScan,
    PMNT_BitmapIndexScan,
    PMNT_DML,
    PMNT_DynamicIndexScan,
    PMNT_DynamicTableScan,
    PMNT_ExternalScan,
    PMNT_FunctionScan,
    PMNT_Group,
    PMNT_Hash,
    PMNT_HashJoin,
    PMNT_HashLeftJoin,
    PMNT_HashLeftAntiSemiJoin,
    PMNT_HashFullJoin,
    PMNT_HashRightJoin,
    PMNT_HashExistsJoin,
    PMNT_HashReverseInJoin,
    PMNT_HashUniqueOuterJoin,
    PMNT_HashUniqueInnerJoin,
    PMNT_IndexScan,
    PMNT_Limit,
    PMNT_Materialize,
    PMNT_MergeJoin,
    PMNT_MergeLeftJoin,
    PMNT_MergeLeftAntiSemiJoin,
    PMNT_MergeFullJoin,
    PMNT_MergeRightJoin,
    PMNT_MergeExistsJoin,
    PMNT_MergeReverseInJoin,
    PMNT_MergeUniqueOuterJoin,
    PMNT_MergeUniqueInnerJoin,
    PMNT_RedistributeMotion,
    PMNT_BroadcastMotion,
    PMNT_GatherMotion,
    PMNT_ExplicitRedistributeMotion,
    PMNT_NestedLoop,
    PMNT_NestedLoopLeftJoin,
    PMNT_NestedLoopLeftAntiSemiJoin,
    PMNT_NestedLoopFullJoin,
    PMNT_NestedLoopRightJoin,
    PMNT_NestedLoopExistsJoin,
    PMNT_NestedLoopReverseInJoin,
    PMNT_NestedLoopUniqueOuterJoin,
    PMNT_NestedLoopUniqueInnerJoin,
    PMNT_PartitionSelector,
    PMNT_Result,
    PMNT_Repeat,
    PMNT_RowTrigger,
    PMNT_SeqScan,
    PMNT_Sequence,
    PMNT_SetOp,
    PMNT_SetOpIntersect,
    PMNT_SetOpIntersectAll,
    PMNT_SetOpExcept,
    PMNT_SetOpExceptAll,
    PMNT_SharedScan,
    PMNT_Sort,
    PMNT_SplitUpdate,
    PMNT_SubqueryScan,
    PMNT_TableFunctionScan,
    PMNT_TableScan,
    PMNT_TidScan,
    PMNT_Unique,
    PMNT_ValuesScan,
    PMNT_Window,
	PMNT_MAXIMUM_ENUM

} PerfmonNodeType;

typedef enum PerfmonNodeStatus
{
	PMNS_Initialize = 0,
	PMNS_Executing,
	PMNS_Finished

} PerfmonNodeStatus;

#endif
