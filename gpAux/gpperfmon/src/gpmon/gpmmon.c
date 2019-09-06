#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "apr_atomic.h"
#include "apr_env.h"
#include "apr_time.h"
#include "apr_hash.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_queue.h"
#include "apr_pools.h"
#include "apr_tables.h"
#include "apr_thread_proc.h"
#include "apr_thread_mutex.h"
#include "apr_md5.h"
#include "gpmonlib.h"
#include "gpmon/gpmon.h"
#include "gpmon_agg.h"
#include "gpmondb.h"
#include "apr_getopt.h"
#include <event.h>
#include "libpq-fe.h"
#include <time.h>

void update_mmonlog_filename(void);
int gpmmon_quantum(void);
void incremement_tail_bytes(apr_uint64_t);
time_t compute_next_dump_to_file(void);
void populate_smdw_aliases(host_t*);
char* get_ip_for_host(char*, bool*);

#define YES_TEXT "yes"
#define NO_TEXT "no"

#define SET_MAXFD(fd)  if (fd < opt.max_fd) ; else opt.max_fd = fd

mmon_options_t opt = { 0 };

static const apr_uint64_t smon_terminate_safe_factor = 10;
static const apr_uint64_t recv_timeout_factor = 10;

// If smon doesn't receive any request from mmon,
// it simply kill itself to restart.
static apr_uint64_t smon_terminate_timeout = 0;
static apr_uint64_t recv_timeout = 0;

/* gpmmon host */
static struct
{
	int port; /* port of gpsmon */
	apr_pool_t* pool; /* pool */
	char* gphome; /* GPHOME env variable */
	apr_int64_t signature; /* a large random number */

	host_t* hosttab; /* multi-home filtered machines */
	int hosttabsz; /* # machines */

	char* master_data_directory;
	char* standby_master_hostname;
	apr_thread_mutex_t *agg_mutex; /* lock when accessing the agg data */
	agg_t* agg;
	apr_hash_t* fsinfotab; /* This is the persistent fsinfo hash table: key = gpmon_fsinfokey_t, value = mmon_fsinfo_t ptr */

	apr_thread_mutex_t *tailfile_mutex; /* lock when accessing the physical tail files */

	int exit; /* TRUE if we need to exit */
	int reload;
	apr_uint64_t tail_buffer_bytes;
	apr_uint64_t _tail_buffer_bytes;
} ax = { 0 };

void interuptable_sleep(unsigned int seconds);

// lock when accessing the debug log file
apr_thread_mutex_t *logfile_mutex = NULL;


/* Default option values */
int verbose = 0; /* == opt.v */
int very_verbose = 0; /* == opt.V */
int quantum = 15; /* == opt.quantum */
int min_query_time = 60; /* == opt.m */

/* thread handles */
static apr_thread_t* conm_th = NULL;
static apr_thread_t* event_th = NULL;
static apr_thread_t* harvest_th = NULL;
static apr_thread_t* message_th = NULL;
apr_queue_t* message_queue = NULL;

/* signal masks */
sigset_t unblocksig;
sigset_t blocksig;

extern int gpdb_exec_search_for_at_least_one_row(const char* QUERY, PGconn* persistant_conn);

/* Function defs */
static int read_conf_file(char *conffile);
static void gethostlist(void);
static void getconfig(void);
static apr_status_t sendpkt(int sock, const gp_smon_to_mmon_packet_t* pkt);
static apr_status_t recvpkt(int sock, gp_smon_to_mmon_packet_t* pkt, bool loop_until_all_recv);


#define MMON_LOG_FILENAME_SIZE (MAXPATHLEN+1)
char mmon_log_filename[MMON_LOG_FILENAME_SIZE];
void update_mmonlog_filename()
{
		time_t stamp = time(NULL);
		struct tm* tm = gmtime(&stamp);
		snprintf(
			mmon_log_filename,
	 		MMON_LOG_FILENAME_SIZE,
			"%s/gpmmon.%d.%02d.%02d_%02d%02d%02d.log",
			opt.log_dir,
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);
}

/** Gets quantum */
int gpmmon_quantum(void)
{
	return opt.quantum;
}

/* prints usage and exit */
static void usage(const char* msg)
{
	fprintf(stderr, "\nusage: %s -D <configuration file> [options]\n\n",
			opt.pname);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\t-?\t\t: print this help screen\n");
	fprintf(stderr, "\t-V\t\t: print packet version information\n\n");
	if (msg)
		fprintf(stderr, "%s\n\n", msg);

	exit(msg ? 1 : 0);
}

void incremement_tail_bytes(apr_uint64_t bytes)
{
	ax.tail_buffer_bytes += bytes;
}

/* Cleanup function called on exit. */
static void cleanup()
{
	/* wait for threads, close ports, apr cleanup, etc. */
	apr_status_t tstatus;
	int i;

	ax.exit = 1;

	if (event_th)
		apr_thread_join(&tstatus, event_th);
	if (conm_th)
		apr_thread_join(&tstatus, conm_th);
	if (harvest_th)
		apr_thread_join(&tstatus, harvest_th);
	if (message_th)
		apr_thread_join(&tstatus, message_th);

	for (i = 0; i < ax.hosttabsz; i++)
	{
		host_t* h = &ax.hosttab[i];
		if (h)
		{
			apr_thread_mutex_lock(h->mutex);
			if (h->sock)
				close(h->sock);
			h->sock = 0;
			apr_thread_mutex_unlock(h->mutex);
			h = NULL;
		}
	}

	if (ax.pool)
		apr_pool_destroy(ax.pool);
}


/**
 * Signal handlers
 */
static void SIGHUP_handler(int sig)
{
	/* Flag to reload configuration values from conf file */
	ax.reload = 1;
}

static void SIGUSR2_handler(int sig)
{
	ax.exit = 1;
}


/** ------------------------------------------------------------
 After we sent a 'D'ump command, gpsmon will send us packets thru
 the TCP connection. This function gets called whenever a packet
 arrives.
 */
static void recv_from_gx(SOCKET sock, short event, void* arg)
{
	host_t* h = arg;
	int e;
	gp_smon_to_mmon_packet_t pktbuf;
	gp_smon_to_mmon_packet_t* pkt = 0;
	TR2(("recv_from_gx sock %d host %s port %d\n", sock, h->hostname, ax.port));

	if (event & EV_TIMEOUT)
	{
		// no response from gpsmon for a long time
		// retry connecting
		TR1(("Connection to %s timeout\n",h->hostname));
		h->eflag = 1;
	} 
	else if (event & EV_READ) 
	{
		// reset timer of timeout event
		struct timeval tv;
		tv.tv_sec = 10 * gpmmon_quantum();
		tv.tv_usec = 0;
		if (event_add(h->event, &tv))
		{
			gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "event_add failed");
		}
	}
	else
	{
		// bad event
		return;
	}

	apr_thread_mutex_lock(h->mutex);

	if (h->event)
	{
		if (!h->eflag)
		{
			e = recvpkt(sock, &pktbuf, true);
			if (e == APR_FROM_OS_ERROR(EINTR)) {
				TR1(("at %s: connection dropped by host %s port %d [set eflag]\n", FLINE, h->hostname, ax.port));
				h->eflag = 1;
			} else if( e != 0 ) {
				gpmon_warningx(FLINE, e, "cannot get packet from host %s port %d", h->hostname, ax.port);
				h->eflag = 1;
			}
		}
		if (h->eflag)
		{
			event_del(h->event);
			h->event = 0;
		}
		else
		{
			pkt = &pktbuf;
			TR2(("received packet %d from %s:%d\n", pkt->header.pkttype, h->hostname, ax.port));
		}
	}

	apr_thread_mutex_unlock(h->mutex);
	if (pkt)
	{
		apr_thread_mutex_lock(ax.agg_mutex);
		e = agg_put(ax.agg, pkt);
		apr_thread_mutex_unlock(ax.agg_mutex);
		if (e)
		{
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, e, "agg_put failed");
		}
	}
}

/** ------------------------------------------------------------
 This is where we setup events for TCP connections to gpsmon and
 service those TCP connections, i.e. receiving qexec/qlog packets
 following a dump command. We send the dump command in gpmmon_main().

 Event thread:
 Forever:
 serve events for 2 sec
 check if we need to set up new event

 On ready:
 if error:
 close(event->ev_fd);
 if readok:
 read a packet
 */
static void* event_main(apr_thread_t* thread_, void* arg_)
{
	struct timeval tv;
	host_t* tab = ax.hosttab;
	const int tabsz = ax.hosttabsz;

	if (!event_init())
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_init failed");
	}
	while (!ax.exit)
	{
		int i;
		int count_valid = 0;

		/* setup new events */
		TR2(("event_main: scan hosttab\n"));

		for (i = 0; i < tabsz; i++)
		{
			host_t* h = &tab[i];
			SOCKET close_sock = 0;
			apr_thread_mutex_lock(h->mutex);
			if (h->sock)
			{
				if (h->eflag)
				{
					if (h->event)
					{
						event_del(h->event);
						h->event = 0;
					}
					close_sock = h->sock;
					h->sock = 0;
				}
				else
				{
					count_valid++;
					if (!h->event)
					{
						/* set up the event */
						h->event = &h->_event;
						struct timeval tv;
						tv.tv_sec = recv_timeout_factor * gpmmon_quantum();
						tv.tv_usec = 0;
						event_set(h->event, h->sock, EV_READ | EV_PERSIST | EV_TIMEOUT,
								recv_from_gx, h);
						if (event_add(h->event, &tv))
						{
							interuptable_sleep(30); // sleep to prevent loop of forking process and failing
							gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_add failed");
						}
					}
				}
			}
			apr_thread_mutex_unlock(h->mutex);
			if (close_sock)
			{
				TR2(("closing socket %d\n", close_sock));
				closesocket(close_sock);
			}
		}

		if (count_valid == 0)
		{
			TR2(("no valid connection, sleep 1\n"));
			apr_sleep(apr_time_from_sec(1));
			continue;
		}

		/* serve events for 3 second */
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		TR2(("event_loopexit\n"));
		if (-1 == event_loopexit(&tv))
		{
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_loopexit failed");
		}
		TR2(("event_dispatch\n"));
		if (-1 == event_dispatch() && ETIME != errno)
		{
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_dispatch failed");
		}
		if (ax.reload == 1)
		{
			TR0(("sighup received, reloading conf files\n"));

			ax.reload = 0;
			/* Check if perfmon is enabled.  If not, exit */
			if (!gpdb_gpperfmon_enabled())
			{
				TR0(("Monitoring has been disabled, exiting...\n"));
				ax.exit = 1;
				continue;
			}
			else
				read_conf_file(opt.conf_file);

			TR0(("finished reloading conf files\n"));
		}
	}
	return 0;
}


static apr_status_t conm_connect(SOCKET* retsock, apr_int32_t* retpid, const char* ipstr, int port, bool ipv6)
{
	struct sockaddr_in sa;
	struct sockaddr_in6 sa6;
	SOCKET sock = -1;
	gp_smon_to_mmon_packet_t pkt;
	int e = 0;
	unsigned short family = 0;
	struct sockaddr * sockaddrlive = NULL;
	size_t length = 0;

	memset(&pkt, 0, sizeof(gp_smon_to_mmon_packet_t));


	if (ipv6)
	{
		family = AF_INET6;
		memset(&sa6, 0, sizeof(sa6));
		sa6.sin6_family = AF_INET6;
		sa6.sin6_port = htons(port);
		inet_pton(AF_INET6, ipstr, &(sa6.sin6_addr));
		sockaddrlive = (struct sockaddr *)&sa6;
		length = sizeof(sa6);
	}
	else
	{
		family = AF_INET;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(ipstr);
		sa.sin_port = htons(port);
		sockaddrlive = (struct sockaddr *)&sa;
		length = sizeof(sa);
	}

	if (-1 == (sock = socket(family, SOCK_STREAM, 0)))
	{
		gpmon_warningx(FLINE, 0, "error %d from socket system call", sock);
		goto bail;
	}

	SET_MAXFD(sock);

	if (-1 == connect(sock, sockaddrlive, length))
	{
		e = APR_FROM_OS_ERROR(errno);
		gpmon_warningx(FLINE, e, "connect system call failed");
		goto bail;
	}

	struct timeval tv;
	tv.tv_sec = recv_timeout;
	tv.tv_usec = 0;
	if (setsockopt(sock,
				   SOL_SOCKET,
				   SO_RCVTIMEO,
				   (char*)&tv,
				   sizeof(tv)) < 0)
	{
		e = APR_FROM_OS_ERROR(errno);
		gpmon_fatalx(FLINE, e, "failed to set SO_RCVTIMEO");
		goto bail;
	}

	gp_smon_to_mmon_set_header(&pkt, GPMON_PKTTYPE_HELLO);
	pkt.u.hello.signature = ax.signature;
	if (0 != (e = sendpkt(sock, &pkt)))
	{
		gpmon_warningx(FLINE, 0, "error %d from sendpkt system call", e);
		goto bail;
	}

	if (0 != (e = recvpkt(sock, &pkt, false))) {
		gpmon_warningx(FLINE, 0, "error %d from recvpkt system call", e);
		goto bail;
	}

	if (0 != (e = gpmon_ntohpkt(pkt.header.magic, pkt.header.version, pkt.header.pkttype))) {
		gpmon_warningx(FLINE, 0, "error %d from recvpkt gpmon_ntohpkt", e);
		goto bail;
	}

	if (pkt.header.pkttype != GPMON_PKTTYPE_HELLO)
	{
		gpmon_warning(FLINE, "invalid packet type");
		e = APR_EINVAL;
		goto bail;
	}

	/* on successful connect, save pid of gpsmon for killing it in case of hang */
	*retpid = pkt.u.hello.pid;

	*retsock = sock;
	return 0;

	bail:
	if (sock >= 0) closesocket(sock);
	return e;
}

/** ------------------------------------------------------------
 Connection management thread:
 Forever:
 sleep 5
 for any broken connection:
 close socket
 for any broken connection:
 start gpsmon using ssh
 for any broken connection:
 try connect
 */
static void* conm_main(apr_thread_t* thread_, void* arg_)
{
	host_t* tab = ax.hosttab;
	const int tabsz = ax.hosttabsz;
	int i, j, e;
	int* broken;
	int count_broken = 0;
	unsigned int loop;

	broken = malloc(sizeof(*broken) * tabsz);
	CHECKMEM(broken);
	memset(broken, 0, sizeof(*broken) * tabsz);

	for (loop = 0; !ax.exit; loop++)
	{
		apr_sleep(apr_time_from_sec(1));

		if (1 == (loop % 2))
		{
			// find broken connections
			count_broken = 0;
			for (i = 0; i < tabsz; i++)
			{
				host_t* h = &tab[i];
				apr_thread_mutex_lock(h->mutex);
				if (h->sock == 0)
					broken[count_broken++] = i;
				apr_thread_mutex_unlock(h->mutex);
			}
		}

		if (CONM_LOOP_LAUNCH_FRAME == (loop % CONM_INTERVAL))
		{

			// for any broken connection, start gpsmon
			for (i = 0; i < count_broken;)
			{
				FILE* fp[BATCH];

				// use these strings to formulate log location for smon without allocating any dynamic memory
				const char* empty_string = "";
				const char* gpperfmon_string = "/gpperfmon";
				const char* ptr_smon_log_location;
				const char* ptr_smon_log_location_suffix;

				const int line_size = 2048;
				char line[line_size];
				memset(fp, 0, sizeof(fp));
				for (j = 0; j < 8 && i < count_broken; j++, i++)
				{
					host_t* h = &tab[broken[i]];
					char* active_hostname;

					advance_connection_hostname(h); // if we have to connect many times to same host try a new hostname for same host

					active_hostname = get_connection_hostname(h);

					// smon will log to gpperfmon directory on master, specified location on smon, or one of the gpperfmon subdir of one of the data directories as default
					if (h->is_master)
					{
						ptr_smon_log_location = opt.log_dir;
						ptr_smon_log_location_suffix = empty_string;
					}
					else if (opt.smon_log_dir)
					{
						ptr_smon_log_location = opt.smon_log_dir;
						ptr_smon_log_location_suffix = empty_string;
					}
					else
					{
						ptr_smon_log_location = h->data_dir;
						ptr_smon_log_location_suffix = gpperfmon_string;
					}

					const int kill_cmd_size = 1024;
					char kill_gpsmon[kill_cmd_size];
					memset(kill_gpsmon, 0, kill_cmd_size);
					if (h->connect_timeout == GPSMON_TIMEOUT_RESTART && h->pid > 0)
					{
						snprintf(kill_gpsmon, kill_cmd_size, "kill -9 %d;", h->pid);
						apr_thread_mutex_lock(h->mutex);
						h->pid = 0; /* don't try kill gpsmon repeatly */
						h->connect_timeout = GPSMON_TIMEOUT_NONE; /* try reconnect immediately */
						apr_thread_mutex_unlock(h->mutex);
					}

					if (h->smon_bin_location) { //if this if filled, then use it as the directory for smon istead of the default
						snprintf(line, line_size, "ssh -v -o 'BatchMode yes' -o 'StrictHostKeyChecking no'"
								" %s '%s echo -e \"%" APR_INT64_T_FMT "\\n\\n\" | %s -m %" FMT64 " -t %" FMT64 " -l %s%s -v %d %d' 2>&1",
								active_hostname, kill_gpsmon, ax.signature, h->smon_bin_location, opt.max_log_size, smon_terminate_timeout, ptr_smon_log_location, ptr_smon_log_location_suffix, opt.v, ax.port);
					} else {
						snprintf(line, line_size, "ssh -v -o 'BatchMode yes' -o 'StrictHostKeyChecking no'"
								" %s '%s echo -e \"%" APR_INT64_T_FMT "\\n\\n\" | %s/bin/gpsmon -m %" FMT64 " -t %" FMT64 " -l %s%s -v %d %d' 2>&1",
								active_hostname, kill_gpsmon, ax.signature, ax.gphome, opt.max_log_size, smon_terminate_timeout, ptr_smon_log_location, ptr_smon_log_location_suffix, opt.v, ax.port);

					}

					if (h->ever_connected)
					{
						TR0(("Connection to %s lost.  Restarting gpsmon.\n", active_hostname));
					}
					else
					{
						TR0(("Making initial connection to %s\n", active_hostname));
					}
					h->ever_connected = 1;

					TR1(("%s\n", line));
					fp[j] = popen(line, "r");
					if (fp[j])
					{
						SET_MAXFD(fileno(fp[j]));
					}
					else
					{
						TR0(("Call popen failed due to %s\n", strerror(errno)));
					}
				}
				for (j = 0; j < 8; j++)
				{
					if (fp[j])
					{
						TR1_FILE(("Debugging logs of ssh", fp[j]));
						pclose(fp[j]);
					}
				}
			}
		}

		// for any broken/timeout connection, try connect
		bool try_connect_normal = (CONM_LOOP_BROKEN_FRAME == (loop % CONM_INTERVAL));
		bool try_connect_hang = (CONM_LOOP_HANG_FRAME == (loop % CONM_INTERVAL));
		if (try_connect_normal || try_connect_hang)
		{
			for (i = 0; i < count_broken; i++)
			{
				host_t* h = &tab[broken[i]];
				if(GPSMON_TIMEOUT_DETECTED == h->connect_timeout)
				{
					/* In next loop will begin to restart and reconnect gpsmon */
					apr_thread_mutex_lock(h->mutex);
					h->connect_timeout = GPSMON_TIMEOUT_RESTART;
					apr_thread_mutex_unlock(h->mutex);
					continue;
				}
				if(GPSMON_TIMEOUT_NONE == h->connect_timeout && !try_connect_normal)
				{
					continue;
				}
				if(GPSMON_TIMEOUT_RESTART == h->connect_timeout && !try_connect_hang)
				{
					continue;
				}
				SOCKET sock = 0;
				apr_int32_t gpsmon_pid = 0;
				char* active_hostname = get_connection_hostname(h);
				char* active_ip = get_connection_ip(h);
				bool ipv6 = get_connection_ipv6_status(h);

				TR1(("connecting to %s (%s:%d)\n", active_hostname, active_ip, ax.port));
				if (0 != (e = conm_connect(&sock, &gpsmon_pid, active_ip, ax.port, ipv6)))
				{
					gpmon_warningx(FLINE, 0, "cannot connect to %s (%s:%d)",
							active_hostname, active_ip, ax.port);
					if (APR_ETIMEDOUT == e) /* connection timeout */
					{
						if (GPSMON_TIMEOUT_RESTART == h->connect_timeout)
						{
							gpmon_warning(FLINE, "Failed to reconnect gpsmon on %s, maybe network isolation or other process occupied the port", active_hostname);
						}
						else if (GPSMON_TIMEOUT_NONE == h->connect_timeout)
						{
							/* Mark the host as timeout, push it behind normal host */
							apr_thread_mutex_lock(h->mutex);
							h->connect_timeout = GPSMON_TIMEOUT_DETECTED;
							apr_thread_mutex_unlock(h->mutex);
						}
					}
					else
					{
						apr_thread_mutex_lock(h->mutex);
						h->connect_timeout = GPSMON_TIMEOUT_NONE;
						apr_thread_mutex_unlock(h->mutex);
					}
					continue;
				}
				/* connected - set it to valid */
				TR1(("connected to %s (%s:%d), pid %d\n", active_hostname, active_ip, ax.port, gpsmon_pid));
				apr_thread_mutex_lock(h->mutex);
				h->sock = sock;
				h->event = 0;
				h->eflag = 0;
				h->connect_timeout = GPSMON_TIMEOUT_NONE;
				h->pid = gpsmon_pid;
				apr_thread_mutex_unlock(h->mutex);
			}
		}
	}
	return 0;
}

/* seperate thread for harvest */
static void* harvest_main(apr_thread_t* thread_, void* arg_)
{
	unsigned int loop;
	apr_status_t status;
	unsigned int consecutive_failures = 0;
	unsigned int partition_check_interval = 3600 * 6; // check for new partitions every 6 hours

	gpdb_check_partitions(&opt);

	for (loop = 1; !ax.exit; loop++)
	{
		apr_sleep(apr_time_from_sec(1));

		if (0 == (loop % opt.harvest_interval))
		{
			int e;
			/*
				PROCESS:
				1) WITH TAIL MUTEX: rename tail files to stage files
				2) WITH TAIL MUTEX: create new tail files
				3) Append data from stage files into _tail files
				4) load data from _tail files into system history
				5) delete _tail files after successful data load

				NOTES:
				1) mutex is held only over rename and creation of new tail files
				   The reason is this is a fast operation and both the main thread doing dumps and the harvest
				   thread uses the tail files

				2) tail files are renamed/moved because this operation is safe to be done while the clients are
				   reading the files.

				3) The stage files are written over every harvest cycle, so the idea is no client
				   will still be reading the tail files for an entire harvest cycle.  (this is not perfect logic but ok)
			*/

			apr_pool_t* pool; /* create this pool so we can destroy it each loop */


			if (0 != (e = apr_pool_create_alloc(&pool, ax.pool)))
			{
				interuptable_sleep(30); // sleep to prevent loop of forking process and failing
				gpmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
				return (void*)1;
			}

			/* LOCK TAIL MUTEX ****************************************/
			apr_thread_mutex_lock(ax.tailfile_mutex);

			ax._tail_buffer_bytes += ax.tail_buffer_bytes;
			ax.tail_buffer_bytes = 0;

			status = gpdb_rename_tail_files(pool);

			status = gpdb_truncate_tail_files(pool);

			apr_thread_mutex_unlock(ax.tailfile_mutex);
			/* UNLOCK TAIL MUTEX ****************************************/

			status = gpdb_copy_stage_to_harvest_files(pool);

			if (status == APR_SUCCESS)
			{
				status = gpdb_harvest();
			}

			if (status != APR_SUCCESS)
			{
				gpmon_warningx(FLINE, 0, "harvest failure: accumulated tail file size is %lu bytes", ax._tail_buffer_bytes);
				consecutive_failures++;
			}

			if (status == APR_SUCCESS ||
					consecutive_failures > 100 ||
					(ax._tail_buffer_bytes > opt.tail_buffer_max))
			{
				/*
					delete the data in the _tail file because it has been inserted successfully into history
					we also delete the data without loading if it loading failed more than 100 times as a defensive measure for corrupted data in the file
					better to lose some perf data and self fix than calling support to have them manually find the corrupted perf data and clear it themselves
					we also delete the data if the size of the data is greater than our max data size
				*/
				status = gpdb_empty_harvest_files(pool);

				if (status != APR_SUCCESS)
				{
					gpmon_warningx(FLINE, 0, "error trying to clear harvest files");
				}
				consecutive_failures = 0;
				ax._tail_buffer_bytes = 0;
			}
			gpdb_import_alert_log(pool);
			apr_pool_destroy(pool); /*destroy the pool since we are done with it*/
		}
		if (0 == (loop % partition_check_interval))
		{
			gpdb_check_partitions(&opt);
		}
	}

	return APR_SUCCESS;
}
/* Separate thread for message sending */
static void* message_main(apr_thread_t* thread_, void* arg_)
{
	apr_queue_t *queue = arg_;
	void *query = NULL;
	apr_status_t status;

	TR2(("In message_main: error_disk_space_percentage = %d, warning_disk_space_percentage = %d, disk_space_interval = %d, max_disk_space_messages_per_interval = %d\n",
		 opt.error_disk_space_percentage, opt.warning_disk_space_percentage, (int) opt.disk_space_interval, opt.max_disk_space_messages_per_interval));
	while (1)
	{
		query = NULL;
		status = apr_queue_pop(queue, &query);
		if (status == APR_EINTR)
		{ //the blocking operation was interrupted (try again)
			continue;
		}
		else if (status != APR_SUCCESS)
		{
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(
				FLINE, status, "message_main ERROR: apr_queue_pop failed: returned %d", status);
			return (void*)1;
		}
		else if (NULL == query)
		{
			TR0(("message_main ERROR: apr_queue_pop returned NULL\n"));
		}
		else
		{ // send the message
			if (!gpdb_exec_search_for_at_least_one_row((const char *)query, NULL))
			{
				TR0(("message_main ERROR: query %s failed. Cannot send message\n", (char *) query));
			}
			free(query);
		}

	}
	return APR_SUCCESS;
}

time_t compute_next_dump_to_file()
{
	time_t current_time = time(NULL);
	return (current_time - (current_time % opt.quantum) + opt.quantum);
}

static void gpmmon_main(void)
{
	int e;
	apr_status_t retCode;
	apr_threadattr_t* ta;
	time_t this_cycle_ts = 0;
	/* log check is not exact. do it every X loops */
	int ticks_since_last_log_check = 0;
	const unsigned int log_check_interval = 60;

	const int safety_ticks = 2 * opt.quantum;
	unsigned int dump_request_time_allowance = opt.quantum / 2;

	/* DUMP TO FILE */
	time_t next_dump_to_file_ts;
	int dump_to_file_safety_ticks = safety_ticks;

	/* SEND MESSAGE  */
	time_t next_send_msg_ts;
	int send_msg_safety_ticks = safety_ticks;

	/* init timestamps */
	next_dump_to_file_ts = compute_next_dump_to_file();
	next_send_msg_ts = next_dump_to_file_ts - dump_request_time_allowance;

	/* TODO: MPP-3974 might have actually been caused by the spin lock problem... investigate */
	setenv("EVENT_NOKQUEUE", "1", 1);
	setenv("EVENT_NOEVPORT", "1", 1);
	setenv("EVENT_SHOW_METHOD", "1", 1);
	/* MPP-3974.  Hawks systems don't like devpoll */
	setenv("EVENT_NODEVPOLL", "1", 1);

	if (0 != (e = apr_pool_create_alloc(&ax.pool, 0)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
	}

	if (0 != (e = apr_env_get(&ax.gphome, "GPHOME", ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "GPHOME environment variable not set");
	}

	/* Create mutexes */
	if (0 != (e = apr_thread_mutex_create(&ax.agg_mutex, APR_THREAD_MUTEX_UNNESTED, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "Resource Error: Failed to create agg_mutex");
	}

	if (0 != (e = apr_thread_mutex_create(&ax.tailfile_mutex, APR_THREAD_MUTEX_UNNESTED, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "Resource Error: Failed to create tailfile_mutex");
	}

	if (0 != (e = apr_thread_mutex_create(&logfile_mutex, APR_THREAD_MUTEX_UNNESTED, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "Resource Error: Failed to create logfile_mutex");
	}

	if (0 != (e = apr_threadattr_create(&ta, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_threadattr_create failed");
	}

	if (0 != (e = apr_threadattr_detach_set(ta, 1)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_threadattr_detach_set failed");
	}

	// generate signature
	// this used to use apr_generate_random_bytes but that hangs on entropy in the system being available
	// this is not used for security or protecting against attacks, so a simpler random number will do
	srand(time(NULL));
	ax.signature = rand();
	ax.signature <<= 32;
	ax.signature += rand();
	if (ax.signature < 0)
		ax.signature = ~ax.signature;

	/* make sure to update the partition tables once before starting all the threads */
	retCode = gpdb_check_partitions(&opt);
	if (retCode != APR_SUCCESS)
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "failed while initializing historical tables with current month partitions");
	}

	/* get hostlist */
	gethostlist();

	/* create the persistent fsinfo hash table */
	ax.fsinfotab = apr_hash_make(ax.pool);
	if (!ax.fsinfotab)
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_hash_make for fsinfo hash table failed");
	}

	/* create the agg */
	if (0 != (e = agg_create(&ax.agg, 1, ax.pool, ax.fsinfotab)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "agg_create failed");
	}

	/* spawn conm thread */
	if (0 != (e = apr_thread_create(&conm_th, ta, conm_main, 0, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_thread_create failed");
	}

	/* spawn event thread */
	if (0 != (e = apr_thread_create(&event_th, ta, event_main, 0, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_thread_create failed");
	}

	/* spawn harvest thread */
	if (0 != (e = apr_thread_create(&harvest_th, ta, harvest_main, 0, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_thread_create failed");
	}

	/* Create message queue */
	if (0 != (e = apr_queue_create(&message_queue, MAX_MESSAGES_PER_INTERVAL, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_queue_create failed");
	}

	/* spawn disk space message thread */
	if (0 != (e = apr_thread_create(&message_th, ta, message_main, message_queue, ax.pool)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_thread_create failed");
	}

	/* main loop */
	while (!ax.exit)
	{
		apr_sleep(apr_time_from_sec(1));

		this_cycle_ts = time(NULL);
		send_msg_safety_ticks--;
		dump_to_file_safety_ticks--;
		ticks_since_last_log_check++;

		/* SEND MESSAGE */
		if ((this_cycle_ts >= next_send_msg_ts) || (send_msg_safety_ticks < 1))
		{
			int i;
			for (i = 0; i < ax.hosttabsz; i++)
			{
				host_t* h = &ax.hosttab[i];
				apr_thread_mutex_lock(h->mutex);
				/* only send to entries with a socket, handling events, and no error */
				TR1(("send dump %d eflag %d\n", h->sock, h->eflag));
				if (h->sock && h->event && !h->eflag)
				{
					if (1 != send(h->sock, "D", 1, 0))
					{
						h->eflag = 1;
						TR1(("at %s: cannot send 'D'ump command [set eflag]\n", FLINE));
					}
				}
				apr_thread_mutex_unlock(h->mutex);
			}

			send_msg_safety_ticks = safety_ticks;
			next_send_msg_ts = this_cycle_ts + opt.quantum;
		}

		/* DUMP TO FILE */
		if ((this_cycle_ts >= next_dump_to_file_ts) || (dump_to_file_safety_ticks < 1))
		{
			agg_t* newagg = 0;
			agg_t* oldagg = 0;

			/* mutex lock the aggregate data while we dump and dupe it */
			apr_thread_mutex_lock(ax.agg_mutex);

			/* mutex tail files during dump call */
			apr_thread_mutex_lock(ax.tailfile_mutex);

			/* dump the current aggregates */
			if (0 != (e = agg_dump(ax.agg)))
			{
				gpmon_warningx(FLINE, e, "unable to finish aggregation");
			}

			apr_thread_mutex_unlock(ax.tailfile_mutex);

			/* make a new one, copy only recently updated entries */
			if (0 != (e = agg_dup(&newagg, ax.agg, ax.pool, ax.fsinfotab)))
			{
				interuptable_sleep(30); // sleep to prevent loop of forking process and failing
				gpmon_fatalx(FLINE, e, "agg_dup failed");
			}
			oldagg = ax.agg;
			ax.agg = newagg;

			apr_thread_mutex_unlock(ax.agg_mutex);
			/* destroy the old agg */
			agg_destroy(oldagg);

			next_dump_to_file_ts = compute_next_dump_to_file();
			next_send_msg_ts = next_dump_to_file_ts - dump_request_time_allowance;
			dump_to_file_safety_ticks = safety_ticks;
		}

		if (!opt.console && (ticks_since_last_log_check > log_check_interval))
		{
			apr_finfo_t finfo;
			//it is ok to use the parent pool here b/c it is not for used for allocation in apr_stat
			if (0 == apr_stat(&finfo, mmon_log_filename, APR_FINFO_SIZE, ax.pool))
			{
				if (opt.max_log_size != 0 && finfo.size > opt.max_log_size)
				{
					update_mmonlog_filename();
					apr_thread_mutex_lock(logfile_mutex);
					freopen(mmon_log_filename, "w", stdout);
					apr_thread_mutex_unlock(logfile_mutex);
				}
			}
			ticks_since_last_log_check = 0;
		}
	}
}

static void print_version(void)
{
	fprintf(stdout, GPMMON_PACKET_VERSION_STRING);
	exit(0);
}

static int read_conf_file(char *conffile)
{
	char buffer[1024] = { 0 };
	char *p = NULL;
	FILE *fp = fopen(conffile, "r");
	int section = 0, section_found = 0;

	opt.quantum = quantum;
	opt.min_query_time = min_query_time;
	opt.harvest_interval = 120;
	opt.max_log_size = 0;
	opt.log_dir = strdup(DEFAULT_GPMMON_LOGDIR);
	opt.max_disk_space_messages_per_interval = MAX_MESSAGES_PER_INTERVAL;
	opt.disk_space_interval = (60*MINIMUM_MESSAGE_INTERVAL);
	opt.partition_age = 0;

	if (!fp)
	{
		fprintf(stderr, "Performance Monitor - Error: Failed to open configuration file.  Using defaults.");
		return 0;
	}

	while (NULL != fgets(buffer, 1024, fp))
	{
		/* remove new line */
		p = gpmon_trim(buffer);

		if (p[0] == '[') /* Start of section */
		{
			if (apr_strnatcasecmp(p, "[gpmmon]") == 0)
				section = section_found = 1;
			else
				section = 0;
		}
		else /* config param */
		{
			char *pName = NULL, *pVal = NULL, *pTemp = NULL;
			/* is it a comment? */
			pTemp = p;
			while (pTemp && *pTemp)
			{
				if (*pTemp == '#')
				{
					*pTemp = '\0';
					break;
				}
				pTemp++;
			}

			pName = strtok(buffer, "=");
			pVal = strtok(NULL, "=");

			if (section == 0 || buffer[0] == 0 || pName == NULL || pVal == NULL)
				continue;

			pName = gpmon_trim(pName);
			pVal = gpmon_trim(pVal);

			if (apr_strnatcasecmp(pName, "quantum") == 0)
			{
				opt.quantum = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "harvest_interval") == 0)
			{
				opt.harvest_interval = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "min_query_time") == 0)
			{
				opt.min_query_time = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "verbose") == 0)
			{
				opt.v = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "qamode") == 0)
			{
				/* this will allow QA to make config settings that are normally illegal */
				opt.qamode = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "console") == 0)
			{
				/* this will disable logging to log files */
				opt.console = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "log_location") == 0)
			{
				/* can't use APR here as the pool is just temporary */
				if (opt.log_dir)
					free(opt.log_dir);

				opt.log_dir = strdup(pVal);
			}
			else if (apr_strnatcasecmp(pName, "smon_log_location") == 0)
			{
				if (opt.smon_log_dir)
					free(opt.smon_log_dir);

				opt.smon_log_dir = strdup(pVal);
			}
			else if (apr_strnatcasecmp(pName, "hadoop_hostfile") == 0)
			{
				if (opt.smon_hadoop_swonly_clusterfile)
					free(opt.smon_hadoop_swonly_clusterfile);
				opt.smon_hadoop_swonly_clusterfile = strdup(pVal);
			}
			else if (apr_strnatcasecmp(pName, "hadoop_logdir") == 0)
			{
				if (opt.smon_hadoop_swonly_logdir)
					free(opt.smon_hadoop_swonly_logdir);
				opt.smon_hadoop_swonly_logdir = strdup(pVal);
			}
			else if (apr_strnatcasecmp(pName, "hadoop_smon_path") == 0)
			{
				if (opt.smon_hadoop_swonly_binfile)
					free(opt.smon_hadoop_swonly_binfile);
				opt.smon_hadoop_swonly_binfile = strdup(pVal);
			}
			else if (apr_strnatcasecmp(pName, "smdw_aliases") == 0)
			{
				opt.smdw_aliases = strdup(pVal);
			}
			else if (apr_strnatcasecmp(pName, "tail_buffer_max") == 0)
			{
				opt.tail_buffer_max = apr_atoi64(pVal);
			}
			else if (apr_strnatcasecmp(pName, "max_log_size") == 0)
			{
				opt.max_log_size = apr_atoi64(pVal);
			}
			else if (apr_strnatcasecmp(pName, "warning_disk_space_percentage") == 0)
			{
				opt.warning_disk_space_percentage = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "error_disk_space_percentage") == 0)
			{
				opt.error_disk_space_percentage = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "disk_space_interval") == 0)
			{
				opt.disk_space_interval = (time_t) (atoi(pVal)*60); //interval in seconds but set in minutes, so multiply
			}
			else if (apr_strnatcasecmp(pName, "max_disk_space_messages_per_interval") == 0)
			{
				opt.max_disk_space_messages_per_interval = atoi(pVal);
			}
			else if (apr_strnatcasecmp(pName, "partition_age") == 0)
			{
				opt.partition_age = atoi(pVal);
			}
			else
			{
				fprintf(stderr, "Unknown option %s\n", pName == NULL ? "(NULL)"
						: pName);
			}
		}
	}

	smon_terminate_timeout = opt.quantum * smon_terminate_safe_factor;
	recv_timeout = opt.quantum * recv_timeout_factor;

	/* check for valid entries */
	if (!section_found)
		fprintf(stderr, "Performance Monitor - Failed to find [gpmmon] section in the "
				"configuration file.  Using default values\n");

	if (opt.quantum != 10 && opt.quantum != 15 && opt.quantum != 20 && opt.quantum != 30 && opt.quantum != 60)
	{
		fprintf(stderr, "Performance Monitor - quantum value must be be either 10, 15, 20, 30 or 60.  Using "
				"default value of 15\n");
		opt.quantum = 15;
	}

	if (opt.min_query_time < 0)
		opt.min_query_time = 0;

	if (opt.log_dir == NULL)
	{
		char log_dir[MAXPATHLEN + 1] = { 0 };
		snprintf(log_dir, MAXPATHLEN, "%s/%s", ax.master_data_directory,
				"gpperfmon/logs/");
		opt.log_dir = strdup(log_dir);
	}

	if (opt.harvest_interval < 30 && !opt.qamode)
	{
		fprintf(stderr, "Performance Monitor - harvest_interval must be greater than 30.  Using "
				"default value 120\n");
		opt.harvest_interval = 120;
	}

	if (opt.warning_disk_space_percentage < 0 || opt.warning_disk_space_percentage >= 100)
	{
		fprintf(stderr, "Performance Monitor - warning_disk_space_percentage must be between 1 and 100.  Disabling.\n");
		opt.warning_disk_space_percentage = 0;
	}


	if (opt.error_disk_space_percentage < 0 || opt.error_disk_space_percentage >= 100)
	{
		fprintf(stderr, "Performance Monitor - error_disk_space_percentage must be between 1 and 100.  Disabling.\n");
		opt.error_disk_space_percentage = 0;
	}

	if (opt.error_disk_space_percentage < opt.warning_disk_space_percentage) {
		fprintf(stderr, "Performance Monitor - error_disk_space_percentage less than warning_disk_space_percentage, so setting to warning_disk_space_percentage.\n");
		opt.error_disk_space_percentage = opt.warning_disk_space_percentage;
	}

	if (opt.max_disk_space_messages_per_interval > MAX_MESSAGES_PER_INTERVAL) {
		fprintf(stderr, "Performance Monitor - max_disk_space_messages_per_interval must be not be greater than %d.  Setting to %d.\n",MAX_MESSAGES_PER_INTERVAL, MAX_MESSAGES_PER_INTERVAL );
		opt.max_disk_space_messages_per_interval = MAX_MESSAGES_PER_INTERVAL;
	} else if (opt.max_disk_space_messages_per_interval < MIN_MESSAGES_PER_INTERVAL) {
		fprintf(stderr, "Performance Monitor - max_disk_space_messages_per_interval must be not be less than %d.  Setting to %d.\n",MIN_MESSAGES_PER_INTERVAL, MIN_MESSAGES_PER_INTERVAL );
		opt.max_disk_space_messages_per_interval = MIN_MESSAGES_PER_INTERVAL;
	}

	if (opt.disk_space_interval < (60 *MINIMUM_MESSAGE_INTERVAL) ) {
		fprintf(stderr, "Performance Monitor - disk_space_interval must be not be less than %d minute.  Setting to %d minute.\n",MINIMUM_MESSAGE_INTERVAL, MINIMUM_MESSAGE_INTERVAL );
		opt.disk_space_interval = (60 *MINIMUM_MESSAGE_INTERVAL);
	} else if (opt.disk_space_interval > (60 *MAXIMUM_MESSAGE_INTERVAL) ) {
		fprintf(stderr, "Performance Monitor - disk_space_interval must be not be greater than than %d minutes.  Setting to %d minutes.\n",MAXIMUM_MESSAGE_INTERVAL, MAXIMUM_MESSAGE_INTERVAL );
		opt.disk_space_interval = (60 *MAXIMUM_MESSAGE_INTERVAL);
	}



	if (opt.tail_buffer_max == 0)
	{
		opt.tail_buffer_max = (1LL << 31); /* 2GB */
	}

	verbose = opt.v;
	min_query_time = opt.min_query_time;
	quantum = opt.quantum;

	fclose(fp);
	return 0;
}

static void parse_command_line(int argc, const char* const argv[])
{
	apr_getopt_t* os;
	int ch;
	const char* arg;
	const char* bin_start = NULL;
	int e;
	static apr_getopt_option_t option[] =
		{
		{ NULL, '?', 0, "Print help screen" },
		{ NULL, 'D', 1, "Configuration file" },
		{ NULL, 'p', 1, "GPDB port"},
		{ "version", 'V', 0, "Print version number" },
		{ NULL, 0, 0, NULL } };

	apr_pool_t* pool;

	if (0 != (e = apr_pool_create_alloc(&pool, 0)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
	}

	bin_start = argv[0] + strlen(argv[0]) - 1;
	while (bin_start != argv[0] && *bin_start != '/')
		bin_start--;
	if (bin_start[0] == '/')
		bin_start++;

	opt.pname = bin_start;

	if (0 != (e = apr_getopt_init(&os, pool, argc, argv)))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, e, "apr_getopt_init failed");
	}

	while (0 == (e = apr_getopt_long(os, option, &ch, &arg)))
	{
		switch (ch)
		{
		case '?':
			usage(0);
			break;
		case 'D':
			opt.conf_file = strdup(arg);
			break;
		case 'p':
			opt.gpdb_port = strdup(arg);
			break;
		case 'V':
			print_version();
			break;
		}
	}

	if (e != APR_EOF)
		usage("Error: illegal arguments");

	if (opt.conf_file == NULL)
		usage("Error: Missing configuration file");

	apr_pool_destroy(pool);
}

void interuptable_sleep(unsigned int seconds)
{
	int i;
	for (i = 0; i < seconds && !ax.exit; i++)
	    apr_sleep(apr_time_from_sec(1));

	if (ax.exit)
		exit(0);
}

int main(int argc, const char* const argv[])
{
	int db_check_count = 0;

	/* save argc, argv and startup dir so we can restart */
	opt.argc = argc;
	opt.argv = argv;

	if (apr_initialize())
	{
		fprintf(stderr, "Performance Monitor - Internal error, failed to initialize APR.\n");
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		exit(1);
	}

	parse_command_line(argc, argv);

	/* Set env if we got a port.  This will be picked up by libpq */
	if (opt.gpdb_port)
		setenv("PGPORT", opt.gpdb_port, 1);

	sigemptyset(&unblocksig);
	sigfillset(&blocksig);

	/* Set up signal handlers */
	if ((signal(SIGHUP, SIGHUP_handler) == SIG_ERR) ||
		(signal(SIGUSR2, SIGUSR2_handler) == SIG_ERR))
	{
		interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatal(FLINE, "Failed to set signal handlers\n");
	}

	/* unblock signals now that we have set up the handlers
	 * and can process them */

	sigprocmask(SIG_SETMASK, &unblocksig, NULL);

	/* Check for gpperfmon database.  If it doesn't exists,
	 * hang around until it does or we get a stop request */
	for (;;)
	{
		int gpperfmon_valid = 0;

		if (ax.exit)
			exit(0);

		gpperfmon_valid = gpdb_validate_gpperfmon();
		if (!gpperfmon_valid)
		{
			/* Don't want to fill up the log with these messages,
			 * so only log it once every 5 minutes */
			if (db_check_count % 5 == 0)
				fprintf(stderr, "Performance Monitor - There was a problem "
								"accessing the gpperfmon database.");

			db_check_count += 1;

			interuptable_sleep(60); // sleep to prevent loop of forking process and failing
		}
		else
			break;
	}

	getconfig();
	read_conf_file(opt.conf_file);

	/* redirect output to log_file */
	/* stdout goes to log: debug and warnings */
	/* stderr goes to pg_log: fatal errors */
	{
		if (gpmon_recursive_mkdir(opt.log_dir))
		{
			fprintf(stderr, "\nPerformance Monitor -- cannot create directory %s", opt.log_dir);
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "cannot create directory %s", opt.log_dir);
		}

		update_mmonlog_filename();
		if (!opt.console && !freopen(mmon_log_filename, "w", stdout))
		{
			fprintf(stderr, "\nPerformance Monitor -- failed to open perfmon log file %s\n", mmon_log_filename);
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatal(FLINE, "\nfailed (1) to open perfmon log file %s\n", mmon_log_filename);
		}
		TR0(("starting mmon logging\n"));
	}

	/* check port */
	if (!(0 < ax.port && ax.port < (1 << 16)))
	{
		usage("Error: invalid port number");
	}

	/* check that we are indeed running in a postgres data directory */
	{
		FILE* fp = fopen("pg_hba.conf", "r");
		if (!fp)
		{
			usage("Error: master data directory is not valid; can't find pg_hba.conf");
		}
		SET_MAXFD(fileno(fp));
		fclose(fp);
	}

	/* start up gpperfmon directory */
	{
		char work_dir[MAXPATHLEN+1] = {0};
		strncpy(work_dir, GPMON_DIR, MAXPATHLEN);
		work_dir[MAXPATHLEN] = 0;

		if (gpmon_recursive_mkdir(work_dir))
		{
			fprintf(stderr, "\ncannot create directory %s/%s", ax.master_data_directory, GPMON_DIR);
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "cannot create directory %s/%s", ax.master_data_directory, GPMON_DIR);
		}
	}

	create_log_alert_table();
	gpmmon_main();

	cleanup();

	return 0;
}

void populate_smdw_aliases(host_t* host)
{
	char* saveptr;
	char* token;

	if (!opt.smdw_aliases)
	{
		return;
	}

	token = strtok_r(opt.smdw_aliases, ",", &saveptr);
	while (token)
	{
		if (!host->addressinfo_tail)
		{
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, 0, "smdw addressname structure is inconsistent");
		}

		// permenant memory for address list -- stored for duration
		host->addressinfo_tail->next = calloc(1, sizeof(addressinfo_holder_t));
		CHECKMEM(host->addressinfo_tail);

		host->addressinfo_tail = host->addressinfo_tail->next;

		host->addressinfo_tail->address = strdup(token);
		CHECKMEM(host->addressinfo_tail->address);

		host->address_count++;

		token = strtok_r(NULL, ",", &saveptr);
	}
}

// returnParamIsIpv6 will be set to true for IPv6 addresses
// the actual IP address string is returned from the function
char* get_ip_for_host(char* host, bool* returnParamIsIpv6)
{
	char * ipstr;
	int			ret;
	struct addrinfo *addrs = NULL;
	struct addrinfo hint;

	/* Initialize hint structure */
	memset(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM; /* TCP */
	hint.ai_family = AF_UNSPEC;	/* Allow for any family */

	ret = getaddrinfo(host, NULL, &hint, &addrs);
	if (ret || !addrs)
	{
		if (addrs)
			freeaddrinfo(addrs);

		gpmon_fatalx(FLINE, 0, "getaddrinfo returned %s",  gai_strerror(ret));
		return NULL;
	}

	ipstr = malloc(128);
	/* just grab the first address... it should be fine */
	if (addrs->ai_family == AF_INET)
	{
		struct sockaddr_in* sock = (struct sockaddr_in*)addrs->ai_addr;
		inet_ntop(addrs->ai_family, &sock->sin_addr, ipstr, 128);
		*returnParamIsIpv6 = false;
	}
	else if (addrs->ai_family == AF_INET6)
	{
		struct sockaddr_in6* sock = (struct sockaddr_in6*)addrs->ai_addr;
		inet_ntop(addrs->ai_family, &sock->sin6_addr, ipstr, 128);
		*returnParamIsIpv6 = true;
	}
	else
	{
	    interuptable_sleep(30); // sleep to prevent loop of forking process and failing
		gpmon_fatalx(FLINE, 0, "Bad address family for host: %s %d", host, addrs->ai_family);
	}
	freeaddrinfo(addrs);

	return ipstr;
}

#define GPMMON_GETHOSTLIST_LINE_BUFSIZE 1024
static void gethostlist()
{
	int i = 0;

	/* Connect to database, get segment hosts from gp_segment_configuration */
	gpdb_get_hostlist(&ax.hosttabsz, &ax.hosttab, ax.pool, &opt);

	for (i = 0; i < ax.hosttabsz; ++i)
	{
		addressinfo_holder_t* addressinfo;

		// there are potentially more hostnames for standby master
		// specified in the config file
		if (ax.standby_master_hostname && strcmp(ax.standby_master_hostname, ax.hosttab[i].hostname) == 0)
	  	{
			populate_smdw_aliases(&ax.hosttab[i]);
		}

		addressinfo = ax.hosttab[i].addressinfo_head;
		while (addressinfo)
		{
			addressinfo->ipstr = get_ip_for_host(addressinfo->address, &addressinfo->ipv6);
			if (!addressinfo->ipstr)
			{
				interuptable_sleep(60); // sleep to prevent loop of forking process and failing
				gpmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "cannot convert host %s to IP", addressinfo->address);
			}
			addressinfo = addressinfo->next;
		}
	}

	// SANITY TEST AND DEBUG PRINT
	TR0(("found %d unique live hosts from catalog\n", ax.hosttabsz));

	for (i = 0; i < ax.hosttabsz; i++)
	{
		addressinfo_holder_t* addressinfo;
		int counter = 0;

		TR0(("HOST: (hostname %s) (is_master %d) (datadir %s) (host_alias_count %d) (hdm %d) (hdw %d) (hbw %d) (hdc %d) (dia %d)\n",
			ax.hosttab[i].hostname,
			ax.hosttab[i].is_master,
			ax.hosttab[i].data_dir,
			ax.hosttab[i].address_count,
			ax.hosttab[i].is_hdm,
			ax.hosttab[i].is_hdw,
			ax.hosttab[i].is_hbw,
			ax.hosttab[i].is_hdc,
			ax.hosttab[i].is_etl));

		addressinfo = ax.hosttab[i].addressinfo_head;
		while (addressinfo)
		{
			// extra sanity checking
			counter++;
			if (counter > ax.hosttab[i].address_count)
			{
				gpmon_fatalx(FLINE, 0, "address counter exceeds number of addresses for host %s", ax.hosttab[i].hostname);
			}

			const char* ipv6on = NULL;
			if (addressinfo->ipv6)
				ipv6on = YES_TEXT;
			else
				ipv6on = NO_TEXT;

			TR1(("\tALIAS: (host %s) (ipstr %s) (ipv6 %s)\n", addressinfo->address, addressinfo->ipstr, ipv6on));
			addressinfo = addressinfo->next;
		}
	}


}

/* send a packet thru sock */
static apr_status_t sendpkt(int sock, const gp_smon_to_mmon_packet_t* pkt)
{
	const char* p = (const char*) pkt;
	const char* q = p + sizeof(*pkt);
	while (p < q)
	{
		int n = send(sock, p, q - p, 0);
		if (n == -1)
		{
			switch (errno)
			{
			case EINTR:
			case EAGAIN:
				continue;
			}
			return APR_FROM_OS_ERROR(errno);
		}
		p += n;
	}
	return 0;
}
/* recv data through a sock */
static apr_status_t recv_data(int sock, char* data, size_t data_size)
{
	char* p = data;
	char* q = p + data_size;
	while (p < q)
	{
		int n = recv(sock, p, q - p, 0);
		if (n == -1)
		{
			// because we use blocking recv. if errno = EAGAIN,
			// it means timeout happened.
			if (errno == EINTR)
				continue;
			return APR_FROM_OS_ERROR(errno);
		}
		// because we use blocking recv, it indicates gpsmon
		// shutdown or closed connection when n = 0.
		if (n == 0)
			return APR_FROM_OS_ERROR(EINTR);
		p += n;
	}

	TR2(("read data from sock %d\n", sock));
	return 0;
}

/* recv a packet thru sock */
static apr_status_t recvpkt(int sock, gp_smon_to_mmon_packet_t* pkt, bool loop_until_all_recv)
{
	int e = 0;

	//receive the header
	if (0 != (e = recv_data(sock, (char *)&pkt->header, sizeof(gp_smon_to_mmon_header_t)))) {
		return e;
	}

	if (pkt->header.pkttype == GPMON_PKTTYPE_QEXEC)
	{
		// Get the data portion, then get the line
		if (0 != (e = recv_data(sock, (char *)&pkt->u.qexec_packet.data, sizeof(qexec_packet_data_t))))
		{
			return e;
		}
	}
	else
	{
		//receive the union packet
		if (0 != (e = recv_data(sock, (char *)&pkt->u, get_size_by_pkttype_smon_to_mmon(pkt->header.pkttype)))) {
			return e;
		}
	}
	return 0;
}


static void getconfig(void)
{
	char *hostname = NULL;
	char *master_data_directory = NULL;
	char *standby_master_hostname = NULL;
	int rc = 0;

	static apr_pool_t *pool = NULL;

	int port = gpdb_get_gpmon_port();
	ax.port = port;

	if (pool == NULL)
	{
		if (APR_SUCCESS != (rc = apr_pool_create_alloc(&pool, NULL)))
		{
			interuptable_sleep(30); // sleep to prevent loop of forking process and failing
			gpmon_fatalx(FLINE, rc, "Failed to create APR pool\n");
		}
	}

	/* fetch datadir */
	gpdb_get_master_data_dir(&hostname, &master_data_directory, pool);
	if (ax.master_data_directory == NULL)
	{
		ax.master_data_directory = strdup(master_data_directory);
		CHECKMEM(ax.master_data_directory);
	}

	/* fetch standby master hostname */
	gpdb_get_single_string_from_query("select hostname from gp_segment_configuration where content = -1 and role = 'm'", &standby_master_hostname, pool);
	if (standby_master_hostname)
	{
		ax.standby_master_hostname = strdup(standby_master_hostname);
		CHECKMEM(ax.standby_master_hostname);
	}
	else
	{
		ax.standby_master_hostname =  NULL;
	}

	/* clear pool for next call */
	apr_pool_clear(pool);
}
