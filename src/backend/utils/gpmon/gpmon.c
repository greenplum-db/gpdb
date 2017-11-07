#include "postgres.h"
#include "c.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#ifdef WIN32
#include <io.h>
#endif
#include "postgres.h"
#include "libpq/pqsignal.h"
#include "gpmon/gpmon.h"

#include "utils/memutils.h"
#include "utils/query_metrics.h"

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"

/* Extern stuff */
extern const char * show_session_authorization(void);
extern char *get_database_name(Oid dbid);

static void gpmon_record_kv(int32 tmid, int32 ssid, int32 ccnt,
						 const char* key,
						 const char* value,
						 bool extraNewLine);
static void gpmon_record_update(int32 tmid, int32 ssid,
								int32 ccnt, int32 status);
static const char* gpmon_null_subst(const char* input);


struct  {
    int    gxsock;
	pid_t  pid;
	struct sockaddr_in gxaddr;
} gpmon = {0};

int64 gpmon_tick = 0;

void gpmon_sig_handler(int sig);

void gpmon_sig_handler(int sig) 
{
	gpmon_tick++;
}


void gpmon_init(void)
{
	struct itimerval tv;
#ifndef WIN32
	pqsigfunc sfunc;
#endif
	pid_t pid = getpid();
	int sock;

	if (pid == gpmon.pid)
		return;
#ifndef WIN32
	sfunc = pqsignal(SIGVTALRM, gpmon_sig_handler);
	if (sfunc == SIG_ERR) {
		elog(WARNING, "gpmon: unable to set signal handler for SIGVTALRM (%m)");
	}
	else if (sfunc == gpmon_sig_handler) {
		close(gpmon.gxsock); 
		gpmon.gxsock = -1;
	}
	else {
		Assert(sfunc == 0);
	}
#endif

	tv.it_interval.tv_sec = gp_gpperfmon_send_interval;
	tv.it_interval.tv_usec = 0;
	tv.it_value = tv.it_interval;
#ifndef WIN32
	if (-1 == setitimer(ITIMER_VIRTUAL, &tv, 0)) {
		elog(WARNING, "gpmon: unable to start timer (%m)");
	}
#endif 

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		elog(WARNING, "gpmon: cannot create socket (%m)");
	}
#ifndef WIN32
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        elog(WARNING, "fcntl(F_SETFL, O_NONBLOCK) failed");
    }
    if (fcntl(sock, F_SETFD, 1) == -1) {
        elog(WARNING, "fcntl(F_SETFD) failed");
    }
#endif 
	gpmon.gxsock = sock;
	memset(&gpmon.gxaddr, 0, sizeof(gpmon.gxaddr));
	gpmon.gxaddr.sin_family = AF_INET;
	gpmon.gxaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	gpmon.gxaddr.sin_port = htons(gpperfmon_port);
	gpmon.pid = pid;
}

/**
 * This method adds a key-value entry to the gpmon text file. The format it uses is:
 * <VALUE_LENGTH> <KEY>\n
 * <VALUE>\n
 * Boolean value extraByte indicates whether an additional newline is desired. This is
 * necessary because gpmon overwrites the last byte to indicate status.
 */
static void gpmon_record_kv(int32 tmid, int32 ssid, int32 ccnt,
				  const char* key,
				  const char* value,
				  bool extraNewLine)
{
	char fname[GPMON_DIR_MAX_PATH];
	FILE* fp;
	int len = strlen(value);

	snprintf(fname, GPMON_DIR_MAX_PATH, "%sq%d-%d-%d.txt", GPMON_DIR, tmid, ssid, ccnt);

	fp = fopen(fname, "a");
	if (!fp)
		return;

	fprintf(fp, "%d %s\n", len, key);
	fwrite(value, 1, len, fp);
	fprintf(fp, "\n");

	if (extraNewLine)
	{
		fprintf(fp, "\n");
	}

	fclose(fp);
}

void gpmon_record_update(int32 tmid, int32 ssid, int32 ccnt,
						 int32 status)
{
	char fname[GPMON_DIR_MAX_PATH];
	FILE *fp;

	snprintf(fname, GPMON_DIR_MAX_PATH, "%sq%d-%d-%d.txt", GPMON_DIR, tmid, ssid, ccnt);

	fp = fopen(fname, "r+");

	if (!fp)
		return;

	if (0 == fseek(fp, -1, SEEK_END))
	{
		fprintf(fp, "%d", status);
	}
	fclose(fp);
}

void gpmon_gettmid(int32* tmid)
{
    char buff[TMGIDSIZE] = {0};
    int32 xid;
    getDistributedTransactionIdentifier(buff);
    sscanf(buff, "%d-%d", tmid, &xid);
} 


void gpmon_send(gpmon_packet_t* p)
{
	if (p->magic != GPMON_MAGIC)  {
		elog(WARNING, "gpmon - bad magic %x", p->magic);
		return;
	}

	if (p->pkttype == GPMON_PKTTYPE_QEXEC) {
		if (gp_perfmon_print_packet_info)
		{
			elog(LOG,
				 "Perfmon Executor Packet: (tmid, ssid, ccnt, segid, pid, nid, status) = "
				 "(%d, %d, %d, %d, %d, %d, %d)",
				 p->u.qexec.key.tmid, p->u.qexec.key.ssid, p->u.qexec.key.ccnt,
				 p->u.qexec.key.hash_key.segid, p->u.qexec.key.hash_key.pid, p->u.qexec.key.hash_key.nid,
				 p->u.qexec.status);
		}
	}
	
	if (gpmon.gxsock > 0) {
		int n = sizeof(*p);
		if (n != sendto(gpmon.gxsock, (const char *)p, n, 0, 
						(struct sockaddr*) &gpmon.gxaddr, 
						sizeof(gpmon.gxaddr))) {
			elog(LOG, "gpmon: cannot send (%m socket %d)", gpmon.gxsock);
		}
	}
}

#define GPMON_QLOG_PACKET_ASSERTS(gpmonPacket) \
		Assert((gp_enable_gpperfmon || gp_enable_query_metrics) && Gp_role == GP_ROLE_DISPATCH); \
		Assert(gpmonPacket); \
		Assert(gpmonPacket->magic == GPMON_MAGIC); \
		Assert(gpmonPacket->version == GPMON_PACKET_VERSION); \
		Assert(gpmonPacket->pkttype == GPMON_PKTTYPE_QLOG)

/**
 * To be used when gpmon packet has not been inited already.
 */
void gpmon_qlog_packet_init(gpmon_packet_t *gpmonPacket)
{
	const char *username = NULL;
	char *dbname = NULL;

	Assert((gp_enable_gpperfmon || gp_enable_query_metrics) && Gp_role == GP_ROLE_DISPATCH);
	Assert(gpmonPacket);
	Assert(gpmonPacket->magic != GPMON_MAGIC);
	
	gpmonPacket->magic = GPMON_MAGIC;
	gpmonPacket->version = GPMON_PACKET_VERSION;
	gpmonPacket->pkttype = GPMON_PKTTYPE_QLOG;

	gpmon_gettmid(&gpmonPacket->u.qlog.key.tmid);
	gpmonPacket->u.qlog.key.ssid = gp_session_id;

	username = show_session_authorization(); /* does not have to be freed */
	/* User Id.  We use session authorization (so to make sense with session id) */
	snprintf(gpmonPacket->u.qlog.user, sizeof(gpmonPacket->u.qlog.user), "%s",
			username ? username : "");

	/* DB Id */
	dbname = get_database_name(MyDatabaseId); /* needs to be freed */
	snprintf(gpmonPacket->u.qlog.db, sizeof(gpmonPacket->u.qlog.db), "%s", dbname ? dbname : ""); 
	if(dbname)
	{
		pfree(dbname);
		dbname = NULL;
	}

	/* Fix up command count */
	gpmonPacket->u.qlog.key.ccnt = gp_command_count;
}

/**
 * Call this method when query is submitted.
 */
void gpmon_qlog_query_submit(gpmon_packet_t *gpmonPacket)
{
	struct timeval tv;

	GPMON_QLOG_PACKET_ASSERTS(gpmonPacket);

	gettimeofday(&tv, 0);
	
	gpmonPacket->u.qlog.status = GPMON_QLOG_STATUS_SUBMIT;
	gpmonPacket->u.qlog.tsubmit = tv.tv_sec;

	if (!gp_enable_gpperfmon)
		return;

	gpmon_record_update(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			gpmonPacket->u.qlog.status);

	gpmon_send(gpmonPacket);
}

/**
 * Wrapper function that returns string if not null. Returns GPMON_UNKNOWN if it is null.
 */
static const char* gpmon_null_subst(const char* input)
{
	return input ? input : GPMON_UNKNOWN;
}


/**
 * Call this method to let gpmon know the query text, application name, resource queue name and priority
 * at submit time. It writes 4 key value pairs using keys: qtext, appname, resqname and priority using
 * the format as described in gpmon_record_kv().
 */

void gpmon_qlog_query_text(const gpmon_packet_t *gpmonPacket,
		const char *queryText,
		const char *appName,
		const char *resqName,
		const char *resqPriority)
{
	GPMON_QLOG_PACKET_ASSERTS(gpmonPacket);

	if (!gp_enable_gpperfmon)
		return;

	queryText = gpmon_null_subst(queryText);
	appName = gpmon_null_subst(appName);
	resqName = gpmon_null_subst(resqName);
	resqPriority = gpmon_null_subst(resqPriority);

	Assert(queryText);
	Assert(appName);
	Assert(resqName);
	Assert(resqPriority);

	gpmon_record_kv(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			"qtext", queryText, false);

	gpmon_record_kv(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			"appname", appName, false);

	gpmon_record_kv(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			"resqname", resqName, false);

	gpmon_record_kv(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			"priority", resqPriority, true);

	gpmon_record_update(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			GPMON_QLOG_STATUS_SUBMIT);

}

/**
 * Call this method when query starts executing.
 */
void gpmon_qlog_query_start(gpmon_packet_t *gpmonPacket)
{
	struct timeval tv;

	GPMON_QLOG_PACKET_ASSERTS(gpmonPacket);

	gettimeofday(&tv, 0);
	
	gpmonPacket->u.qlog.status = GPMON_QLOG_STATUS_START;
	gpmonPacket->u.qlog.tstart = tv.tv_sec;

	if (!gp_enable_gpperfmon)
		return;

	gpmon_record_update(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			gpmonPacket->u.qlog.status);

	gpmon_send(gpmonPacket);
}

/**
 * Call this method when query finishes executing.
 */
void gpmon_qlog_query_end(gpmon_packet_t *gpmonPacket)
{
	struct timeval tv;

	GPMON_QLOG_PACKET_ASSERTS(gpmonPacket);
	Assert(gpmonPacket->u.qlog.status == GPMON_QLOG_STATUS_START);
	gettimeofday(&tv, 0);
	
	gpmonPacket->u.qlog.status = GPMON_QLOG_STATUS_DONE;
	gpmonPacket->u.qlog.tfin = tv.tv_sec;

	if (!gp_enable_gpperfmon)
		return;
	
	gpmon_record_update(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			gpmonPacket->u.qlog.status);

	gpmon_send(gpmonPacket);
}

/**
 * Call this method when query errored out.
 */
void gpmon_qlog_query_error(gpmon_packet_t *gpmonPacket)
{
	struct timeval tv;

	GPMON_QLOG_PACKET_ASSERTS(gpmonPacket);
	Assert(gpmonPacket->u.qlog.status == GPMON_QLOG_STATUS_START ||
		   gpmonPacket->u.qlog.status == GPMON_QLOG_STATUS_SUBMIT ||
		   gpmonPacket->u.qlog.status == GPMON_QLOG_STATUS_CANCELING);

	gettimeofday(&tv, 0);

	gpmonPacket->u.qlog.status = GPMON_QLOG_STATUS_ERROR;
	gpmonPacket->u.qlog.tfin = tv.tv_sec;

	if (!gp_enable_gpperfmon)
		return;

	gpmon_record_update(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			gpmonPacket->u.qlog.status);

	gpmon_send(gpmonPacket);
}

/*
 * gpmon_qlog_query_canceling
 *    Record that the query is being canceled.
 */
void
gpmon_qlog_query_canceling(gpmon_packet_t *gpmonPacket)
{
	GPMON_QLOG_PACKET_ASSERTS(gpmonPacket);
	Assert(gpmonPacket->u.qlog.status == GPMON_QLOG_STATUS_START ||
		   gpmonPacket->u.qlog.status == GPMON_QLOG_STATUS_SUBMIT);

	gpmonPacket->u.qlog.status = GPMON_QLOG_STATUS_CANCELING;

	if (!gp_enable_gpperfmon)
		return;

	gpmon_record_update(gpmonPacket->u.qlog.key.tmid,
			gpmonPacket->u.qlog.key.ssid,
			gpmonPacket->u.qlog.key.ccnt,
			gpmonPacket->u.qlog.status);

	gpmon_send(gpmonPacket);
}

