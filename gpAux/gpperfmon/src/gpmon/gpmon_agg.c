#undef GP_VERSION
#include "postgres_fe.h"

#include "apr_general.h"
#include "apr_hash.h"
#include "apr_time.h"
#include "apr_queue.h"
#include "apr_strings.h"
#include "gpmon/gpmon.h"
#include "gpmon_agg.h"
#include "gpmondb.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

typedef enum disk_space_message_t
{
	DISK_SPACE_NO_MESSAGE_SENT = 0,
	DISK_SPACE_WARNING_SENT ,
	DISK_SPACE_ERROR_SENT
} disk_space_message_t;

typedef struct mmon_fsinfo_t
{
	gpmon_fsinfokey_t key;

	apr_int64_t				bytes_used;
	apr_int64_t				bytes_available;
	apr_int64_t				bytes_total;
	disk_space_message_t	sent_error_flag;
	time_t					last_update_timestamp;
} mmon_fsinfo_t; //the fsinfo structure used in mmon

typedef struct mmon_qexec_t
{
	gpmon_qexeckey_t 	key;
	apr_uint64_t 		rowsout;
	apr_uint64_t		_cpu_elapsed; /* CPU elapsed for iter */
	apr_uint64_t 		measures_rows_in;
} mmon_qexec_t;  //The qexec structure used in mmon

typedef struct mmon_query_seginfo_t
{
	gpmon_query_seginfo_key_t	key;
	apr_int64_t					final_rowsout;
	apr_uint64_t				sum_cpu_elapsed;
	apr_uint64_t				sum_measures_rows_out;
} mmon_query_seginfo_t;  //The agg value at segment level for query

typedef struct qdnode_t {
	apr_int64_t last_updated_generation;
	int recorded;
	int num_metrics_packets;
	gpmon_qlog_t qlog;
	apr_hash_t* qexec_hash;
	apr_hash_t*	query_seginfo_hash;
} qdnode_t;

struct agg_t
{
	apr_int64_t generation;
	apr_pool_t* pool;
	apr_pool_t* parent_pool;
	apr_hash_t* qtab;		/* key = gpmon_qlog_key_t, value = qdnode ptr. */
	apr_hash_t* htab;		/* key = hostname, value = gpmon_metrics_t ptr */
	apr_hash_t* stab;		/* key = databaseid, value = gpmon_seginfo_t ptr */
	apr_hash_t* fsinfotab;	/* This is the persistent fsinfo hash table: key = gpmon_fsinfokey_t, value = mmon_fsinfo_t ptr */
};

typedef struct dbmetrics_t {
	apr_int32_t queries_total;
	apr_int32_t queries_running;
	apr_int32_t queries_queued;
} dbmetrics_t;

extern int min_query_time;
extern mmon_options_t opt;
extern apr_queue_t* message_queue;

extern void incremement_tail_bytes(apr_uint64_t bytes);
static bool is_query_not_active(apr_int32_t tmid, apr_int32_t ssid,
			apr_int32_t ccnt, apr_hash_t *hash, apr_pool_t *pool);

/**
 * Disk space check helper function
 * Note- trys to push a message on a queue so that the message thread can send the message
 */
static apr_status_t  check_disk_space(mmon_fsinfo_t* rec)
{
	static time_t interval_start_time = 0;
	static unsigned int number_messages_sent_this_interval = 0;
	time_t now = 0;
	int used_disk_space_percent =  ROUND_DIVIDE((rec->bytes_used *100),rec->bytes_total);

	now = time(NULL);
	// reset the interval if needed
	if ((now - interval_start_time) >= opt.disk_space_interval){
		interval_start_time = now;
		number_messages_sent_this_interval = 0;
	}

	// Check the disk space if we haven't already sent an error
	if (rec->sent_error_flag != DISK_SPACE_ERROR_SENT) {
		disk_space_message_t send_flag = DISK_SPACE_NO_MESSAGE_SENT;
		char* message = 0;

		// check for errors and then warnings
		if ((opt.error_disk_space_percentage != 0) && (used_disk_space_percent >= opt.error_disk_space_percentage)) {
			//Send an error if the error_disk_space_percentage threshold is set and the used_disk_space_percent is greater or equal to it
			send_flag = DISK_SPACE_ERROR_SENT;
			message = "ERROR";
		} else if ((rec->sent_error_flag != DISK_SPACE_WARNING_SENT) && (opt.warning_disk_space_percentage != 0 ) &&
					(used_disk_space_percent >= opt.warning_disk_space_percentage)) {
			//Send warning if the warning_disk_space_percentage threshold is set and the used_disk_space_percent is greater or equal to it
			//and if a warning has not already been sent
			send_flag = DISK_SPACE_WARNING_SENT;
			message = "WARNING";
		} else if ((rec->sent_error_flag == DISK_SPACE_WARNING_SENT) && (used_disk_space_percent < opt.warning_disk_space_percentage)) {
			//if a warning as been sent and the used disk has fallen below the below the warning threshold reset the send flag
			rec->sent_error_flag = DISK_SPACE_NO_MESSAGE_SENT;
		}

		// Send a warning or error if needed by putting the message in a queue
		if (send_flag != DISK_SPACE_NO_MESSAGE_SENT){
			//only sent the message if
			if (number_messages_sent_this_interval < opt.max_disk_space_messages_per_interval) {
				char *query;
				apr_status_t status;
				unsigned int query_size_max = NAMEDATALEN + GPMON_FSINFO_MAX_PATH + 200;

				query = malloc(query_size_max);
				if (!query) {
					TR0(("check_disk_space ERROR: malloc(%d) returned NULL, out of memory\n", query_size_max));
					return APR_ENOMEM;
				}
				snprintf(query, query_size_max, "select gp_elog('%s: percent used disk space for %s %s is %d%%', True)",
						message, rec->key.hostname, rec->key.fsname, used_disk_space_percent);

				status = apr_queue_trypush(message_queue, (void *) query);
				if (status == APR_EINTR) { //blocking interrupted try one more time
					status = apr_queue_trypush(message_queue, (void *) query);
				}
				if (status != APR_SUCCESS) {
					TR0(("check_disk_space ERROR: apr_queue_trypush returned %d; cannot send %s\n", status, query));
					free(query);
				} else {
					number_messages_sent_this_interval++;
				}

			} else {
				TR1(("check_disk_space: message max reached: Not sending message for %s %s. used_disk_space_percent = %d%%\n", rec->key.hostname, rec->key.fsname, used_disk_space_percent));
			}

			rec->sent_error_flag = send_flag;
		}

	} else if ( ( opt.warning_disk_space_percentage != 0 ) && ( used_disk_space_percent < opt.warning_disk_space_percentage )) {
		//if there is a warning percent to check and the used disk has fallen below the below the warning threshold reset the send flag
		rec->sent_error_flag = DISK_SPACE_NO_MESSAGE_SENT;
	} else if ( ( opt.warning_disk_space_percentage == 0 ) && ( used_disk_space_percent < opt.error_disk_space_percentage )) {
		//if there is no warning percent to check and the used disk has fallen below the below the error threshold reset the send flag
		rec->sent_error_flag = DISK_SPACE_NO_MESSAGE_SENT;
	}
	return 0;
}

static bool is_query_not_active(apr_int32_t tmid, apr_int32_t ssid, apr_int32_t ccnt, apr_hash_t *hash, apr_pool_t *pool)
{
	// get active query of session
	char *key = apr_psprintf(pool, "%d", ssid);
	char *active_query = apr_hash_get(hash, key, APR_HASH_KEY_STRING);
	if (active_query == NULL)
	{
		TR0(("Found orphan query, tmid:%d, ssid:%d, ccnt:%d\n", tmid, ssid, ccnt));
		return true;
	}

	// read query text from q file
	char *query = get_query_text(tmid, ssid, ccnt, pool);
	if (query == NULL)
	{
		TR0(("Found error while reading query text in file '%sq%d-%d-%d.txt'\n", GPMON_DIR, tmid, ssid, ccnt));
		return true;
	}
	// if the current active query of session (ssid) is not the same
	// as the one we are checking, we assume q(tmid)-(ssid)-(ccnt).txt
	// has wrong status. This is a bug in execMain.c, which too hard to
	// fix it there.
	int qlen = strlen(active_query);
	if (qlen > MAX_QUERY_COMPARE_LENGTH)
	{
		qlen = MAX_QUERY_COMPARE_LENGTH;
	}
	int res = strncmp(query, active_query, qlen);
	if (res != 0)
	{
		TR0(("Found orphan query, tmid:%d, ssid:%d, ccnt:%d\n", tmid, ssid, ccnt));
		return true;
	}

	return false;
}

static apr_status_t agg_put_fsinfo(agg_t* agg, const gpmon_fsinfo_t* met)
{
	mmon_fsinfo_t* rec;

	rec = apr_hash_get(agg->fsinfotab, &met->key, sizeof(met->key));
	if (!rec) {
		// Use the parent pool because we need the fsinfo to be persistent and never be freed
		rec = apr_palloc(agg->parent_pool, sizeof(*rec));
		if (!rec)
			return APR_ENOMEM;
		rec->key = met->key;
		rec->sent_error_flag = DISK_SPACE_NO_MESSAGE_SENT;
		apr_hash_set(agg->fsinfotab, &met->key, sizeof(met->key), rec);
	}
	rec->bytes_available = met->bytes_available;
	rec->bytes_total = met->bytes_total;
	rec->bytes_used = met->bytes_used;
	rec->last_update_timestamp = time(NULL); //set the updated timestamp for the packet

	// if both the option percentages are set to 0 than the disk space check is disabled
	// Also if max_disk_space_messages_per_interval is 0 the disk space check is disabled
	if (((opt.warning_disk_space_percentage) || (opt.error_disk_space_percentage)) &&
			(opt.max_disk_space_messages_per_interval != 0)) {
		check_disk_space(rec);
	}
	return 0;
}

static apr_status_t agg_put_queryseg(agg_t* agg, const gpmon_query_seginfo_t* met, apr_int64_t generation)
{
	qdnode_t* dp;
	gpmon_qlogkey_t key;
	mmon_query_seginfo_t* rec = 0;

	/* find qdnode of this qexec */
	key.tmid = met->key.qkey.tmid;
	key.ssid = met->key.qkey.ssid;
	key.ccnt = met->key.qkey.ccnt;
	dp = apr_hash_get(agg->qtab, &key, sizeof(key));

	if (!dp) { /* not found, internal SPI query.  Ignore. */
		return 0;
	}
	rec = apr_hash_get(dp->query_seginfo_hash, &met->key.segid, sizeof(met->key.segid));

	/* if found, replace it */
	if (rec) {
		rec->final_rowsout = met->final_rowsout;
		rec->sum_cpu_elapsed += met->sum_cpu_elapsed;
		rec->sum_measures_rows_out += met->sum_measures_rows_out;
	}
	else {
		/* not found, make new hash entry */

		if (!(rec = apr_palloc(agg->pool, sizeof(mmon_query_seginfo_t)))){

			return APR_ENOMEM;
		}
		memcpy(&rec->key, &met->key, sizeof(gpmon_query_seginfo_key_t));
		rec->final_rowsout = met->final_rowsout;
		rec->sum_cpu_elapsed = met->sum_cpu_elapsed;
		rec->sum_measures_rows_out = met->sum_measures_rows_out;

		apr_hash_set(dp->query_seginfo_hash, &rec->key.segid, sizeof(rec->key.segid), rec);
	}

	dp->last_updated_generation = generation;
	return 0;
}

static apr_status_t agg_put_metrics(agg_t* agg, const gpmon_metrics_t* met)
{
	gpmon_metrics_t* rec;

	rec = apr_hash_get(agg->htab, met->hname, APR_HASH_KEY_STRING);
	if (rec) {
		*rec = *met;
	} else {
		rec = apr_palloc(agg->pool, sizeof(*rec));
		if (!rec)
			return APR_ENOMEM;
		*rec = *met;
		apr_hash_set(agg->htab, rec->hname, APR_HASH_KEY_STRING, rec);
	}
	return 0;
}

static apr_status_t agg_put_segment(agg_t* agg, const gpmon_seginfo_t* seg)
{
	gpmon_seginfo_t* rec;

	rec = apr_hash_get(agg->stab, &seg->dbid, sizeof(seg->dbid));
	if (rec)
	{
		*rec = *seg;
	}
	else
	{
		rec = apr_palloc(agg->pool, sizeof(*rec));
		if (!rec)
		{
			return APR_ENOMEM;
		}
		*rec = *seg;
		apr_hash_set(agg->stab, &rec->dbid, sizeof(rec->dbid), rec);
	}
	return 0;
}

static apr_status_t agg_put_query_metrics(agg_t* agg, const gpmon_qlog_t* qlog, apr_int64_t generation)
{
	qdnode_t* node;

	node = apr_hash_get(agg->qtab, &qlog->key, sizeof(qlog->key));
	if (!node) {
		gpmon_qlogkey_t new_key = qlog->key;
		new_key.ccnt = 0;
		node = apr_hash_get(agg->qtab, &new_key, sizeof(new_key));
	}
	if (node)
	{
		// here update the stats for the query
		node->qlog.cpu_elapsed += qlog->cpu_elapsed;
		node->qlog.p_metrics.cpu_pct += qlog->p_metrics.cpu_pct;
		node->last_updated_generation = generation;
		node->num_metrics_packets++;
		TR2(("Query Metrics: (host %s ssid %d ccnt %d) (cpuelapsed %d cpupct %f) / %d\n",
			qlog->user, qlog->key.ssid, qlog->key.ccnt, node->qlog.cpu_elapsed, node->qlog.p_metrics.cpu_pct,
			node->num_metrics_packets));
	}
	return 0;
}

static apr_status_t agg_put_qlog(agg_t* agg, const gpmon_qlog_t* qlog,
				 apr_int64_t generation)
{
	qdnode_t* node;

	node = apr_hash_get(agg->qtab, &qlog->key, sizeof(qlog->key));
	if (node) {
		node->qlog = *qlog;
		if (0 != strcmp(qlog->db, GPMON_DB)) {
			TR2(("agg_put_qlog: found %d.%d.%d generation %d recorded %d\n", qlog->key.tmid, qlog->key.ssid, qlog->key.ccnt, generation, node->recorded));
		}
	} else {
		node = apr_pcalloc(agg->pool, sizeof(*node));
		if (!node)
			return APR_ENOMEM;

		node->qlog = *qlog;
		node->recorded = 0;
		node->qlog.cpu_elapsed = 0;
		node->qlog.p_metrics.cpu_pct = 0.0;
		node->num_metrics_packets = 0;

		node->qexec_hash = apr_hash_make(agg->pool);
		if (!node->qexec_hash) {
			TR2(("agg_put_qlog: qexec_hash = apr_hash_make(agg->pool) returned null\n"));
			return APR_ENOMEM;
		}

		node->query_seginfo_hash = apr_hash_make(agg->pool);
		if (!node->query_seginfo_hash) {
			TR2(("agg_put_qlog: query_seginfo_hash = apr_hash_make(agg->pool) returned null\n"));
			return APR_ENOMEM;
		}

		apr_hash_set(agg->qtab, &node->qlog.key, sizeof(node->qlog.key), node);
		if (0 != strcmp(qlog->db, GPMON_DB)) {
			TR2(("agg_put: new %d.%d.%d generation %d recorded %d\n", qlog->key.tmid, qlog->key.ssid, qlog->key.ccnt, generation, node->recorded));
		}
	}
	node->last_updated_generation = generation;

	return 0;
}


static apr_status_t agg_put_qexec(agg_t* agg, const qexec_packet_t* qexec_packet, apr_int64_t generation)
{
	qdnode_t* dp;
	gpmon_qlogkey_t key;
	mmon_qexec_t* mmon_qexec_existing = 0;

	/* find qdnode of this qexec */
	key.tmid = qexec_packet->data.key.tmid;
	key.ssid = qexec_packet->data.key.ssid;
	key.ccnt = qexec_packet->data.key.ccnt;
	dp = apr_hash_get(agg->qtab, &key, sizeof(key));

	if (!dp) { /* not found, internal SPI query.  Ignore. */
		return 0;
	}

	mmon_qexec_existing = apr_hash_get(dp->qexec_hash, &qexec_packet->data.key.hash_key, sizeof(qexec_packet->data.key.hash_key));

	/* if found, replace it */
	if (mmon_qexec_existing) {
		mmon_qexec_existing->key.ccnt = qexec_packet->data.key.ccnt;
		mmon_qexec_existing->key.ssid = qexec_packet->data.key.ssid;
		mmon_qexec_existing->key.tmid = qexec_packet->data.key.tmid;
		mmon_qexec_existing->_cpu_elapsed = qexec_packet->data._cpu_elapsed;
		mmon_qexec_existing->measures_rows_in = qexec_packet->data.measures_rows_in;
		mmon_qexec_existing->rowsout = qexec_packet->data.rowsout;
	}
	else {
		/* not found, make new hash entry */
		if (! (mmon_qexec_existing = apr_palloc(agg->pool, sizeof(mmon_qexec_t))))
			return APR_ENOMEM;		

		memcpy(&mmon_qexec_existing->key, &qexec_packet->data.key, sizeof(gpmon_qexeckey_t));
		mmon_qexec_existing->_cpu_elapsed = qexec_packet->data._cpu_elapsed;
		mmon_qexec_existing->measures_rows_in = qexec_packet->data.measures_rows_in;
		mmon_qexec_existing->rowsout = qexec_packet->data.rowsout;
		apr_hash_set(dp->qexec_hash, &mmon_qexec_existing->key.hash_key, sizeof(mmon_qexec_existing->key.hash_key), mmon_qexec_existing);
	}

	dp->last_updated_generation = generation;
	return 0;
}


apr_status_t agg_create(agg_t** retagg, apr_int64_t generation, apr_pool_t* parent_pool, apr_hash_t* fsinfotab)
{
	int e;
	apr_pool_t* pool;
	agg_t* agg;

	if (0 != (e = apr_pool_create_alloc(&pool, parent_pool)))
		return e;

	agg = apr_pcalloc(pool, sizeof(*agg));
	if (!agg) {
		apr_pool_destroy(pool);
		return APR_ENOMEM;
	}

	agg->generation = generation;
	agg->pool = pool;
	agg->parent_pool = parent_pool;
	agg->fsinfotab = fsinfotab; // This hash table for the fsinfo is persistent and will use the parent pool

	agg->qtab = apr_hash_make(pool);
	if (!agg->qtab) {
		apr_pool_destroy(pool);
		return APR_ENOMEM;
	}

	agg->htab = apr_hash_make(pool);
	if (!agg->htab) {
		apr_pool_destroy(pool);
		return APR_ENOMEM;
	}

	agg->stab = apr_hash_make(pool);
	if (!agg->stab) {
		apr_pool_destroy(pool);
		return APR_ENOMEM;
	}

	*retagg = agg;
	return 0;
}



apr_status_t agg_dup(agg_t** retagg, agg_t* oldagg, apr_pool_t* parent_pool, apr_hash_t* fsinfotab)
{
	int e, cnt;
	agg_t* newagg;
	apr_hash_index_t *hi, *hj;

	if (0 != (e = agg_create(&newagg, oldagg->generation + 1, parent_pool, fsinfotab)))
	{
		return e;
	}

	apr_hash_t *active_query_tab = get_active_queries(newagg->pool);
	if (! active_query_tab)
	{
		agg_destroy(newagg);
		return APR_EINVAL;
	}

	for (hi = apr_hash_first(0, oldagg->qtab); hi; hi = apr_hash_next(hi))
	{
		void* vptr;
		qdnode_t* dp;
		qdnode_t* newdp;
		apr_int32_t status;

		apr_hash_this(hi, 0, 0, &vptr);
		dp = vptr;

		/* skip all entries that weren't updated recently and aren't waiting in a queue */
		/* Read status from query text as this is reliable */
		status = get_query_status(dp->qlog.key.tmid, dp->qlog.key.ssid, dp->qlog.key.ccnt);

		apr_int32_t age = newagg->generation - dp->last_updated_generation - 1;
		if (age > 0)
		{
			if (  (status != GPMON_QLOG_STATUS_SUBMIT
			       && status != GPMON_QLOG_STATUS_CANCELING
			       && status != GPMON_QLOG_STATUS_START)
			   || ((age % 5 == 0) /* don't call is_query_not_active every time because it's expensive */
			       && is_query_not_active(dp->qlog.key.tmid, dp->qlog.key.ssid, dp->qlog.key.ccnt, active_query_tab, newagg->pool)))
			{
				if (0 != strcmp(dp->qlog.db, GPMON_DB))
				{
					TR2(("agg_dup: skip %d.%d.%d generation %d, current generation %d, recorded %d\n",
						dp->qlog.key.tmid, dp->qlog.key.ssid, dp->qlog.key.ccnt,
						(int) dp->last_updated_generation, (int) newagg->generation, dp->recorded));
				}
				continue;
			}
		}

		/* check if we missed a status change */
		if (dp->qlog.status != status)
			dp->qlog.status = status;

		if (0 != strcmp(dp->qlog.db, GPMON_DB)) {
			TR2( ("agg_dup: add %d.%d.%d, generation %d, recorded %d:\n", dp->qlog.key.tmid, dp->qlog.key.ssid, dp->qlog.key.ccnt, dp->last_updated_generation, dp->recorded));
		}

		/* dup this entry */
		if (!(newdp = apr_palloc(newagg->pool, sizeof(*newdp)))) {
			agg_destroy(newagg);
			return APR_ENOMEM;
		}

		*newdp = *dp;

		newdp->qexec_hash = apr_hash_make(newagg->pool);
		if (!newdp->qexec_hash) {
			agg_destroy(newagg);
			return APR_ENOMEM;
		}

		cnt = 0;
		// Copy the qexec hash table
		for (hj = apr_hash_first(newagg->pool, dp->qexec_hash); hj; hj = apr_hash_next(hj)) {
			mmon_qexec_t* new_qexec;
			apr_hash_this(hj, 0, 0, &vptr);

			//allocate the packet
			if (!(new_qexec = apr_pcalloc(newagg->pool, sizeof(mmon_qexec_t)))) {
				agg_destroy(newagg);
				return APR_ENOMEM;
			}
			*new_qexec = *((mmon_qexec_t*)vptr);

			apr_hash_set(newdp->qexec_hash, &(new_qexec->key.hash_key), sizeof(new_qexec->key.hash_key), new_qexec);
			TR2( ("\t    %d: (%d, %d)\n", ++cnt, new_qexec->key.hash_key.segid, new_qexec->key.hash_key.nid));
		}

		newdp->query_seginfo_hash = apr_hash_make(newagg->pool);
		if (!newdp->query_seginfo_hash) {
			agg_destroy(newagg);
			return APR_ENOMEM;
		}

		cnt = 0;
		// Copy the query_seginfo hash table
		for (hj = apr_hash_first(newagg->pool, dp->query_seginfo_hash); hj; hj = apr_hash_next(hj)) {
			mmon_query_seginfo_t* new_query_seginfo;
			apr_hash_this(hj, 0, 0, &vptr);

			if (!(new_query_seginfo = apr_pcalloc(newagg->pool, sizeof(mmon_query_seginfo_t)))) {
				agg_destroy(newagg);
				return APR_ENOMEM;
			}
			*new_query_seginfo = *((mmon_query_seginfo_t*)vptr);

			apr_hash_set(newdp->query_seginfo_hash, &(new_query_seginfo->key.segid), sizeof(new_query_seginfo->key.segid), new_query_seginfo);
			TR2( ("\t    %d: (%d)\n", ++cnt, new_query_seginfo->key.segid));
		}

		// reset metrics that are accumulated each quantum
		newdp->qlog.cpu_elapsed = 0;
		newdp->qlog.p_metrics.cpu_pct = 0.0;
		newdp->num_metrics_packets = 0;

		apr_hash_set(newagg->qtab, &newdp->qlog.key, sizeof(newdp->qlog.key), newdp);
	}

	*retagg = newagg;
	return 0;
}

void agg_destroy(agg_t* agg)
{
	apr_pool_destroy(agg->pool);
}

apr_status_t agg_put(agg_t* agg, const gp_smon_to_mmon_packet_t* pkt)
{
	if (pkt->header.pkttype == GPMON_PKTTYPE_METRICS)
		return agg_put_metrics(agg, &pkt->u.metrics);
	if (pkt->header.pkttype == GPMON_PKTTYPE_QLOG)
		return agg_put_qlog(agg, &pkt->u.qlog, agg->generation);
	if (pkt->header.pkttype == GPMON_PKTTYPE_QEXEC)
		return agg_put_qexec(agg, &pkt->u.qexec_packet, agg->generation);
	if (pkt->header.pkttype == GPMON_PKTTYPE_SEGINFO)
		return agg_put_segment(agg, &pkt->u.seginfo);
	if (pkt->header.pkttype == GPMON_PKTTYPE_QUERY_HOST_METRICS)
		return agg_put_query_metrics(agg, &pkt->u.qlog, agg->generation);
	if (pkt->header.pkttype == GPMON_PKTTYPE_FSINFO)
		return agg_put_fsinfo(agg, &pkt->u.fsinfo);
	if (pkt->header.pkttype == GPMON_PKTTYPE_QUERYSEG)
		return agg_put_queryseg(agg, &pkt->u.queryseg, agg->generation);

	gpmon_warning(FLINE, "unknown packet type %d", pkt->header.pkttype);
	return 0;
}


typedef struct bloom_t bloom_t;
struct bloom_t {
	unsigned char map[1024];
};
static void bloom_init(bloom_t* bloom);
static void bloom_set(bloom_t* bloom, const char* name);
static int  bloom_isset(bloom_t* bloom, const char* name);
static void delete_old_files(bloom_t* bloom);
static apr_uint32_t write_fsinfo(agg_t* agg, const char* nowstr);
static apr_uint32_t write_system(agg_t* agg, const char* nowstr);
static apr_uint32_t write_segmentinfo(agg_t* agg, char* nowstr);
static apr_uint32_t write_dbmetrics(dbmetrics_t* dbmetrics, char* nowstr);
static apr_uint32_t write_qlog(FILE* fp, qdnode_t *qdnode, const char* nowstr, apr_uint32_t done);
static apr_uint32_t write_qlog_full(FILE* fp, qdnode_t *qdnode, const char* nowstr);

apr_status_t agg_dump(agg_t* agg)
{
	apr_hash_index_t *hi;
	bloom_t bloom;
	char nowstr[GPMON_DATE_BUF_SIZE];
	FILE* fp_queries_now = 0;
	FILE* fp_queries_tail = 0;

	dbmetrics_t dbmetrics = {0};

	apr_uint32_t temp_bytes_written = 0;

	gpmon_datetime_rounded(time(NULL), nowstr);

	bloom_init(&bloom);

	/* we never delete system_tail/ system_now/
		queries_tail/ queries_now/ files */
	bloom_set(&bloom, GPMON_DIR "system_now.dat");
	bloom_set(&bloom, GPMON_DIR "system_tail.dat");
	bloom_set(&bloom, GPMON_DIR "system_stage.dat");
	bloom_set(&bloom, GPMON_DIR "_system_tail.dat");
	bloom_set(&bloom, GPMON_DIR "queries_now.dat");
	bloom_set(&bloom, GPMON_DIR "queries_tail.dat");
	bloom_set(&bloom, GPMON_DIR "queries_stage.dat");
	bloom_set(&bloom, GPMON_DIR "_queries_tail.dat");
	bloom_set(&bloom, GPMON_DIR "database_now.dat");
	bloom_set(&bloom, GPMON_DIR "database_tail.dat");
	bloom_set(&bloom, GPMON_DIR "database_stage.dat");
	bloom_set(&bloom, GPMON_DIR "_database_tail.dat");
	bloom_set(&bloom, GPMON_DIR "segment_now.dat");
	bloom_set(&bloom, GPMON_DIR "segment_tail.dat");
	bloom_set(&bloom, GPMON_DIR "segment_stage.dat");
	bloom_set(&bloom, GPMON_DIR "_segment_tail.dat");
	bloom_set(&bloom, GPMON_DIR "diskspace_now.dat");
	bloom_set(&bloom, GPMON_DIR "diskspace_tail.dat");
	bloom_set(&bloom, GPMON_DIR "diskspace_stage.dat");
	bloom_set(&bloom, GPMON_DIR "_diskspace_tail.dat");


	/* dump metrics */
	temp_bytes_written = write_system(agg, nowstr);
	incremement_tail_bytes(temp_bytes_written);

	/* write segment metrics */
	temp_bytes_written = write_segmentinfo(agg, nowstr);
	incremement_tail_bytes(temp_bytes_written);

	/* write fsinfo metrics */
	temp_bytes_written = write_fsinfo(agg, nowstr);
	incremement_tail_bytes(temp_bytes_written);

	if (! (fp_queries_tail = fopen(GPMON_DIR "queries_tail.dat", "a")))
		return APR_FROM_OS_ERROR(errno);

	/* loop through queries */
	for (hi = apr_hash_first(0, agg->qtab); hi; hi = apr_hash_next(hi))
	{
		void* vptr;
		qdnode_t* qdnode;
		apr_hash_this(hi, 0, 0, &vptr);
		qdnode = vptr;

		if (qdnode->qlog.status == GPMON_QLOG_STATUS_DONE || qdnode->qlog.status == GPMON_QLOG_STATUS_ERROR)
		{
			if (!qdnode->recorded && ((qdnode->qlog.tfin - qdnode->qlog.tstart) >= min_query_time))
			{
				TR1(("queries_tail: %x add query %d.%d.%d, status %d, generation %d, recorded %d\n",
						agg->qtab, qdnode->qlog.key.tmid, qdnode->qlog.key.ssid, qdnode->qlog.key.ccnt, qdnode->qlog.status, qdnode->last_updated_generation, qdnode->recorded));

				temp_bytes_written += write_qlog_full(fp_queries_tail, qdnode, nowstr);
				incremement_tail_bytes(temp_bytes_written);

				qdnode->recorded = 1;
			}
		}
		else
		{
			switch (qdnode->qlog.status)
			{
			case GPMON_QLOG_STATUS_START:
			case GPMON_QLOG_STATUS_CANCELING:
				dbmetrics.queries_running++;
				break;
			case GPMON_QLOG_STATUS_SUBMIT:
				dbmetrics.queries_queued++;
				break;
			default:
				/* Not interested */
				break;
			}
		}
	}
	dbmetrics.queries_total = dbmetrics.queries_running + dbmetrics.queries_queued;

	fclose(fp_queries_tail);
	fp_queries_tail = 0;

	/* dump dbmetrics */
	temp_bytes_written += write_dbmetrics(&dbmetrics, nowstr);
	incremement_tail_bytes(temp_bytes_written);

	if (! (fp_queries_now = fopen(GPMON_DIR "_queries_now.dat", "w")))
		return APR_FROM_OS_ERROR(errno);

	for (hi = apr_hash_first(0, agg->qtab); hi; hi = apr_hash_next(hi))
	{
		void* vptr;
		qdnode_t* qdnode;

		apr_hash_this(hi, 0, 0, &vptr);
		qdnode = vptr;

		/* don't touch this file */
		{
			const int fname_size = sizeof(GPMON_DIR) + 100;
			char fname[fname_size];
			snprintf(fname, fname_size, GPMON_DIR "q%d-%d-%d.txt",
			qdnode->qlog.key.tmid, qdnode->qlog.key.ssid,
			qdnode->qlog.key.ccnt);

			bloom_set(&bloom, fname);
		}

		/* write to _query_now.dat */
		if (qdnode->qlog.status != GPMON_QLOG_STATUS_DONE && qdnode->qlog.status != GPMON_QLOG_STATUS_ERROR)
		{
			write_qlog(fp_queries_now, qdnode, nowstr, 0);
		}
		else if (qdnode->qlog.tfin - qdnode->qlog.tstart >= min_query_time)
		{
			write_qlog(fp_queries_now, qdnode, nowstr, 1);
		}

	}

	if (fp_queries_now) fclose(fp_queries_now);
	if (fp_queries_tail) fclose(fp_queries_tail);
	rename(GPMON_DIR "_system_now.dat", GPMON_DIR "system_now.dat");
	rename(GPMON_DIR "_segment_now.dat", GPMON_DIR "segment_now.dat");
	rename(GPMON_DIR "_queries_now.dat", GPMON_DIR "queries_now.dat");
	rename(GPMON_DIR "_database_now.dat", GPMON_DIR "database_now.dat");
	rename(GPMON_DIR "_diskspace_now.dat", GPMON_DIR "diskspace_now.dat");

	/* clean up ... delete all old files by checking our bloom filter */
	delete_old_files(&bloom);

	return 0;
}

extern int gpmmon_quantum(void);

static void delete_old_files(bloom_t* bloom)
{
	char findDir[256] = {0};
	char findCmd[512] = {0};
	FILE* fp = NULL;
	time_t cutoff = time(0) - gpmmon_quantum() * 3;

	/* Need to remove trailing / in dir so find results are consistent
     * between platforms
     */
	strncpy(findDir, GPMON_DIR, 255);
	if (findDir[strlen(findDir) -1] == '/')
		findDir[strlen(findDir) - 1] = '\0';

	snprintf(findCmd, 512, "find %s -name \"q*-*.txt\" 2> /dev/null", findDir);
	fp = popen(findCmd, "r");

	if (fp)
	{
		for (;;)
		{
			char line[1024];
			char* p;
			struct stat stbuf;
			apr_int32_t status;

			line[sizeof(line) - 1] = 0;
			if (! (p = fgets(line, sizeof(line), fp)))
				break;
			if (line[sizeof(line) - 1])
				continue; 	/* fname too long */

			p = gpmon_trim(p);
			TR2(("Checking file %s\n", p));

			if (0 == stat(p, &stbuf))
			{
#if defined(linux)
				int expired = stbuf.st_mtime < cutoff;
#else
				int expired = stbuf.st_mtimespec.tv_sec < cutoff;
#endif
				TR2(("File %s expired: %d\n", p, expired));
				if (expired)
				{
					apr_int32_t tmid = 0, ssid = 0, ccnt = 0;
					if (bloom_isset(bloom, p))
					{
						TR2(("File %s has bloom set.  Checking status\n", p));
						/* Verify no bloom collision */
						sscanf(p, GPMON_DIR "q%d-%d-%d.txt", &tmid, &ssid, &ccnt);
						TR2(("tmid: %d, ssid: %d, ccnt: %d\n", tmid, ssid, ccnt));
						status = get_query_status(tmid, ssid, ccnt);
						TR2(("File %s has status of %d\n", p, status));
						if (status == GPMON_QLOG_STATUS_DONE ||
						   status == GPMON_QLOG_STATUS_ERROR)
						{
							TR2(("Deleting file %s\n", p));
							unlink(p);
						}
					}
					else
					{
						TR2(("Deleting file %s\n", p));
						unlink(p);
					}
				}
			}
		}
		pclose(fp);
	}
	else
	{
		gpmon_warning(FLINE, "Failed to get a list of query text files.\n");
	}
}

static apr_uint32_t write_segmentinfo(agg_t* agg, char* nowstr)
{
	FILE* fp = fopen(GPMON_DIR "segment_tail.dat", "a");
	FILE* fp2 = fopen(GPMON_DIR "_segment_now.dat", "w");
	apr_hash_index_t* hi;
	const int line_size = 256;
	char line[line_size];
	apr_uint32_t bytes_written = 0;

	if (!fp || !fp2)
	{
		if (fp) fclose(fp);
		if (fp2) fclose(fp2);
		return 0;
	}

	for (hi = apr_hash_first(0, agg->stab); hi; hi = apr_hash_next(hi))
	{
		gpmon_seginfo_t* sp;
		int bytes_this_record;
		void* valptr = 0;
		apr_hash_this(hi, 0, 0, (void**) &valptr);
		sp = (gpmon_seginfo_t*) valptr;

		snprintf(line, line_size, "%s|%d|%s|%" FMTU64 "|%" FMTU64, nowstr, sp->dbid, sp->hostname, sp->dynamic_memory_used, sp->dynamic_memory_available);

		bytes_this_record = strlen(line) + 1;
		if (bytes_this_record == line_size)
		{
			gpmon_warning(FLINE, "segmentinfo line to too long ... ignored: %s", line);
			continue;
		}
		fprintf(fp, "%s\n", line);
		fprintf(fp2, "%s\n", line);
		bytes_written += bytes_this_record;
    }

	fclose(fp);
	fclose(fp2);
	return bytes_written;
}

static apr_uint32_t write_fsinfo(agg_t* agg, const char* nowstr)
{
	FILE* fp = fopen(GPMON_DIR "diskspace_tail.dat", "a");
	FILE* fp2 = fopen(GPMON_DIR "_diskspace_now.dat", "w");
	apr_hash_index_t* hi;
	const int line_size = 512;
	char line[line_size];
	apr_uint32_t bytes_written = 0;
	static time_t last_time_fsinfo_written = 0;

	if (!fp || !fp2)
	{
		if (fp) fclose(fp);
		if (fp2) fclose(fp2);
		return 0;
	}

	for (hi = apr_hash_first(0, agg->fsinfotab); hi; hi = apr_hash_next(hi))
	{
		mmon_fsinfo_t* fsp;
		void* valptr = 0;
		int bytes_this_line;

		apr_hash_this(hi, 0, 0, (void**) &valptr);
		fsp = (mmon_fsinfo_t*) valptr;

		// We only want to write the fsinfo for packets that have been updated since the last time we wrote
		// the fsinfo, so skip the fsinfo if its timestamp is less than the last time written timestamp
		if (fsp->last_update_timestamp < last_time_fsinfo_written) {
			continue;
		}

		snprintf(line, line_size, "%s|%s|%s|%" FMT64 "|%" FMT64 "|%" FMT64,
				nowstr,
				fsp->key.hostname,
				fsp->key.fsname,
				fsp->bytes_total,
				fsp->bytes_used,
				fsp->bytes_available);

		TR2(("write_fsinfo(): writing %s\n", line));
		bytes_this_line = strlen(line) + 1;
		if (bytes_this_line == line_size){
			gpmon_warning(FLINE, "fsinfo metrics line too long ... ignored: %s", line);
			continue;
		}

		fprintf(fp, "%s\n", line);
		fprintf(fp2, "%s\n", line);

		bytes_written += bytes_this_line;
	}

	fclose(fp);
	fclose(fp2);

	last_time_fsinfo_written = time(NULL); //set the static time written variable

	return bytes_written;
}

static apr_uint32_t write_dbmetrics(dbmetrics_t* dbmetrics, char* nowstr)
{
	FILE* fp = fopen(GPMON_DIR "database_tail.dat", "a");
	FILE* fp2 = fopen(GPMON_DIR "_database_now.dat", "w");
	int e;
	const int line_size = 256;
	char line[line_size];
	int bytes_written;

	if (!fp || !fp2)
	{
		e = APR_FROM_OS_ERROR(errno);
		if (fp) fclose(fp);
		if (fp2) fclose(fp2);
		return e;
	}

	snprintf(line, line_size, "%s|%d|%d|%d", nowstr,
             dbmetrics->queries_total,
             dbmetrics->queries_running,
             dbmetrics->queries_queued);

	if (strlen(line) + 1 == line_size){
		gpmon_warning(FLINE, "dbmetrics line too long ... ignored: %s", line);
		bytes_written = 0;
	} else {
		fprintf(fp, "%s\n", line);
		fprintf(fp2, "%s\n", line);
		bytes_written = strlen(line) + 1;
	}

    fclose(fp);
    fclose(fp2);

    return bytes_written;
}

static apr_uint32_t write_system(agg_t* agg, const char* nowstr)
{
	FILE* fp = fopen(GPMON_DIR "system_tail.dat", "a");
	FILE* fp2 = fopen(GPMON_DIR "_system_now.dat", "w");
	apr_hash_index_t* hi;
	const int line_size = 1000;
	char line[line_size];
	apr_uint32_t bytes_written = 0;

	if (!fp || !fp2)
	{
		if (fp) fclose(fp);
		if (fp2) fclose(fp2);
		return 0;
 	}

	for (hi = apr_hash_first(0, agg->htab); hi; hi = apr_hash_next(hi))
	{
		gpmon_metrics_t* mp;
		void* valptr = 0;
		int quantum = gpmmon_quantum();
		int bytes_this_line;
		apr_hash_this(hi, 0, 0, (void**) &valptr);
		mp = (gpmon_metrics_t*) valptr;

		snprintf(line, line_size,
		"%s|%s|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%.2f|%.2f|%.2f|%.4f|%.4f|%.4f|%d|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64,
		nowstr,
		mp->hname,
		mp->mem.total,
		mp->mem.used,
		mp->mem.actual_used,
		mp->mem.actual_free,
		mp->swap.total,
		mp->swap.used,
		(apr_int64_t)ceil((double)mp->swap.page_in / (double)quantum),
		(apr_int64_t)ceil((double)mp->swap.page_out / (double)quantum),
		mp->cpu.user_pct,
		mp->cpu.sys_pct,
		mp->cpu.idle_pct,
		mp->load_avg.value[0],
		mp->load_avg.value[1],
		mp->load_avg.value[2],
		quantum,
		mp->disk.ro_rate,
		mp->disk.wo_rate,
		mp->disk.rb_rate,
		mp->disk.wb_rate,
		mp->net.rp_rate,
		mp->net.wp_rate,
		mp->net.rb_rate,
		mp->net.wb_rate);

		bytes_this_line = strlen(line) + 1;
		if (bytes_this_line == line_size){
			gpmon_warning(FLINE, "system metrics line too long ... ignored: %s", line);
			continue;
		}

		fprintf(fp, "%s\n", line);
		fprintf(fp2, "%s\n", line);

		bytes_written += bytes_this_line;
	}

	fclose(fp);
	fclose(fp2);
	return bytes_written;
}

static apr_int64_t get_rowsout(qdnode_t* qdnode)
{

	apr_hash_index_t *hi;
	//qenode_t* pqe = NULL;
	apr_int64_t rowsout = 0;
	void* valptr;
	mmon_query_seginfo_t *query_seginfo;

	for (hi = apr_hash_first(NULL, qdnode->query_seginfo_hash); hi; hi = apr_hash_next(hi))
	{
		apr_hash_this(hi, 0, 0, &valptr);
		query_seginfo = (mmon_query_seginfo_t*) valptr;
		if (query_seginfo->final_rowsout != -1)
		{
			rowsout = query_seginfo->final_rowsout;
			break;
		}
	}
	return rowsout;
}


static void _get_sum_seg_info(apr_hash_t* segtab, apr_int64_t* total_data_out, int* segcount_out)
{
	apr_hash_index_t *hi;
	void* valptr;
	apr_int64_t* seg_data_sum = NULL;

	for (hi = apr_hash_first(NULL, segtab); hi; hi = apr_hash_next(hi))
	{
		apr_hash_this(hi, 0, 0, &valptr);
		seg_data_sum = (apr_int64_t*) valptr;
		*total_data_out += *seg_data_sum;
		TR2(("(SKEW) Segment resource usage: %d\n", *seg_data_sum));
		(*segcount_out)++;
	}
}

static void _get_sum_deviation_squared(apr_hash_t* segtab, const apr_int64_t data_avg, apr_int64_t* total_deviation_squared_out)
{
	apr_hash_index_t *hi;
	void* valptr;
	apr_int64_t* seg_data_sum = NULL;

	for (hi = apr_hash_first(NULL, segtab); hi; hi = apr_hash_next(hi))
	{
		apr_int64_t dev = 0;

		apr_hash_this(hi, NULL, NULL, &valptr);
		seg_data_sum = (apr_int64_t*) valptr;
		dev = *seg_data_sum - data_avg;
		TR2(("(SKEW) Deviation: %d\n", dev));
		*total_deviation_squared_out += dev * dev;
	}
}

static double get_cpu_skew(qdnode_t* qdnode)
{
	apr_pool_t* tmp_pool;
	apr_hash_t* segtab;
	apr_hash_index_t *hi;

	apr_int64_t cpu_avg = 0;
	apr_int64_t total_cpu = 0;
	apr_int64_t total_deviation_squared = 0;
	double variance = 0;
	double standard_deviation = 0;
	double coefficient_of_variation = 0;
	apr_int64_t* seg_cpu_sum = NULL;
	void* valptr;

	int segcnt = 0;
	int e;

	if (!qdnode)
		return 0.0f;

	if (0 != (e = apr_pool_create_alloc(&tmp_pool, 0)))
	{
		gpmon_warningx(FLINE, e, "apr_pool_create_alloc failed");
		return 0.0f;
	}

	segtab = apr_hash_make(tmp_pool);
	if (!segtab)
	{
		gpmon_warning(FLINE, "Out of memory");
		return 0.0f;
	}

	TR2(("Calc mean per segment\n"));

	for (hi = apr_hash_first(NULL, qdnode->query_seginfo_hash); hi; hi = apr_hash_next(hi))
	{
		mmon_query_seginfo_t	*rec;
		apr_hash_this(hi, 0, 0, &valptr);
		rec = (mmon_query_seginfo_t*) valptr;

		if (rec->key.segid == -1)
			continue;

		seg_cpu_sum = apr_hash_get(segtab, &rec->key.segid, sizeof(rec->key.segid));

		if (!seg_cpu_sum) {
			seg_cpu_sum = apr_palloc(tmp_pool, sizeof(apr_int64_t));
			*seg_cpu_sum = 0;
		}
		*seg_cpu_sum += rec->sum_cpu_elapsed;
		apr_hash_set(segtab, &rec->key.segid, sizeof(rec->key.segid), seg_cpu_sum);
	}

	_get_sum_seg_info(segtab, &total_cpu, &segcnt);

	if (!segcnt) {
		TR2(("No segments for CPU skew calculation\n"));
		apr_pool_destroy(tmp_pool);
		return 0.0f;
	}

	cpu_avg = total_cpu / segcnt;
	TR2(("(SKEW) Avg resource usage: %" FMT64 "\n", cpu_avg));

	_get_sum_deviation_squared(segtab, cpu_avg, &total_deviation_squared);

	variance = total_deviation_squared / (double)segcnt;

	standard_deviation = sqrt(variance);

	TR2(("(SKEW) CPU standard deviation: %f\n", standard_deviation));

	coefficient_of_variation = cpu_avg ? standard_deviation/(double)cpu_avg : 0.0f;

	apr_pool_destroy(tmp_pool);
	TR2(("(SKEW) CPU Skew: %f\n", coefficient_of_variation));

	return coefficient_of_variation;
}

static double get_row_skew(qdnode_t* qdnode)
{
	apr_pool_t* tmp_pool;
	apr_hash_t* segtab;
	apr_hash_index_t *hi;

	apr_int64_t total_row_out = 0;
	apr_int64_t total_deviation_squared = 0;
	double variance = 0.0f;
	double standard_deviation = 0;
	double coefficient_of_variation = 0;
	apr_int64_t row_out_avg = 0;
	apr_int64_t* seg_row_out_sum = NULL;
	void* valptr;

	int segcnt = 0;
	int e;

	if (!qdnode)
		return 0.0f;

	if (0 != (e = apr_pool_create_alloc(&tmp_pool, 0)))
	{
		gpmon_warningx(FLINE, e, "apr_pool_create_alloc failed");
		return 0.0f;
	}

	segtab = apr_hash_make(tmp_pool);
	if (!segtab)
	{
		gpmon_warning(FLINE, "Out of memory");
		return 0.0f;
	}

	/* Calc rows in sum per segment */
	TR2(("Calc rows in sum  per segment\n"));
	for (hi = apr_hash_first(NULL, qdnode->query_seginfo_hash); hi; hi = apr_hash_next(hi))
	{
		mmon_query_seginfo_t	*rec;
		apr_hash_this(hi, 0, 0, &valptr);
		rec = (mmon_query_seginfo_t*) valptr;

		if (rec->key.segid == -1)
			continue;

		seg_row_out_sum = apr_hash_get(segtab, &rec->key.segid, sizeof(rec->key.segid));

		if (!seg_row_out_sum) {
			seg_row_out_sum = apr_palloc(tmp_pool, sizeof(apr_int64_t));
			*seg_row_out_sum = 0;
		}
		*seg_row_out_sum += rec->sum_measures_rows_out;
		apr_hash_set(segtab, &rec->key.segid, sizeof(rec->key.segid), seg_row_out_sum);
	}

	_get_sum_seg_info(segtab, &total_row_out, &segcnt);

	if (!segcnt) {
		TR2(("No segments for Rows skew calculation\n"));
		apr_pool_destroy(tmp_pool);
		return 0.0f;
	}

	row_out_avg = total_row_out / segcnt;

	TR2(("(SKEW) Avg rows out: %" FMT64 "\n", row_out_avg));

	_get_sum_deviation_squared(segtab, row_out_avg, &total_deviation_squared);

	variance = total_deviation_squared / (double)segcnt;
	standard_deviation = sqrt(variance);

	TR2(("(SKEW) Rows in standard deviaton: %f\n", standard_deviation));

	coefficient_of_variation = row_out_avg ? standard_deviation/(double)row_out_avg : 0.0f;

	apr_pool_destroy(tmp_pool);
	TR2(("(SKEW) Rows out skew: %f\n", coefficient_of_variation));

	return coefficient_of_variation;
}


static void fmt_qlog(char* line, const int line_size, qdnode_t* qdnode, const char* nowstr, apr_uint32_t done)
{
	char timsubmitted[GPMON_DATE_BUF_SIZE];
	char timstarted[GPMON_DATE_BUF_SIZE];
	char timfinished[GPMON_DATE_BUF_SIZE];
	double cpu_skew = 0.0f;
	double row_skew = 0.0f;
	int query_hash = 0;
	apr_int64_t rowsout = 0;
	float cpu_current;
	cpu_skew = get_cpu_skew(qdnode);
	row_skew = get_row_skew(qdnode);
	rowsout = get_rowsout(qdnode);
	gpmon_datetime((time_t)qdnode->qlog.tsubmit, timsubmitted);

	if (qdnode->qlog.tstart)
	{
		gpmon_datetime((time_t)qdnode->qlog.tstart, timstarted);
	}
	else
	{
		snprintf(timstarted, GPMON_DATE_BUF_SIZE, "null");
	}

	if (done)
	{
		cpu_current = 0.0f;
		gpmon_datetime((time_t)qdnode->qlog.tfin, timfinished);
	}
	else
	{
		if (qdnode->num_metrics_packets)
		{
			// average cpu_pct per reporting machine
			cpu_current = qdnode->qlog.p_metrics.cpu_pct / qdnode->num_metrics_packets;
		}
		else
		{
			cpu_current = 0.0f;
		}
		snprintf(timfinished, GPMON_DATE_BUF_SIZE,  "null");
	}

	snprintf(line, line_size, "%s|%d|%d|%d|%s|%s|%d|%s|%s|%s|%s|%" FMT64 "|%" FMT64 "|%.4f|%.2f|%.2f|%d",
		nowstr,
		qdnode->qlog.key.tmid,
		qdnode->qlog.key.ssid,
		qdnode->qlog.key.ccnt,
		qdnode->qlog.user,
		qdnode->qlog.db,
		qdnode->qlog.cost,
		timsubmitted,
		timstarted,
		timfinished,
		gpmon_qlog_status_string(qdnode->qlog.status),
		rowsout,
		qdnode->qlog.cpu_elapsed,
		cpu_current,
		cpu_skew,
		row_skew,
		query_hash);
}


static apr_uint32_t write_qlog(FILE* fp, qdnode_t *qdnode, const char* nowstr, apr_uint32_t done)
{
	const int line_size = 1024;
	char line[line_size];
	int bytes_written;

	fmt_qlog(line, line_size, qdnode, nowstr, done);
	bytes_written = strlen(line) + 1;

	if (bytes_written == line_size)
	{
		gpmon_warning(FLINE, "qlog line too long ... ignored: %s", line);
		return 0;
	}
	else
	{
		/* Query text "joined" by python script */
		fprintf(fp, "%s|||||\n", line);
		return bytes_written;
	}
}

static int get_and_print_next_query_file_kvp(FILE* outfd, FILE* queryfd, char* qfname, apr_uint32_t* bytes_written)
{
    const int line_size = 1024;
    char line[line_size];
    line[0] = 0;
    char *p = NULL;
    int field_len = 0;
    int retCode = 0;

    p = fgets(line, line_size, queryfd);
    line[line_size-1] = 0; // in case libc is buggy

    if (!p) {
	    gpmon_warning(FLINE, "Error parsing file: %s", qfname);
        return APR_NOTFOUND;
    }

    retCode = sscanf(p, "%d", &field_len);

    if (1 != retCode){
	    gpmon_warning(FLINE, "bad format on file: %s", qfname);
        return APR_NOTFOUND;
    }

    if (field_len < 0) {
	    gpmon_warning(FLINE, "bad field length on file: %s", qfname);
        return APR_NOTFOUND;
	}

    if (!field_len) {
        // empty field, read through the newline
        p = fgets(line, line_size, queryfd);
        if (p)
            return APR_SUCCESS;
        else
            return APR_NOTFOUND;
    }

    fprintf(outfd, "\"");
    (*bytes_written)++;

    while (field_len > 0) {
	    int max, n;
        char* q;
        max = field_len > sizeof(line) ? sizeof(line) : field_len;
        n = fread(line, 1, max, queryfd);
        for (p = line, q = line + n; p < q; p++)
        {
            if (*p == '"')
            {
                fputc('\"', outfd);
                (*bytes_written)++;
            }

            fputc(*p, outfd);

            (*bytes_written)++;

        }
        field_len -= n;
        if (n < max) break;
    }

	fprintf(outfd, "\"");
	(*bytes_written)++;

    int n = fread(line, 1, 1, queryfd);
    if (n != 1)
    {
	    gpmon_warning(FLINE, "missing expected newline in file: %s", qfname);
        return APR_NOTFOUND;
    }

    return APR_SUCCESS;
}

static apr_uint32_t write_qlog_full(FILE* fp, qdnode_t *qdnode, const char* nowstr)
{
	const int line_size = 1024;
	const int qfname_size = 256;
    char line[line_size];
    char qfname[qfname_size];
    FILE* qfptr = 0;
    apr_uint32_t bytes_written = 0;

    fmt_qlog(line, line_size, qdnode, nowstr, 1);
    bytes_written = strlen(line) + 1;
	if (bytes_written == line_size)
	{
		gpmon_warning(FLINE, "qlog line too long ... ignored: %s", line);
		return 0;
	}

	fprintf(fp, "%s", line);

	snprintf(qfname, qfname_size, GPMON_DIR "q%d-%d-%d.txt", qdnode->qlog.key.tmid,
            qdnode->qlog.key.ssid, qdnode->qlog.key.ccnt);

	qfptr = fopen(qfname, "r");
    if (!qfptr)
    {
	    fprintf(fp, "|||||\n");
	    bytes_written += 6;
	    return bytes_written;
    }

    // 0 add query text
    // 1 add query plan
    // 2 add application name
    // 3 add rsqname
    // 4 add priority

    int total_iterations = 5;
    int all_good = 1;
    int iter;
    int retCode = APR_SUCCESS;
    for (iter = 0; iter < total_iterations; ++iter)
    {
	    fprintf(fp, "|");
        bytes_written++;

        if (!all_good || iter == 1){
            // we have no data for query plan
            // if we failed once already don't bother trying to parse query file
            continue;
        }

        retCode = get_and_print_next_query_file_kvp(fp, qfptr, qfname, &bytes_written);
        if (retCode != APR_SUCCESS)
            all_good = 0;
    }

    fprintf(fp, "\n");
    fclose(qfptr);
	return bytes_written;
}

static void bloom_init(bloom_t* bloom)
{
    memset(bloom->map, 0, sizeof(bloom->map));
}

static void bloom_set(bloom_t* bloom, const char* name)
{
    apr_ssize_t namelen = strlen(name);
    const unsigned int hashval =
	apr_hashfunc_default(name, &namelen) % (8 * sizeof(bloom->map));
    const int idx = hashval / 8;
    const int off = hashval % 8;
    /* printf("bloom set %s h%d\n", name, hashval); */
    bloom->map[idx] |= (1 << off);
}

static int bloom_isset(bloom_t* bloom, const char* name)
{
    apr_ssize_t namelen = strlen(name);
    const unsigned int hashval =
	apr_hashfunc_default(name, &namelen) % (8 * sizeof(bloom->map));
    const int idx = hashval / 8;
    const int off = hashval % 8;
    /*
      printf("bloom check %s h%d = %d\n", name, hashval,
      0 != (bloom->map[idx] & (1 << off)));
    */
    return 0 != (bloom->map[idx] & (1 << off));
}


