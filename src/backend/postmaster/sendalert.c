/*-------------------------------------------------------------------------
 *
 * sendalert.c
 *	  Send alerts via SMTP (email) or SNMP INFORM messages.
 *
 * Portions Copyright (c) 2009, Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/postmaster/sendalert.c
 *
 *-------------------------------------------------------------------------
 */
#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE<600
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE<200112L
#undef _POSIX_C_SOURCE
/* Define to activate features from IEEE Stds 1003.1-2001 */
#define _POSIX_C_SOURCE 200112L
#endif
#include "postgres.h"
#include "pg_config.h"  /* Adding this helps eclipse see that USE_SNMP is set */

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <arpa/inet.h>

#include "lib/stringinfo.h"

#include "pgtime.h"

#include "postmaster/syslogger.h"
#include "postmaster/sendalert.h"
#include "utils/guc.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "sendalert_common.h"

extern int	PostPortNumber;

#if defined(HAVE_DECL_CURLOPT_MAIL_FROM) && HAVE_DECL_CURLOPT_MAIL_FROM
#include <curl/curl.h>
#endif

#ifdef USE_SNMP
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/definitions.h>
#include <net-snmp/types.h>

//#include <net-snmp/utilities.h>
#include <net-snmp/session_api.h>
#include <net-snmp/pdu_api.h>
#include <net-snmp/mib_api.h>
#include <net-snmp/varbind_api.h>
//#include <net-snmp/config_api.h>
#include <net-snmp/output_api.h>


oid             objid_enterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
oid             objid_sysdescr[] = { 1, 3, 6, 1, 2, 1, 1, 1, 0 };
oid             objid_sysuptime[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
oid             objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
oid				objid_rdbmsrelstate[] = { 1, 3, 6, 1, 2, 1, 39, 1, 9, 1, 1 };

// {iso(1) identified-organization(3) dod(6) internet(1) private(4) enterprises(1) gpdbMIB(31327) gpdbObjects(1) gpdbAlertMsg(1)}
oid				objid_gpdbAlertMsg[] = { 1, 3, 6, 1, 4, 1, 31327, 1, 1 };
oid				objid_gpdbAlertSeverity[] = { 1, 3, 6, 1, 4, 1, 31327, 1, 2 };
oid				objid_gpdbAlertSqlstate[] = { 1, 3, 6, 1, 4, 1, 31327, 1, 3 };
oid				objid_gpdbAlertDetail[] = { 1, 3, 6, 1, 4, 1, 31327, 1, 4 };
oid				objid_gpdbAlertSqlStmt[] = { 1, 3, 6, 1, 4, 1, 31327, 1, 5 };
oid				objid_gpdbAlertSystemName[] = { 1, 3, 6, 1, 4, 1, 31327, 1, 6 };
#endif

#if defined(HAVE_DECL_CURLOPT_MAIL_FROM) && HAVE_DECL_CURLOPT_MAIL_FROM
/* state information for messagebody_cb function */
typedef struct
{
	StringInfoData body;
	size_t consumed;
} upload_ctx;

static size_t messagebody_cb(void *ptr, size_t size, size_t nmemb, void *userp);
static void build_messagebody(StringInfo buf, const GpErrorData *errorData,
				  const char *subject, const char *email_priority);

static void send_alert_via_email(const GpErrorData * errorData,
					 const char * subject, const char * email_priority);
static char *extract_email_addr(char *str);
static bool SplitMailString(char *rawstring, char delimiter, List **namelist);
#endif

#ifdef USE_SNMP
static int send_snmp_inform_or_trap();
extern pg_time_t	MyStartTime;
#endif


int send_alert_from_chunks(const PipeProtoChunk *chunk,
		const PipeProtoChunk * saved_chunks_in)
{

	int ret = -1;
	GpErrorData errorData;

	CSVChunkStr chunkstr =
	{ chunk, chunk->data + sizeof(GpErrorDataFixFields) };

	memset(&errorData, 0, sizeof(errorData));

	memcpy(&errorData.fix_fields, chunk->data, sizeof(errorData.fix_fields));

	if (chunk == NULL)
		return -1;
	if (chunk->hdr.len == 0)
		return -1;
	if (chunk->hdr.zero != 0)
		return -1;

	if (chunk->hdr.log_format != 'c')
		elog(ERROR,"send_alert_from_chunks only works when CSV logging is enabled");

	errorData.username = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.databasename = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.remote_host = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.remote_port = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_severity = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.sql_state = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_message = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_detail = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_hint = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.internal_query = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_context = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.debug_query_string = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_func_name = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.error_filename = get_str_from_chunk(&chunkstr,saved_chunks_in);
	errorData.stacktrace = get_str_from_chunk(&chunkstr,saved_chunks_in);


	PG_TRY();
	{
	ret = send_alert(&errorData);
	}
	PG_CATCH();
	{
		elog(LOG,"send_alert failed.  Not sending the alert");
		free(errorData.stacktrace ); errorData.stacktrace = NULL;
		free((char *)errorData.error_filename ); errorData.error_filename = NULL;
		free((char *)errorData.error_func_name ); errorData.error_func_name = NULL;
		free(errorData.debug_query_string ); errorData.debug_query_string = NULL;
		free(errorData.error_context); errorData.error_context = NULL;
		free(errorData.internal_query ); errorData.internal_query = NULL;
		free(errorData.error_hint ); errorData.error_hint = NULL;
		free(errorData.error_detail ); errorData.error_detail = NULL;
		free(errorData.error_message ); errorData.error_message = NULL;
		free(errorData.sql_state ); errorData.sql_state = NULL;
		free((char *)errorData.error_severity ); errorData.error_severity = NULL;
		free(errorData.remote_port ); errorData.remote_port = NULL;
		free(errorData.remote_host ); errorData.remote_host = NULL;
		free(errorData.databasename ); errorData.databasename = NULL;
		free(errorData.username ); errorData.username = NULL;
		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	// Don't forget to free them!  Best in reverse order of the mallocs.

	free(errorData.stacktrace ); errorData.stacktrace = NULL;
	free((char *)errorData.error_filename ); errorData.error_filename = NULL;
	free((char *)errorData.error_func_name ); errorData.error_func_name = NULL;
	free(errorData.debug_query_string ); errorData.debug_query_string = NULL;
	free(errorData.error_context); errorData.error_context = NULL;
	free(errorData.internal_query ); errorData.internal_query = NULL;
	free(errorData.error_hint ); errorData.error_hint = NULL;
	free(errorData.error_detail ); errorData.error_detail = NULL;
	free(errorData.error_message ); errorData.error_message = NULL;
	free(errorData.sql_state ); errorData.sql_state = NULL;
	free((char *)errorData.error_severity ); errorData.error_severity = NULL;
	free(errorData.remote_port ); errorData.remote_port = NULL;
	free(errorData.remote_host ); errorData.remote_host = NULL;
	free(errorData.databasename ); errorData.databasename = NULL;
	free(errorData.username ); errorData.username = NULL;

	return ret;
}

#ifdef USE_SNMP
static int send_snmp_inform_or_trap(const GpErrorData * errorData, const char * subject, const char * severity)
{

	netsnmp_session session, *ss = NULL;
	netsnmp_pdu    *pdu, *response;
	int				status;
	char            csysuptime[20];
	static bool 	snmp_initialized = false;
	static char myhostname[255];	/* gethostname usually is limited to 65 chars out, but make this big to be safe */
	char	   *rawstring = NULL;
	List	   *elemlist = NIL;
	ListCell   *l = NULL;

	/*
	 * "inform" messages get a positive acknowledgement response from the SNMP manager.
	 * If it doesn't come, the message might be resent.
	 *
	 * "trap" messages are one-way, and we have no idea if the manager received it.
	 * But, it's faster and cheaper, and no need to retry.  So some people might prefer it.
	 */
	bool inform = strcmp(gp_snmp_use_inform_or_trap,"inform") == 0;


	if (gp_snmp_monitor_address == NULL || gp_snmp_monitor_address[0] == '\0')
	{
		static bool firsttime = 1;

		ereport(firsttime ? LOG : DEBUG1,(errmsg("SNMP inform/trap alerts are disabled")));
		firsttime = false;

		return -1;
	}


	/*
	 * SNMP managers are required to handle messages up to at least 484 bytes long, but I believe most existing
	 * managers support messages up to one packet (ethernet frame) in size, 1472 bytes.
	 *
	 * But, should we take that chance?  Or play it safe and limit the message to 484 bytes?
	 */

	elog(DEBUG2,"send_snmp_inform_or_trap");

	if (!snmp_initialized)
	{
		snmp_enable_stderrlog();

		if (gp_snmp_debug_log != NULL && gp_snmp_debug_log[0] != '\0')
		{
			snmp_enable_filelog(gp_snmp_debug_log, 1);

			//debug_register_tokens("ALL");
			snmp_set_do_debugging(1);
		}

		/*
		 * Initialize the SNMP library.  This also reads the MIB database.
		 */
		/* Add GPDB-MIB to the list to be loaded */
		putenv("MIBS=+GPDB-MIB:SNMP-FRAMEWORK-MIB:SNMPv2-CONF:SNMPv2-TC:SNMPv2-TC");

		init_snmp("sendalert");

		snmp_initialized = true;

		{
			char portnum[16];
			myhostname[0] = '\0';

			if (gethostname(myhostname, sizeof(myhostname)) == 0)
			{
				strcat(myhostname,":");
				pg_ltoa(PostPortNumber,portnum);
				strcat(myhostname,portnum);
			}
		}
	}

	/*
	 * Trap/Inform messages always start with the system up time. (SysUpTime.0)
	 *
	 * This presumably would be the uptime of GPDB, not the machine it is running on, I think.
	 *
	 * Use Postmaster's "MyStartTime" as a way to get that.
	 */

	sprintf(csysuptime, "%ld", (long)(time(NULL) - MyStartTime));


	/*
	// ERRCODE_DISK_FULL could be reported vi rbmsMIB rdbmsTraps rdbmsOutOfSpace trap.
	// But it appears we never generate that error?

	// ERRCODE_ADMIN_SHUTDOWN means SysAdmin aborted somebody's request.  Not interesting?

	// ERRCODE_CRASH_SHUTDOWN sounds interesting, but I don't see that we ever generate it.

	// ERRCODE_CANNOT_CONNECT_NOW means we are starting up, shutting down, in recovery, or Too many users are logged on.

	// abnormal database system shutdown
	*/



	/*
	 * The gpdbAlertSeverity is a crude attempt to classify some of these messages based on severity,
	 * where OK means everything is running normal, Down means everything is shut down, degraded would be
	 * for times when some segments are down, but the system is up, The others are maybe useful in the future
	 *
	 *  gpdbSevUnknown(0),
	 *	gpdbSevOk(1),
	 *	gpdbSevWarning(2),
	 *	gpdbSevError(3),
	 *	gpdbSevFatal(4),
	 *	gpdbSevPanic(5),
	 *	gpdbSevSystemDegraded(6),
	 *	gpdbSevSystemDown(7)
	 */


	char detail[MAX_ALERT_STRING+1];
	snprintf(detail, MAX_ALERT_STRING, "%s", errorData->error_detail);
	detail[127] = '\0';

	char sqlstmt[MAX_ALERT_STRING+1];
	char * sqlstmtp = errorData->debug_query_string;
	if (sqlstmtp == NULL || sqlstmtp[0] == '\0')
		sqlstmtp = errorData->internal_query;
	if (sqlstmtp == NULL)
		sqlstmtp = "";


	snprintf(sqlstmt, MAX_ALERT_STRING, "%s", sqlstmtp);
	sqlstmt[MAX_ALERT_STRING] = '\0';


	/* Need a modifiable copy of To list */
	rawstring = pstrdup(gp_snmp_monitor_address);

	/* Parse string into list of identifiers */
	if (!SplitMailString(rawstring, ',', &elemlist))
	{
		/* syntax error in list */
		ereport(LOG,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid list syntax for \"gp_snmp_monitor_address\"")));
		return -1;
	}


	/*
	 * This session is just a template, and doesn't need to be connected.
	 * It is used by snmp_add(), which copies this info, opens the new session, and assigns the transport.
	 */
	snmp_sess_init( &session );	/* Initialize session to default values */
	session.version = SNMP_VERSION_2c;
	session.timeout = SNMP_DEFAULT_TIMEOUT;
	session.retries = SNMP_DEFAULT_RETRIES;
	session.remote_port = 162; 							// I think this isn't used by net-snmp any more.

	/*if (strchr(session.peername,':')==NULL)
		strcat(session.peername,":162");*/
	session.community = (u_char *)gp_snmp_community;
	session.community_len = strlen((const char *)session.community);  // SNMP_DEFAULT_COMMUNITY_LEN means "public"
	session.callback_magic = NULL;

	foreach(l, elemlist)
	{
		char	   *cur_gp_snmp_monitor_address = (char *) lfirst(l);

		if (cur_gp_snmp_monitor_address == NULL || cur_gp_snmp_monitor_address[0] == '\0')
			continue;

		session.peername = cur_gp_snmp_monitor_address;
		/*
		 * If we try to "snmp_open( &session )", net-snmp will set up a connection to that
		 * endpoint on port 161, assuming we are the network monitor, and the other side is an agent.
		 *
		 * But we are pretending to be an agent, sending traps to the NM, so we don't need this.
		 */

		/*if (!snmp_open( &session ))   	// Don't open the session here!
		{
			const char     *str;
			int             xerr;
			xerr = snmp_errno;
			str = snmp_api_errstring(xerr);
			elog(LOG, "snmp_open: %s", str);
			return -1;
		}*/

		/*
		 * This call copies the info from "session" to "ss", assigns the transport, and opens the session.
		 * We must specify "snmptrap" so the transport will know we want port 162 by default.
		 */
		ss = snmp_add(&session,
					  netsnmp_transport_open_client("snmptrap", cur_gp_snmp_monitor_address),
					  NULL, NULL);
		if (ss == NULL) {
			/*
			 * diagnose netsnmp_transport_open_client and snmp_add errors with
			 * the input netsnmp_session pointer
			 */
			{
				char           *err;
				snmp_error(&session, NULL, NULL, &err);
				elog(LOG, "send_alert snmp_add of %s failed: %s", cur_gp_snmp_monitor_address, err);
				free(err);
			}
			return -1;
		}

		/*
		 * We need to create the pdu each time, as it gets freed when we send a trap.
		 */
		pdu = snmp_pdu_create(inform ? SNMP_MSG_INFORM : SNMP_MSG_TRAP2);
		if (!pdu)
		{
			const char     *str;
			int             xerr;
			xerr = snmp_errno;
			str = snmp_api_errstring(xerr);
			elog(LOG, "Failed to create notification PDU: %s", str);
			return -1;
		}

		/*
		 * Trap/Inform messages always start with the system up time. (SysUpTime.0)
		 * We use Postmaster's "MyStartTime" as a way to get that.
		 */

		snmp_add_var(pdu, objid_sysuptime,
							sizeof(objid_sysuptime) / sizeof(oid),
							't', (const char *)csysuptime);

	#if 0
		/*
		 * In the future, we might want to send RDBMS-MIB::rdbmsStateChange when the system becomes unavailable or
		 * partially unavailable.  This code, which is not currently used, shows how to build the pdu for
		 * that trap.
		 */
		/* {iso(1) identified-organization(3) dod(6) internet(1) mgmt(2) mib-2(1) rdbmsMIB(39) rdbmsTraps(2) rdbmsStateChange(1)} */
		snmp_add_var(pdu, objid_snmptrap,
							sizeof(objid_snmptrap) / sizeof(oid),
							'o', "1.3.6.1.2.1.39.2.1");  // rdbmsStateChange


		snmp_add_var(pdu, objid_rdbmsrelstate,
							sizeof(objid_rdbmsrelstate) / sizeof(oid),
							'i', "5");  // 4 = restricted, 5 = unavailable
	#endif

		/* {iso(1) identified-organization(3) dod(6) internet(1) private(4) enterprises(1) gpdbMIB(31327) gpdbTraps(5) gpdbTrapsList(0) gpdbAlert(1)} */

		/*
		 * We could specify this trap oid by name, rather than numeric oid, but then if the GPDB-MIB wasn't
		 * found, we'd get an error.  Using the numeric oid means we can still work without the MIB loaded.
		 */
		snmp_add_var(pdu, objid_snmptrap,
							sizeof(objid_snmptrap) / sizeof(oid),
							'o', "1.3.6.1.4.1.31327.5.0.1");  // gpdbAlert


		snmp_add_var(pdu, objid_gpdbAlertMsg,
							sizeof(objid_gpdbAlertMsg) / sizeof(oid),
							's', subject);  // SnmpAdminString = UTF-8 text
		snmp_add_var(pdu, objid_gpdbAlertSeverity,
							sizeof(objid_gpdbAlertSeverity) / sizeof(oid),
							'i', (char *)severity);

		snmp_add_var(pdu, objid_gpdbAlertSqlstate,
							sizeof(objid_gpdbAlertSqlstate) / sizeof(oid),
							's', errorData->sql_state);

		snmp_add_var(pdu, objid_gpdbAlertDetail,
							sizeof(objid_gpdbAlertDetail) / sizeof(oid),
							's', detail); // SnmpAdminString = UTF-8 text

		snmp_add_var(pdu, objid_gpdbAlertSqlStmt,
							sizeof(objid_gpdbAlertSqlStmt) / sizeof(oid),
							's', sqlstmt); // SnmpAdminString = UTF-8 text

		snmp_add_var(pdu, objid_gpdbAlertSystemName,
							sizeof(objid_gpdbAlertSystemName) / sizeof(oid),
							's', myhostname); // SnmpAdminString = UTF-8 text



		elog(DEBUG2,"ready to send to %s",cur_gp_snmp_monitor_address);
		if (inform)
			status = snmp_synch_response(ss, pdu, &response);
		else
			status = snmp_send(ss, pdu) == 0;

		elog(DEBUG2,"send, status %d",status);
		if (status != STAT_SUCCESS)
		{
			/* Something went wrong */
			if (ss)
			{
				char           *err;
				snmp_error(ss, NULL, NULL, &err);
				elog(LOG, "sendalert failed to send %s: %s", inform ? "inform" : "trap", err);
				free(err);
			}
			else
			{
				elog(LOG, "sendalert failed to send %s: %s", inform ? "inform" : "trap", "Something went wrong");
			}
			if (!inform)
				snmp_free_pdu(pdu);
		}
		else if (inform)
			snmp_free_pdu(response);

		snmp_close(ss);
		ss = NULL;

	}

	/*
	 * don't do the shutdown, to avoid the cost of starting up snmp each time
	 * (plus, it doesn't seem to work to run snmp_init() again after a shutdown)
	 *
	 * It would be nice to call this when the syslogger is shutting down.
	 */
	/*snmp_shutdown("sendalert");*/


	return 0;
}
#endif

#if defined(HAVE_DECL_CURLOPT_MAIL_FROM) && HAVE_DECL_CURLOPT_MAIL_FROM
static void
send_alert_via_email(const GpErrorData *errorData,
					 const char *subject, const char *email_priority)
{
	CURL	   *curl;
	upload_ctx	upload_ctx;
	char	   *rawstring;
	List	   *elemlist = NIL;
	ListCell   *l;
	static int	num_connect_failures = 0;
	static time_t last_connect_failure_ts = 0;
	char		smtp_url[200];
	int			num_recipients;
	int			num_sent_successfully;
	char	   *from = NULL;

	if (gp_email_connect_failures && num_connect_failures >= gp_email_connect_failures)
	{
		if (time(0) - last_connect_failure_ts > gp_email_connect_avoid_duration)
		{
			num_connect_failures = 0;
			elog(LOG, "Retrying emails now...");
		}
		else
		{
			elog(LOG, "Not attempting emails as of now");
			return;
		}
	}

	if (gp_email_to == NULL || strlen(gp_email_to) == 0)
	{
		static bool firsttime = 1;

		ereport(firsttime ? LOG : DEBUG1,(errmsg("e-mail alerts are disabled")));
		firsttime = false;
		return;
	}

    /*
     * Per curl docs/example:
	 *
     * Note that this option isn't strictly required, omitting it will result
     * in libcurl sending the MAIL FROM command with empty sender data. All
     * autoresponses should have an empty reverse-path, and should be directed
     * to the address in the reverse-path which triggered them. Otherwise, they
     * could cause an endless loop. See RFC 5321 Section 4.5.5 for more details.
     */
	if (strlen(gp_email_from) == 0)
	{
		elog(LOG, "e-mail alerts are not properly configured:  No 'from:' address configured");
		return;
	}

	if (strchr(gp_email_to, ';') == NULL && strchr(gp_email_to, ',') != NULL)
	{
		// email addrs should be separated by ';', but because we used to require ',',
		// let's accept that if it looks OK.
		while (strchr(gp_email_to,',') != NULL)
			*strchr(gp_email_to,',') = ';';
	}

	from = extract_email_addr(gp_email_from);

	/* Build the message headers + body */
	initStringInfo(&upload_ctx.body);
	build_messagebody(&upload_ctx.body, errorData, subject, email_priority);

	if (gp_email_smtp_server != NULL && strlen(gp_email_smtp_server) > 0)
		snprintf(smtp_url, sizeof(smtp_url), "smtp://%s", gp_email_smtp_server);
	else
		snprintf(smtp_url, sizeof(smtp_url), "smtp://localhost");

	/* Need a modifiable copy of To list */
	rawstring = pstrdup(gp_email_to);

	/* Parse string into list of identifiers */
	if (!SplitMailString(rawstring, ';', &elemlist))
	{
		/* syntax error in list */
		ereport(LOG,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid list syntax for \"gp_email_to\"")));
	}

	/*
	 * Ok, let's initialize libcurl.
	 */
	curl = curl_easy_init();
	if (curl)
	{
		PG_TRY();
		{
			/*
			 * Loop around the list of recipients, and send the same message
			 * to each one. We send all the messages using the same
			 * easy-handle; that allows curl to reuse the same connection for
			 * each sent message.
			 *
			 * XXX: libcurl supports sending to a list of recipients, but if
			 * the delivery to any one of them fails, it bails out without
			 * sending to any of the recipients. See curl known bug #70,
			 * http://curl.haxx.se/bug/view.cgi?id=1116
			 *
			 * If that ever gets fixed in libcurl, we should build a single
			 * list of recipients here and only send the mail once.
			 */
			num_recipients = 0;
			num_sent_successfully = 0;
			foreach(l, elemlist)
			{
				char	   *cur_gp_email_addr = (char *) lfirst(l);
				char	   *to;
				struct curl_slist *recipient;
				bool		connect_failure = false;
				CURLcode	res;
				char		curlerror[CURL_ERROR_SIZE];

				if (cur_gp_email_addr == NULL || *cur_gp_email_addr == '\0')
					continue;
				num_recipients++;

				/* Recipient must be in RFC2821 format */
				to = extract_email_addr(cur_gp_email_addr);

				recipient = curl_slist_append(NULL, to);

				if (recipient == NULL)
				{
					elog(LOG, "could not append recipient %s", to);
					pfree(to);
					continue;
				}

				curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerror);
				curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
				curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
								 (long) gp_email_connect_timeout);
				if (gp_email_smtp_userid && *gp_email_smtp_userid)
					curl_easy_setopt(curl, CURLOPT_USERNAME,
									 gp_email_smtp_userid);
				if (gp_email_smtp_password && *gp_email_smtp_password)
					curl_easy_setopt(curl, CURLOPT_PASSWORD,
									 gp_email_smtp_password);
				if (from && *from)
					curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from);
				curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipient);

				/*
				 * Use a callback function to pass the payload (headers and
				 * the message body) to libcurl.
				 */
				upload_ctx.consumed = 0;
				curl_easy_setopt(curl, CURLOPT_READFUNCTION, messagebody_cb);
				curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

				/* Ok, all set. Send the message */
				res = curl_easy_perform(curl);

				curl_slist_free_all(recipient);

				/* Did it succeed? */
				switch(res)
				{
					case CURLE_OK:
						num_connect_failures = 0;
						num_sent_successfully++;
						break;

						/*
						 * If we get any of these errors, something went wrong
						 * with connecting to the server. Treat it as a
						 * connection failure, and bail out immediately
						 * without trying the rest of the recipients.
						 */
					case CURLE_OPERATION_TIMEDOUT:
					case CURLE_UNSUPPORTED_PROTOCOL:
					case CURLE_URL_MALFORMAT:
					case CURLE_COULDNT_RESOLVE_PROXY:
					case CURLE_COULDNT_RESOLVE_HOST:
					case CURLE_COULDNT_CONNECT:
						ereport(LOG,
								(errmsg("could not connect to SMTP server"),
								 errdetail("%s", curlerror)));
						connect_failure = true;
						break;

						/*
						 * Any other error might be because of an invalid
						 * recipient, for example, and we might still succeed
						 * in sending to the other recipients. Continue.
						 */
					default:
						ereport(LOG,
								(errmsg("could not send alert email to \"%s\"", to),
								 errdetail("%s", curlerror)));
						break;
				}

				pfree(to);

				if (connect_failure)
					break;
			}
			if (num_recipients == 0 && list_length(elemlist) > 0)
				ereport(LOG,
						(errmsg("Could not understand e-mail To: list")));
			else if (num_sent_successfully == 0)
			{
				/*
				 * If we couldn't deliver the mail to any recipients, treat it
				 * as a connection failure.
				 */
				num_connect_failures++;
				last_connect_failure_ts = time(NULL);
			}
		}
		PG_CATCH();
		{
			/*
			 * Close the handle. This closes the connection to the SMTP
			 * server.
			 */
			curl_easy_cleanup(curl);

			PG_RE_THROW();
		}
		PG_END_TRY();

		/* Close the handle. This closes the connection to the SMTP server. */
		curl_easy_cleanup(curl);
	}
	else
		elog(LOG, "Unable to send e-mail: curl_easy_init failed");

	/*
	 * Free other stuff we allocated. We run in a long-lived memory context
	 * so we have to be careful to not leak.
	 */
	pfree(rawstring);
	list_free(elemlist);
	pfree(from);
	pfree(upload_ctx.body.data);
}
#endif

int send_alert(const GpErrorData * errorData)
{

	char subject[128];
	bool send_via_email = true;
	char email_priority[2];
	bool send_via_snmp = true;
	char snmp_severity[2];

	static char previous_subject[128];
	pg_time_t current_time;
	static pg_time_t previous_time;
	static GpErrorDataFixFields previous_fix_fields;

	elog(DEBUG2,"send_alert: %s: %s",errorData->error_severity, errorData->error_message);

	/*
	 * SIGPIPE must be ignored, or we will have problems.
	 *
	 * but this is already set in syslogger.c, so we are OK
	 * //pqsignal(SIGPIPE, SIG_IGN);  // already set in syslogger.c
	 *
	 */

	/* Set up a subject line for the alert.  set_alert_severity will limit it to 127 bytes just to be safe. */
	/* Assign a severity and email priority for this alert*/

	set_alert_severity(errorData,
						subject,
						&send_via_email,
						email_priority,
						&send_via_snmp,
						snmp_severity);


	/*
	 * Check to see if we are sending the same message as last time.
	 * This could mean the system is in a loop generating an error over and over,
	 * or the application is just re-doing the same bad request over and over.
	 *
	 * This is pretty crude, as we don't consider loops where we alternate between a small
	 * number of messages.
	 */

	if (strcmp(subject,previous_subject)==0)
	{
		/*
		 * Looks like the same message based on the errMsg, but we need to
		 * investigate further.
		 */
		bool same_message_repeated = true;

		/*
		 * If the message is from a different segDB, consider it a different message.
		 */
		if (errorData->fix_fields.gp_segment_id != previous_fix_fields.gp_segment_id)
			same_message_repeated = false;
		if (errorData->fix_fields.gp_is_primary != previous_fix_fields.gp_is_primary)
			same_message_repeated = false;
		/*
		 * If the message is from a different user, consider it a different message,
		 * unless it is a FATAL, because an application repeatedly sending in a request
		 * that crashes (SIGSEGV) will get a new session ID each time
		 */
		if (errorData->fix_fields.gp_session_id != previous_fix_fields.gp_session_id)
			if (strcmp(errorData->error_severity,"FATAL") != 0)
				same_message_repeated = false;
		/*
		 * Don't consider gp_command_count, because a loop where the application is repeatedly
		 * sending a bad request will have a changing command_count.
		 *
		 * Likewise, the transaction ids will be changing each time, so don't consider them.
		 */

		if (same_message_repeated)
		{
			current_time = (pg_time_t)time(NULL);
			/*
			 * This is the same alert as last time.  Limit us to one repeat alert every 30 seconds
			 * to avoid spamming the sysAdmin's mailbox or the snmp network monitor.
			 *
			 * We don't just turn off the alerting until a different message comes in, because
			 * if enough time has passed, this message might (probably?) refer to a new issue.
			 *
			 * Note that the message will still exist in the log, it's just that we won't
			 * send it via e-mail or snmp notification.
			 */

			if (current_time - previous_time < 30)
			{
				/* Bail out here rather than send the alert. */
				elog(DEBUG2,"Suppressing repeats of this alert messages...");
				return -1;
			}
		}

	}

	strcpy(previous_subject, subject);
	previous_time = (pg_time_t)time(NULL);
	memcpy(&previous_fix_fields,&errorData->fix_fields,sizeof(GpErrorDataFixFields));

#ifdef USE_SNMP
	if (send_via_snmp)
		send_snmp_inform_or_trap(errorData, subject, snmp_severity);
	else 
		elog(DEBUG4,"Not sending via SNMP");
#endif

#if defined(HAVE_DECL_CURLOPT_MAIL_FROM) && HAVE_DECL_CURLOPT_MAIL_FROM
	if (send_via_email)
		send_alert_via_email(errorData, subject, email_priority);
	else
		elog(DEBUG4,"Not sending via e-mail");
#endif

	return 0;
}


static size_t
pg_strnlen(const char *str, size_t maxlen)
{
	const char *p = str;

	while (maxlen-- > 0 && *p)
		p++;
	return p - str;
}

static void move_to_next_chunk(CSVChunkStr * chunkstr,
		const PipeProtoChunk * saved_chunks)
{
	Assert(chunkstr != NULL);
	Assert(saved_chunks != NULL);

	if (chunkstr->chunk != NULL)
		if (chunkstr->p - chunkstr->chunk->data >= chunkstr->chunk->hdr.len)
		{
			/* switch to next chunk */
			if (chunkstr->chunk->hdr.next >= 0)
			{
				chunkstr->chunk = &saved_chunks[chunkstr->chunk->hdr.next];
				chunkstr->p = chunkstr->chunk->data;
			}
			else
			{
				/* no more chunks */
				chunkstr->chunk = NULL;
				chunkstr->p = NULL;
			}
		}
}

char *
get_str_from_chunk(CSVChunkStr *chunkstr, const PipeProtoChunk *saved_chunks)
{
	int wlen = 0;
	int len = 0;
	char * out = NULL;

	Assert(chunkstr != NULL);
	Assert(saved_chunks != NULL);

	move_to_next_chunk(chunkstr, saved_chunks);

	if (chunkstr->p == NULL)
	{
		return strdup("");
	}

	len = chunkstr->chunk->hdr.len - (chunkstr->p - chunkstr->chunk->data);

	/* Check if the string is an empty string */
	if (len > 0 && chunkstr->p[0] == '\0')
	{
		chunkstr->p++;
		move_to_next_chunk(chunkstr, saved_chunks);

		return strdup("");
	}

	if (len == 0 && chunkstr->chunk->hdr.next >= 0)
	{
		const PipeProtoChunk *next_chunk =
				&saved_chunks[chunkstr->chunk->hdr.next];
		if (next_chunk->hdr.len > 0 && next_chunk->data[0] == '\0')
		{
			chunkstr->p++;
			move_to_next_chunk(chunkstr, saved_chunks);
			return strdup("");
		}
	}

	wlen = pg_strnlen(chunkstr->p, len);

	if (wlen < len)
	{
		// String all contained in this chunk
		out = malloc(wlen + 1);
		memcpy(out, chunkstr->p, wlen + 1); // include the null byte
		chunkstr->p += wlen + 1; // skip to start of next string.
		return out;
	}

	out = malloc(wlen + 1);
	memcpy(out, chunkstr->p, wlen);
	out[wlen] = '\0';
	chunkstr->p += wlen;

	while (chunkstr->p)
	{
		move_to_next_chunk(chunkstr, saved_chunks);
		if (chunkstr->p == NULL)
			break;
		len = chunkstr->chunk->hdr.len - (chunkstr->p - chunkstr->chunk->data);

		wlen = pg_strnlen(chunkstr->p, len);

		/* Write OK, don't forget to account for the trailing 0 */
		if (wlen < len)
		{
			// Remainder of String all contained in this chunk
			out = realloc(out, strlen(out) + wlen + 1);
			strncat(out, chunkstr->p, wlen + 1); // include the null byte

			chunkstr->p += wlen + 1; // skip to start of next string.
			return out;
		}
		else
		{
			int newlen = strlen(out) + wlen;
			out = realloc(out, newlen + 1);
			strncat(out, chunkstr->p, wlen);
			out[newlen] = '\0';

			chunkstr->p += wlen;
		}
	}

	return out;
}

#if defined(HAVE_DECL_CURLOPT_MAIL_FROM) && HAVE_DECL_CURLOPT_MAIL_FROM
/*
 * Support functions for building an alert email.
 */

/*
 *  The message is read a line at a time and the newlines converted
 *  to \r\n.  Unfortunately, RFC 822 states that bare \n and \r are
 *  acceptable in messages and that individually they do not constitute a
 *  line termination.  This requirement cannot be reconciled with storing
 *  messages with Unix line terminations.  RFC 2822 rescues this situation
 *  slightly by prohibiting lone \r and \n in messages.
 */
static void
add_to_message(StringInfo buf, const char *newstr_in)
{
	const char * newstr = newstr_in;
	char * p;
	if (newstr == NULL)
		return;

	/* Drop any leading \n characters:  Not sure what to do with them */
	while (*newstr == '\n')
		newstr++;

	/* Scan for \n, and convert to \r\n */
	while ((p = strchr(newstr,'\n')) != NULL)
	{
		/* Don't exceed 900 chars added to this line, so total line < 1000 */
		if (p - newstr >= 900)
		{
			appendBinaryStringInfo(buf, newstr, 898);
			appendStringInfoString(buf, "\r\n\t");
			newstr += 898;
		}
		else if (p - newstr >=2 && *(p-1) != '\r')
		{
			appendBinaryStringInfo(buf, newstr, p - newstr);
			appendStringInfoString(buf, "\r\n\t");
			newstr = p+1;
		}
		else
		{
			appendBinaryStringInfo(buf, newstr, p - newstr + 1);
			newstr = p+1;
		}
	}
	appendStringInfoString(buf, newstr);
}

static size_t
messagebody_cb(void *ptr, size_t size, size_t nmemb, void *userp)
{
	upload_ctx *ctx = (upload_ctx *) userp;
	size_t len;

	len = size * nmemb;
	if ((size == 0) || (nmemb == 0) || (len < 1))
		return 0;

	if (len > ctx->body.len - ctx->consumed)
		len = ctx->body.len - ctx->consumed;

	memcpy(ptr, ctx->body.data + ctx->consumed, len);
	ctx->consumed += len;

    return len;
}

static void
build_messagebody(StringInfo buf, const GpErrorData *errorData, const char *subject,
		const char *email_priority)
{
	/* Perhaps better to use text/html ? */

	appendStringInfo(buf, "From: %s\r\n", gp_email_from);
	appendStringInfo(buf, "To: %s\r\n", gp_email_to);
	appendStringInfo(buf, "Subject: %s\r\n", subject);
	if (email_priority[0] != '\0' && email_priority[0] != '3') // priority not the default?
		appendStringInfo(buf, "X-Priority: %s", email_priority); // set a priority.  1 == highest priority, 5 lowest

	appendStringInfoString(buf,	"MIME-Version: 1.0\r\n"
			"Content-Type: text/plain;\r\n"
			"  charset=utf-8\r\n"
			"Content-Transfer-Encoding: 8bit\r\n\r\n");

	/* Lines must be < 1000 bytes long for 7bit or 8bit transfer-encoding */
	/* Perhaps use base64 encoding instead?  Or just wrap them? */

	/*
	 * How to identify which system is sending the alert?  Perhaps our
	 * hostname and port is good enough?
	 */
	appendStringInfoString(buf, "Alert from GPDB system");
	{
		char myhostname[255];	/* gethostname usually is limited to 65 chars out, but make this big to be safe */
		myhostname[0] = '\0';

		if (gethostname(myhostname, sizeof(myhostname)) == 0)
			appendStringInfo(buf, " %s on port %d", myhostname, PostPortNumber);
	}
	appendStringInfoString(buf,":\r\n\r\n");
	if (errorData->username != NULL &&  errorData->databasename != NULL &&
		strlen(errorData->username)>0 && strlen(errorData->databasename)>0)
	{
		appendStringInfoString(buf, errorData->username);
		if (errorData->remote_host != NULL && strlen(errorData->remote_host) > 0)
		{
			if (strcmp(errorData->remote_host,"[local]")==0)
				appendStringInfoString(buf, " logged on locally from master node");
			else
				appendStringInfo(buf, " logged on from host %s", errorData->remote_host);
		}
		appendStringInfo(buf, " connected to database %s\r\n",
					errorData->databasename);
	}
	if (errorData->fix_fields.omit_location != 't')
	{
		if (errorData->fix_fields.gp_segment_id != -1)
		{
			appendStringInfo(buf,"Error occurred on segment %d\r\n",
							 errorData->fix_fields.gp_segment_id);
		}
		else
			appendStringInfoString(buf, "Error occurred on master segment\r\n");
	}
	appendStringInfoString(buf, "\r\n");

	appendStringInfo(buf, "%s: ", errorData->error_severity);
	if (errorData->sql_state != NULL && pg_strnlen(errorData->sql_state,5)>4 &&
		strncmp(errorData->sql_state,"XX100",5)!=0 &&
		strncmp(errorData->sql_state,"00000",5)!=0)
	{
		appendStringInfo(buf, "(%s) ", errorData->sql_state);
	}
	appendStringInfoString(buf, errorData->error_message);
	appendStringInfoString(buf, "\r\n");
	appendStringInfoString(buf, "\r\n");

	if (errorData->error_detail != NULL &&strlen(errorData->error_detail) > 0)
	{
		appendStringInfoString(buf, _("DETAIL:  "));
		add_to_message(buf, errorData->error_detail);
		appendStringInfoString(buf, "\r\n");
	}
	if (errorData->error_hint != NULL &&strlen(errorData->error_hint) > 0)
	{
		appendStringInfoString(buf, _("HINT:  "));
		add_to_message(buf, errorData->error_hint);
		appendStringInfoString(buf, "\r\n");
	}
	if (errorData->internal_query != NULL &&strlen(errorData->internal_query) > 0)
	{
		appendStringInfoString(buf, _("QUERY:  "));
		add_to_message(buf, errorData->internal_query);
		appendStringInfoString(buf, "\r\n");
	}
	if (errorData->error_context != NULL && strlen(errorData->error_context) > 0)
	{
		appendStringInfoString(buf, _("CONTEXT:  "));
		add_to_message(buf, errorData->error_context);
		appendStringInfoString(buf, "\r\n");
	}
	if (errorData->fix_fields.omit_location != 't')
	{
		if (errorData->error_filename != NULL && strlen(errorData->error_filename) > 0)
		{
			appendStringInfoString(buf, _("LOCATION:  "));

			if (errorData->error_func_name && strlen(errorData->error_func_name) > 0)
				appendStringInfo(buf, "%s, ", errorData->error_func_name);

			appendStringInfo(buf, "%s:%d\r\n",
							 errorData->error_filename,
							 errorData->fix_fields.error_fileline);
		}
		if (errorData->stacktrace != NULL && strlen(errorData->stacktrace) > 0)
		{
			appendStringInfoString(buf, "STACK TRACE:\r\n\t");
			add_to_message(buf, errorData->stacktrace);
			appendStringInfoString(buf, "\r\n");
		}
	}
	if (errorData->debug_query_string != NULL &&strlen(errorData->debug_query_string) > 0)
	{
		appendStringInfoString(buf, _("STATEMENT:  "));
		add_to_message(buf, errorData->debug_query_string);
		appendStringInfoString(buf, "\r\n");
	}
}

/*
 * Pull out just the e-mail address from a possibly human-readable string
 * like:
 *
 * Full Name <email@example.com>
 */
static char *
extract_email_addr(char *str)
{
	char	   *begin;
	char	   *end;

	begin = strchr(str, '<');
	if (begin != NULL)
	{
		begin++;
		end = strchr(begin, '>');
		if (end != NULL)
		{
			int			len = end - begin;
			char	   *email;

			email = palloc(len + 1);
			memcpy(email, begin, len);
			email[len] = '\0';

			return email;
		}
	}
	return pstrdup(str);
}

static bool
SplitMailString(char *rawstring, char delimiter,
					  List **namelist)
{
	char	   *nextp = rawstring;
	bool		done = false;

	*namelist = NIL;

	while (isspace((unsigned char) *nextp))
		nextp++;				/* skip leading whitespace */

	if (*nextp == '\0')
		return true;			/* allow empty string */

	/* At the top of the loop, we are at start of a new address. */
	do
	{
		char	   *curname;
		char	   *endp;

		curname = nextp;
		while (*nextp && *nextp != delimiter)
			nextp++;
		endp = nextp;
		if (curname == nextp)
			return false;	/* empty unquoted name not allowed */



		while (isspace((unsigned char) *nextp))
			nextp++;			/* skip trailing whitespace */

		if (*nextp == delimiter)
		{
			nextp++;
			while (isspace((unsigned char) *nextp))
				nextp++;		/* skip leading whitespace for next */
			/* we expect another name, so done remains false */
		}
		else if (*nextp == '\0')
			done = true;
		else
			return false;		/* invalid syntax */

		/* Now safe to overwrite separator with a null */
		*endp = '\0';

		/*
		 * Finished isolating current name --- add it to list
		 */
		*namelist = lappend(*namelist, curname);

		/* Loop back if we didn't reach end of string */
	} while (!done);

	return true;
}
#endif /* HAVE_DECL_CURLOPT_MAIL_FROM */
