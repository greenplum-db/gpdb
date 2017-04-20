#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#if defined (sun)
	#include <kstat.h>
#endif /* sun */
#include <math.h>
#include <sys/param.h>
#include "apr_getopt.h"
#include "apr_env.h"
#include "apr_hash.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "gpmonlib.h"
#include "gpmon/gpmon.h"
#include "apr_thread_proc.h"
#include "event.h"
#include "sigar.h"
#include <time.h>

#define FMT64 APR_INT64_T_FMT
#define APPLIANCE_FILE "/etc/gpdb-appliance-version"
#define APPLIANCE_HOSTNAME_FILE "/etc/gpnode"


/* Macros for min because solaris doesn't have it. */
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */

void update_log_filename(void);
int is_appliance(void);
char* get_dca_hostname(char*, size_t);
void gx_main(int, apr_int64_t);

/* Temporary global memory to store the qexec line for a send*/
char	qexec_smon_temp_line[QEXEC_MAX_ROW_BUF_SIZE];


// no locking of log file in smon because it is single threaded
apr_thread_mutex_t *logfile_mutex = NULL;

static struct
{
	const char* pname;
	int v;
	int V;
	int D;
	const char* arg_port;
	const char* log_dir;
	apr_uint64_t max_log_size;
	int iterator_aggregate;
	// The timeout in seconds for smon to restart if no requests
	// come during that period.
	apr_uint64_t terminate_timeout;
	// If gpsmon ignores query exec packets. We add this flag
	// due to out of memory issue on gpmmon when segment number or
	// splice number or partition number is big. [JIRA: MPP-25017].
	bool do_ignore_qexec_packet;
} opt = { 0 };

int verbose = 0; /* == opt.v */
int very_verbose = 0; /* == opt.V */
int number_cpu_cores = 1;
float cpu_cores_utilization_multiplier = 1.0; /* multipy CPU % from libsigar by this factor to get the CPU % per machine */

typedef struct pidrec_t pidrec_t;
struct pidrec_t
{
	apr_uint64_t updated_tick; /* when this pidrec was updated */
	apr_uint32_t pid;
	char* pname;
	char* cwd;
	gpmon_proc_metrics_t p_metrics;
	apr_uint64_t cpu_elapsed;
	gpmon_qlogkey_t query_key;
};

typedef struct gx_t gx_t;
struct gx_t
{
	int port;
	apr_int64_t signature;
	apr_uint64_t tick;
	time_t now;
	int is_appliance;

	sigar_t* sigar;

	/*fslist Does not incude remote filesystems and used for reporting metrics, not space avaliable & free.*/
	const char** fslist;
	const char** devlist;
	const char** netlist;

	/*This fs list includes remote filesystems and is used for reporting space avaliable & free. */
	const char** allfslist;

	SOCKET listen_sock;
	struct event listen_event;

	SOCKET tcp_sock;
	struct event tcp_event;

	SOCKET udp_sock;
	struct event udp_event;

	apr_pool_t* pool;
	int qd_pid;
	const char* hostname; /* my hostname */

	/* hash tables */
	apr_hash_t* qexectab; /* stores qexec packets */
	apr_hash_t* qlogtab; /* stores qlog packets */
	apr_hash_t* segmenttab; /* stores segment packets */
	apr_hash_t* pidtab; /* key=pid, value=pidrec_t */
	apr_hash_t* filereptab; /* stores gpmon_filerepinfo_t packets */
	apr_hash_t* querysegtab; /* stores gpmon_query_seginfo_t */
};

typedef struct qexec_agg_hash_key_t {
	apr_int32_t tmid;	/* transaction time */
	apr_int32_t ssid;	/* session id */
	apr_int16_t ccnt;	/* command count */
	apr_int16_t nid;	/* plan node id */
}qexec_agg_hash_key_t;


typedef struct qexec_agg_t{
	qexec_agg_hash_key_t key;
	apr_hash_t* qexecaggtab;
}qexec_agg_t;

static struct gx_t gx = { 0 };

#if defined (sun)
static kstat_ctl_t *kc = NULL;
#endif /* sun */

/* structs and hash tables for metrics */
static apr_hash_t* net_devices = NULL;
static apr_hash_t* disk_devices = NULL;
struct timeval g_time_last_reading = { 0 };

typedef struct net_device_t net_device_t;
struct net_device_t
{
	char* name;
	apr_uint64_t rx_bytes;
	apr_uint64_t tx_bytes;
	apr_uint64_t rx_packets;
	apr_uint64_t tx_packets;
};

typedef struct disk_device_t disk_device_t;
struct disk_device_t
{
	char* name;
	apr_uint64_t reads;
	apr_uint64_t writes;
	apr_uint64_t read_bytes;
	apr_uint64_t write_bytes;
};


#define LOG_FILENAME_SIZE 64
char log_filename[LOG_FILENAME_SIZE];
void update_log_filename()
{
	time_t stamp = time(NULL);
	struct tm* tm = gmtime(&stamp);
	snprintf(log_filename, LOG_FILENAME_SIZE, "gpsmon.%d.%02d.%02d_%02d%02d%02d.log",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec);
}

typedef struct gpsmon_filerepinfo_t
{
	// KEY
	char primary_hostname[NAMEDATALEN];
	apr_uint16_t primary_port;
	char mirror_hostname[NAMEDATALEN];
	apr_uint16_t mirror_port;
	bool isPrimary;

} gpsmon_filerepinfo_t;



static void gx_accept(SOCKET sock, short event, void* arg);
static void gx_recvfrom(SOCKET sock, short event, void* arg);
static apr_uint32_t create_qexec_packet(const gpmon_qexec_t* qexec, gp_smon_to_mmon_packet_t* pkt);

/**
 * helper function to copy the union packet from a gpmon_packet_t to a gp_smon_to_mmon_packet_t
 * @note This function should never be called with a qexec packet!
 */
static inline void copy_union_packet_gp_smon_to_mmon(gp_smon_to_mmon_packet_t* pkt, const gpmon_packet_t* pkt_src)
{
	switch (pkt_src->pkttype) {
		case GPMON_PKTTYPE_HELLO:
			memcpy(&pkt->u.hello, &pkt_src->u.hello, sizeof(gpmon_hello_t));
			break;
		case GPMON_PKTTYPE_METRICS:
			memcpy(&pkt->u.metrics, &pkt_src->u.metrics, sizeof(gpmon_metrics_t));
			break;
		case GPMON_PKTTYPE_QLOG:
			memcpy(&pkt->u.qlog, &pkt_src->u.qlog, sizeof(gpmon_qlog_t));
			break;
		case GPMON_PKTTYPE_SEGINFO:
			memcpy(&pkt->u.seginfo, &pkt_src->u.seginfo, sizeof(gpmon_seginfo_t));
			break;
		case GPMON_PKTTYPE_FILEREP:
			memcpy(&pkt->u.filerepinfo, &pkt_src->u.filerepinfo, sizeof(gpmon_filerepinfo_t));
			break;
		case GPMON_PKTTYPE_QUERY_HOST_METRICS:
			memcpy(&pkt->u.qlog, &pkt_src->u.qlog, sizeof(gpmon_qlog_t));
			break;
		case GPMON_PKTTYPE_FSINFO:
			memcpy(&pkt->u.fsinfo, &pkt_src->u.fsinfo, sizeof(gpmon_fsinfo_t));
			break;
		case GPMON_PKTTYPE_QUERYSEG:
		case GPMON_PKTTYPE_QEXEC:
		default:
			gpmon_fatal(FLINE, "Invalid pkttype %d for copy_union_packet_gp_smon_to_mmon\n", pkt_src->pkttype);
			break;
	}
	return;
}

/**
 * This local helper function allocates a gp_smon_to_mmon_packet_t and copies the gpmon_packet_t to it
 * @note This function should never be called with a qexec packet!
 */
static gp_smon_to_mmon_packet_t* gx_pkt_to_smon_to_mmon(apr_pool_t* pool, const gpmon_packet_t* pkt)
{
	gp_smon_to_mmon_packet_t* t = apr_palloc(pool, sizeof(*t));
	CHECKMEM(t);
	gp_smon_to_mmon_set_header(t, pkt->pkttype);
	copy_union_packet_gp_smon_to_mmon(t, pkt);
	return t;
}

static void gx_exit(const char* reason)
{
	TR0(("exit %s\n", reason ? reason : "1"));
	exit(reason ? 1 : 0);
}

static void send_fully(SOCKET sock, const void* p_, int len)
{
	const char* p = p_;
	const char* q = p + len;
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
			gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "send failed");
		}
		p += n;
	}
}
/* Helper function to send the header and then send the union packet */
static void send_smon_to_mon_pkt(SOCKET sock, gp_smon_to_mmon_packet_t* pkt)
{
	send_fully(sock, &pkt->header, sizeof(gp_smon_to_mmon_header_t));
	if (pkt->header.pkttype == GPMON_PKTTYPE_QEXEC) {
		send_fully(sock, &pkt->u.qexec_packet.data, sizeof(qexec_packet_data_t) );
		send_fully(sock, pkt->u.qexec_packet.line, pkt->u.qexec_packet.data.size_of_line);
	} else {
		send_fully(sock, &pkt->u, get_size_by_pkttype_smon_to_mmon(pkt->header.pkttype));
	}
	TR2(("Sent packet of type %d to mmon\n", pkt->header.pkttype));
}

static void get_pid_metrics(apr_int32_t pid, apr_int32_t tmid, apr_int32_t ssid, apr_int32_t ccnt)
{
	sigar_proc_cpu_t cpu;
	sigar_proc_mem_t mem;
	sigar_proc_fd_t fd;
	/*int newrec = 0;*/
	pidrec_t* rec;
	apr_pool_t* pool = apr_hash_pool_get(gx.pidtab);

	rec = apr_hash_get(gx.pidtab, &pid, sizeof(pid));
	if (rec && rec->updated_tick == gx.tick)
		return; /* updated in current cycle */

	TR2(("--------------------- stating %d\n", pid));
	memset(&cpu, 0, sizeof(cpu));
	memset(&mem, 0, sizeof(mem));
	memset(&fd, 0, sizeof(fd));
	sigar_proc_mem_get(gx.sigar, pid, &mem);
	sigar_proc_cpu_get(gx.sigar, pid, &cpu);
	sigar_proc_fd_get(gx.sigar, pid, &fd);

	if (!rec)
	{
		sigar_proc_exe_t exe;

		/*newrec = 1;*/

		rec = apr_palloc(pool, sizeof(*rec));
		CHECKMEM(rec);

		rec->pid = pid;
		rec->query_key.tmid = tmid;
		rec->query_key.ssid = ssid;
		rec->query_key.ccnt = ccnt;

		rec->pname = rec->cwd = 0;
		if (0 == sigar_proc_exe_get(gx.sigar, pid, &exe))
		{
			rec->pname = apr_pstrdup(pool, exe.name);
			rec->cwd = apr_pstrdup(pool, exe.root);
		}
		if (!rec->pname)
			rec->pname = "unknown";
		if (!rec->cwd)
			rec->cwd = "unknown";

		apr_hash_set(gx.pidtab, &rec->pid, sizeof(rec->pid), rec);
	}

	rec->updated_tick = gx.tick;
	rec->p_metrics.fd_cnt = (apr_uint32_t) fd.total;
	rec->p_metrics.cpu_pct = (float) (cpu.percent * cpu_cores_utilization_multiplier);
	rec->p_metrics.mem.size = mem.size;
	rec->p_metrics.mem.resident = mem.resident;

#ifdef __linux__
	rec->p_metrics.mem.share = mem.share;
#else
	rec->p_metrics.mem.share = 0;
#endif

	rec->cpu_elapsed = cpu.total;
}

int is_appliance(){
	FILE* fp = fopen(APPLIANCE_FILE, "r");
	if (fp){
		fclose(fp);
		return 1;
	}
	else{
		return 0;
	}
}


#if defined (sun)
/* Takes a kstat entry and determines if it is
 * and active network card on the system.
 */
int is_active_nic(kstat_t *ksp)
{
	char nic_name[64] = { '\0' };

	if (ksp == NULL)
		return 0;

	/* An example of a kstat entry that would match would be module=e1000g and
	 * instance=1.
	 */
	if (strcmp(ksp->ks_class, "net") == 0 && strcmp(ksp->ks_module, "lo") != 0)
		snprintf(nic_name, 64, "%s%d", ksp->ks_module, ksp->ks_instance);
		if (strcmp(ksp->ks_name, nic_name) == 0)
			return 1;
	return 0;
}
#endif /* sun */

#define FSUSAGE_TOBYTES(X) (X * 1024)

static void send_fsinfo(SOCKET sock)
{
	sigar_file_system_usage_t fsusage;
	gp_smon_to_mmon_packet_t  pkt;
	const char**              fsdir;
	int                       status = 0;

	memset(&fsusage, 0, sizeof(sigar_file_system_usage_t));

	for (fsdir = gx.fslist; *fsdir; fsdir++)
	{
		status = sigar_file_system_usage_get(gx.sigar, *fsdir, &fsusage);
		if (status == SIGAR_OK)
		{
			TR2(("sigar_file_system_usage_get() succeeded. fsdir: %s total: %lu free: %lu used: %lu \n", *fsdir, fsusage.total, fsusage.free, fsusage.used));
			memset(&pkt, 0, sizeof(gp_smon_to_mmon_packet_t));

			gp_smon_to_mmon_set_header(&pkt,GPMON_PKTTYPE_FSINFO);

			strncpy(pkt.u.fsinfo.key.fsname, *fsdir, sizeof(pkt.u.fsinfo.key.fsname) - 1);

			pkt.u.fsinfo.bytes_used = FSUSAGE_TOBYTES(fsusage.used);
			pkt.u.fsinfo.bytes_available = FSUSAGE_TOBYTES(fsusage.free);
			pkt.u.fsinfo.bytes_total = FSUSAGE_TOBYTES(fsusage.total);
			strncpy(pkt.u.fsinfo.key.hostname, gx.hostname, sizeof(pkt.u.fsinfo.key.hostname) - 1);

			send_smon_to_mon_pkt(sock, &pkt);
		}
		else
		{
			TR2(("sigar_file_system_usage_get() failed.  fsdir: %s status: %i \n", *fsdir, status));
		}
	}
}
#if !(defined (sun))
// Helper function to calculate the metric differences
static apr_uint64_t metric_diff_calc( sigar_uint64_t newval, apr_uint64_t oldval, const char *name_for_log, const char* value_name_for_log ){
	apr_uint64_t diff;

	if (newval < oldval) // assume that the value was reset and we are starting over
	{
		TR0(("metric_diff_calc: new value %" APR_UINT64_T_FMT " is less than old value %" APR_UINT64_T_FMT " for %s metric %s; assume the value was reset and set diff to new value.\n",
				newval, oldval, name_for_log, value_name_for_log));
		diff = newval;
	}
	else
	{
		diff = newval - oldval;
	}
#if defined(rhel4_x86_64) || defined(rhel5_x86_64) || defined(rhel7_x86_64) || defined(rhel6_x86_64) || defined(sol10_x86_64) || defined(suse10_x86_64)
	// Add this debug on 64 bit machines to try and debug strange values we are seeing
	if(diff > 1000000000000000000  ) {
		TR0(("Crazy high value for diff! new value=%" APR_UINT64_T_FMT ", old value=%" APR_UINT64_T_FMT ", diff=%" APR_UINT64_T_FMT "  for %s metric %s; assume the value was reset and set diff to new value.\n",
				newval, oldval, name_for_log, value_name_for_log));
	}
#endif
	return diff;
}
#endif

// Helper function to calculate cpu percentage during a period
static float calc_diff_percentage(sigar_uint64_t newvalue, sigar_uint64_t oldvalue, int total_diff, const char *itemname)
{
	float result = ((float) (newvalue - oldvalue) * 100 / total_diff);
	if (newvalue < oldvalue)
	{
		TR0(("calc_diff_percentage: new value %" APR_UINT64_T_FMT " is less than old value %" APR_UINT64_T_FMT " for metric %s; set to 0.\n",
				newvalue, oldvalue, itemname));
		result = 0.0;
	}
	else if (result > 100)
	{
		TR0(("calc_diff_percentage: new value %" APR_UINT64_T_FMT " old value %" APR_UINT64_T_FMT " total diff %d for metric %s; set to 100.\n",
				newvalue, oldvalue, total_diff, itemname));
		result = 100;
	}
	return result;
}

static void send_machine_metrics(SOCKET sock)
{
	sigar_mem_t mem;
	sigar_swap_t swap;
	sigar_cpu_t cpu;
	sigar_loadavg_t loadavg;
	sigar_disk_usage_t tdisk;
	sigar_net_interface_stat_t tnet;
	static int first = 1;
	static sigar_cpu_t pcpu = { 0 };
	static sigar_swap_t pswap = { 0 };
	gp_smon_to_mmon_packet_t pkt;
	struct timeval currenttime = { 0 };
	double seconds_duration = 0.0;
#if defined (sun)
	kstat_t *ksp = NULL;
	kstat_named_t *knp = NULL;
	kstat_io_t kio;
#else /* sun */
	sigar_file_system_usage_t fsusage;
	const char** fsdir;
	const char** netname;
	sigar_net_interface_stat_t netstat;
#endif /* sun */
	int cpu_total_diff;

	/* NIC metrics */
	apr_uint64_t rx_packets = 0;
	apr_uint64_t tx_packets = 0;
	apr_uint64_t rx_bytes = 0;
	apr_uint64_t tx_bytes = 0;

	/* Disk metrics */
	apr_uint64_t reads = 0;
	apr_uint64_t writes = 0;
	apr_uint64_t read_bytes = 0;
	apr_uint64_t write_bytes = 0;

	memset(&mem, 0, sizeof(mem));
	sigar_mem_get(gx.sigar, &mem);
	TR2(("mem ram: %" FMT64 " total: %" FMT64 " used: %" FMT64 " free: %" FMT64 "\n",
		 mem.ram, mem.total, mem.used, mem.free));

	memset(&swap, 0, sizeof(swap));
	sigar_swap_get(gx.sigar, &swap);
	TR2(("swap total: %" FMT64 " used: %" FMT64 "page_in: %" FMT64 " page_out: %" FMT64 "\n",
		 swap.total, swap.used, swap.page_in, swap.page_out));

	memset(&cpu, 0, sizeof(cpu));
	sigar_cpu_get(gx.sigar, &cpu);
	TR2(("cpu user: %" FMT64 " sys: %" FMT64 " idle: %" FMT64 " wait: %" FMT64 " nice: %" FMT64 " total: %" FMT64 "\n",
			cpu.user, cpu.sys, cpu.idle, cpu.wait, cpu.nice, cpu.total));

	memset(&loadavg, 0, sizeof(loadavg));
	sigar_loadavg_get(gx.sigar, &loadavg);
	TR2(("load_avg: %e %e %e\n", loadavg.loadavg[0], loadavg.loadavg[1], loadavg.loadavg[2]));
	memset(&tdisk, 0, sizeof(tdisk));
	memset(&tnet, 0, sizeof(tnet));

#if defined (sun)
	/* libsigar currently doesn't support ZFS, so for now
	 use kstat directly for drive I/O stats

	 Additionally, thumper and thors have network I/O stat
	 issues with libsigar so we'll pull those from kstat
	 as well.

	 TODO: Fix libsigar (update it?)
	 */
	kstat_chain_update(kc);

	for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	{
		switch(ksp->ks_type)
		{
			case KSTAT_TYPE_IO:
			if (strcmp(ksp->ks_class, "disk") == 0)
			{
				disk_device_t* disk = (disk_device_t*)apr_hash_get(disk_devices, ksp->ks_name, APR_HASH_KEY_STRING);
				/* Check if this is a new device */
				if (!disk)
				{
					disk = (disk_device_t*)apr_palloc(gx.pool, sizeof(disk_device_t));
					disk->name = apr_pstrdup(gx.pool, ksp->ks_name);
					disk->read_bytes = disk->write_bytes = disk->reads = disk->writes = 0;
					apr_hash_set(disk_devices, disk->name, APR_HASH_KEY_STRING, disk);
				}

				reads = disk->reads;
				writes = disk->writes;
				read_bytes = disk->read_bytes;
				write_bytes = disk->write_bytes;

				kstat_read(kc, ksp, &kio);
				if (kio.reads < disk->reads)
				{
					disk->reads = (GPSMON_METRIC_MAX - disk->reads) + kio.reads;
					reads = disk->reads;
				}
				else
				{
					reads = kio.reads - disk->reads;
					disk->reads = kio.reads;
				}

				if (kio.writes < disk->writes)
				{
					disk->writes = (GPSMON_METRIC_MAX - disk->writes) + kio.writes;
					writes = disk->writes;
				}
				else
				{
					writes = kio.writes - disk->writes;
					disk->writes = kio.writes;
				}

				if (kio.nwritten < disk->write_bytes)
				{
					disk->write_bytes = (GPSMON_METRIC_MAX - disk->write_bytes) + kio.nwritten;
					write_bytes = disk->write_bytes;
				}
				else
				{
					write_bytes = kio.nwritten - disk->write_bytes;
					disk->write_bytes = kio.nwritten;
				}

				if (kio.nread < disk->read_bytes)
				{
					disk->read_bytes = (GPSMON_METRIC_MAX - disk->read_bytes) + kio.nread;
					read_bytes = disk->read_bytes;
				}
				else
				{
					read_bytes = kio.nread - disk->read_bytes;
					disk->read_bytes = kio.nread;
				}

				tdisk.reads += reads;
				tdisk.writes += writes;
				tdisk.write_bytes += write_bytes;
				tdisk.read_bytes += read_bytes;
			}
			break;
			case KSTAT_TYPE_NAMED:
			if (is_active_nic(ksp))
			{
				char nic_name[64] = { '\0' };

				snprintf(nic_name, 64, "%s%d", ksp->ks_module, ksp->ks_instance);

				net_device_t* nic = (net_device_t*)apr_hash_get(net_devices, nic_name, APR_HASH_KEY_STRING);
				/* Check if this is a new device */
				if (!nic)
				{
					nic = (net_device_t*)apr_palloc(gx.pool, sizeof(net_device_t));
					nic->name = apr_pstrdup(gx.pool, nic_name);
					nic->tx_bytes = nic->rx_bytes = nic->tx_packets = nic->rx_packets = 0;
					apr_hash_set(net_devices, nic->name, APR_HASH_KEY_STRING, nic);
				}

				rx_packets = nic->rx_packets;
				tx_packets = nic->tx_packets;
				rx_bytes = nic->rx_bytes;
				tx_bytes = nic->tx_bytes;

				kstat_read(kc, ksp, NULL);
				/* outbound bytes */
				knp = kstat_data_lookup(ksp, "obytes64");
				if (knp)
				{
					if (knp->value.ui64 < nic->tx_bytes)
					{
						nic->tx_bytes = (GPSMON_METRIC_MAX - nic->tx_bytes) + knp->value.ui64;
						tx_bytes = nic->tx_bytes;
					}
					else
					{
						tx_bytes = knp->value.ui64 - nic->tx_bytes;
						nic->tx_bytes = knp->value.ui64;
					}
				}
				/* inbound bytes */
				knp = kstat_data_lookup(ksp, "rbytes64");
				if (knp)
				{
					if (knp->value.ui64 < nic->rx_bytes)
					{
						nic->rx_bytes = (GPSMON_METRIC_MAX - nic->rx_bytes) + knp->value.ui64;
						rx_bytes = nic->rx_bytes;
					}
					else
					{
						rx_bytes = knp->value.ui64 - nic->rx_bytes;
						nic->rx_bytes = knp->value.ui64;
					}
				}
				/* outbound packets */
				knp = kstat_data_lookup(ksp, "opackets64");
				if (knp)
				{
					if (knp->value.ui64 < nic->tx_packets)
					{
						nic->tx_packets = (GPSMON_METRIC_MAX - nic->tx_packets) + knp->value.ui64;
						tx_packets = nic->tx_packets;
					}
					else
					{
						tx_packets = knp->value.ui64 - nic->tx_packets;
						nic->tx_packets = knp->value.ui64;
					}
				}
				/* inbound packets */
				knp = kstat_data_lookup(ksp, "ipackets64");
				if (knp)
				{
					if (knp->value.ui64 < nic->rx_packets)
					{
						nic->rx_packets = (GPSMON_METRIC_MAX - nic->rx_packets) + knp->value.ui64;
						rx_packets = nic->rx_packets;
					}
					else
					{
						rx_packets = knp->value.ui64 - nic->rx_packets;
						nic->rx_packets = knp->value.ui64;
					}
				}
				/* Add this interfaces diff to the total
				 * Overflow here is extremely unlikely considering these are 64-bit values
				 * and it's just a diff...  An example would be on a system with 4 NICs sending
				 * a total of 0xffffffffffffffff bytes between updates.
				 */
				tnet.rx_bytes += rx_bytes;
				tnet.tx_bytes += tx_bytes;
				tnet.rx_packets += rx_packets;
				tnet.tx_packets += tx_packets;
			}
			break;
			default:
			/* not interested */
			break;
		}
	}
#else /* sun */
	for (fsdir = gx.fslist; *fsdir; fsdir++)
	{
		int e = sigar_file_system_usage_get(gx.sigar, *fsdir, &fsusage);

		if (0 == e)
		{
			disk_device_t* disk = (disk_device_t*)apr_hash_get(disk_devices, *fsdir, APR_HASH_KEY_STRING);
			/* Check if this is a new device */
			if (!disk)
			{
				disk = (disk_device_t*)apr_palloc(gx.pool, sizeof(disk_device_t));
				disk->name = apr_pstrdup(gx.pool, *fsdir);
				disk->read_bytes = disk->write_bytes = disk->reads = disk->writes = 0;
				apr_hash_set(disk_devices, disk->name, APR_HASH_KEY_STRING, disk);
			}
			reads = disk->reads;
			writes = disk->writes;
			read_bytes = disk->read_bytes;
			write_bytes = disk->write_bytes;

			// DISK READS
			reads = metric_diff_calc(fsusage.disk.reads, disk->reads, disk->name, "disk reads");
			disk->reads = fsusage.disk.reads; // old = new

			// DISK WRITES
			writes = metric_diff_calc(fsusage.disk.writes, disk->writes, disk->name, "disk writes");
			disk->writes = fsusage.disk.writes; // old = new

			// WRITE BYTES
			write_bytes = metric_diff_calc(fsusage.disk.write_bytes, disk->write_bytes, disk->name, "disk write bytes");
			disk->write_bytes = fsusage.disk.write_bytes; // old = new

			// READ BYTES
			read_bytes = metric_diff_calc(fsusage.disk.read_bytes, disk->read_bytes, disk->name, "disk read bytes");
			disk->read_bytes = fsusage.disk.read_bytes; // old = new

			tdisk.reads += reads;
			tdisk.writes += writes;
			tdisk.write_bytes += write_bytes;
			tdisk.read_bytes += read_bytes;
		}
	}
#endif /* sun */
	TR2(("disk reads: %" APR_UINT64_T_FMT " writes: %" APR_UINT64_T_FMT
		 " rbytes: %" APR_UINT64_T_FMT " wbytes: %" APR_UINT64_T_FMT "\n",
		 tdisk.reads, tdisk.writes, tdisk.read_bytes, tdisk.write_bytes));

#if !defined(sun)
	for (netname = gx.netlist; *netname; netname++)
	{
		int e = sigar_net_interface_stat_get(gx.sigar, *netname, &netstat);

		if (0 == e)
		{
			net_device_t* nic = (net_device_t*)apr_hash_get(net_devices, *netname, APR_HASH_KEY_STRING);

			/* Check if this is a new device */
			if (!nic)
			{
				nic = (net_device_t*)apr_palloc(gx.pool, sizeof(net_device_t));
				nic->name = apr_pstrdup(gx.pool, *netname);
				nic->tx_bytes = nic->rx_bytes = nic->tx_packets = nic->rx_packets = 0;
				apr_hash_set(net_devices, nic->name, APR_HASH_KEY_STRING, nic);
			}

			//////// RECEIVE PACKEtS
			rx_packets = metric_diff_calc(netstat.rx_packets, nic->rx_packets, nic->name, "rx packets");
			nic->rx_packets = netstat.rx_packets; // old = new

			//////// RECEIVE BYTES
			rx_bytes = metric_diff_calc(netstat.rx_bytes, nic->rx_bytes, nic->name, "rx bytes");
			nic->rx_bytes = netstat.rx_bytes; // old = new

			//////// SEND PACKETS
			tx_packets = metric_diff_calc(netstat.tx_packets, nic->tx_packets, nic->name, "tx packets");
			nic->tx_packets = netstat.tx_packets; // old = new

			//////// SEND BYTES
			tx_bytes = metric_diff_calc(netstat.tx_bytes, nic->tx_bytes, nic->name, "tx bytes");
			nic->tx_bytes = netstat.tx_bytes; // old = new

			tnet.rx_packets += rx_packets;
			tnet.rx_bytes += rx_bytes;
			tnet.tx_packets += tx_packets;
			tnet.tx_bytes += tx_bytes;
		}
	}
#endif /* !sun */
	TR2(("rx: %" APR_UINT64_T_FMT " rx_bytes: %" APR_UINT64_T_FMT "\n",
					tnet.rx_packets, tnet.rx_bytes));
	TR2(("tx: %" APR_UINT64_T_FMT " tx_bytes: %" APR_UINT64_T_FMT "\n",
					tnet.tx_packets, tnet.tx_bytes));

	if (first)
	{
		pswap = swap, pcpu = cpu;

		/* We want 0s for these metrics on first pass rather
		 * than some possibly huge number that will throw off
		 * the UI graphs.
		 */
		memset(&tdisk, 0, sizeof(tdisk));
		memset(&tnet, 0, sizeof(tnet));
	}
	first = 0;

	gp_smon_to_mmon_set_header(&pkt,GPMON_PKTTYPE_METRICS);

	pkt.u.metrics.mem.total = mem.total;
	pkt.u.metrics.mem.used = mem.used;
	pkt.u.metrics.mem.actual_used = mem.actual_used;
	pkt.u.metrics.mem.actual_free = mem.actual_free;
	pkt.u.metrics.swap.total = swap.total;
	pkt.u.metrics.swap.used = swap.used;
	pkt.u.metrics.swap.page_in = swap.page_in - pswap.page_in;
	pkt.u.metrics.swap.page_out = swap.page_out - pswap.page_out;
	cpu_total_diff = cpu.total - pcpu.total;
	if (cpu_total_diff)
	{
		float cpu_user = calc_diff_percentage(cpu.user, pcpu.user, cpu_total_diff, "cpu.user") + calc_diff_percentage(cpu.nice, pcpu.nice, cpu_total_diff, "cpu.nice");
		float cpu_sys  = calc_diff_percentage(cpu.sys,  pcpu.sys,  cpu_total_diff, "cpu.sys")  + calc_diff_percentage(cpu.wait, pcpu.wait, cpu_total_diff, "cpu.wait");
		float cpu_idle = calc_diff_percentage(cpu.idle, pcpu.idle, cpu_total_diff, "cpu.idle");

#if defined (sun)
		// lib sigar on sun returns total cpu% of 100% times number of cores
		// normalize to make total 100%
		float cpu_norm = round(  (cpu_user + cpu_sys + cpu_idle) / 100.0f  );
		TR2(("cpu_norm = %f", cpu_norm));

		// paranoid check for divide by zero
		if (cpu_norm < .95)
		{
			cpu_norm = 1.0;
		}

		cpu_user /= cpu_norm;
		cpu_sys /= cpu_norm;
		cpu_idle /= cpu_norm;
#endif

		pkt.u.metrics.cpu.user_pct = cpu_user;
		pkt.u.metrics.cpu.sys_pct = cpu_sys;
		pkt.u.metrics.cpu.idle_pct = cpu_idle;
	}
	else
	{
		pkt.u.metrics.cpu.user_pct = 0;
		pkt.u.metrics.cpu.sys_pct = 0;
		pkt.u.metrics.cpu.idle_pct = 0;
	}
	pkt.u.metrics.load_avg.value[0] = (float) loadavg.loadavg[0];
	pkt.u.metrics.load_avg.value[1] = (float) loadavg.loadavg[1];
	pkt.u.metrics.load_avg.value[2] = (float) loadavg.loadavg[2];

	gettimeofday(&currenttime, NULL);
	seconds_duration = subtractTimeOfDay(&g_time_last_reading, &currenttime);

	pkt.u.metrics.disk.ro_rate = (apr_uint64_t)ceil(tdisk.reads/seconds_duration);
	pkt.u.metrics.disk.wo_rate = (apr_uint64_t)ceil(tdisk.writes/seconds_duration);
	pkt.u.metrics.disk.rb_rate = (apr_uint64_t)ceil(tdisk.read_bytes/seconds_duration);
	pkt.u.metrics.disk.wb_rate = (apr_uint64_t)ceil(tdisk.write_bytes/seconds_duration);
	pkt.u.metrics.net.rp_rate = (apr_uint64_t)ceil(tnet.rx_packets/seconds_duration);
	pkt.u.metrics.net.wp_rate = (apr_uint64_t)ceil(tnet.tx_packets/seconds_duration);
	pkt.u.metrics.net.rb_rate = (apr_uint64_t)ceil(tnet.rx_bytes/seconds_duration);
	pkt.u.metrics.net.wb_rate = (apr_uint64_t)ceil(tnet.tx_bytes/seconds_duration);

	g_time_last_reading = currenttime;

	strncpy(pkt.u.metrics.hname, gx.hostname, sizeof(pkt.u.metrics.hname) - 1);
	pkt.u.metrics.hname[sizeof(pkt.u.metrics.hname) - 1] = 0;
	send_smon_to_mon_pkt(sock, &pkt);

	/* save for next time around */
	pswap = swap, pcpu = cpu;
}


/*
* This helper function sums the upper and lower portions of the new 64 bit value
* The upper_sum is the value being used to calculate the upper portion (56 bits worth) of the sum
* The lower_sum is the value being used to calculate the lower portion (8 bits worth) of the sum
* The upper_sum can handle the maximum value up to 4096 max values before it overflows ((2^56)-1) X 4096 = (2^64)-1)
* The lower_sum can handle the maximum value up to 16843009 max values before it overflows ((2^8)-1) X 16843009 = (2^32)-1)
*/
static inline void qexec_average_sum_calc_64_unsigned(apr_uint64_t* upper_sum, apr_uint32_t* lower_sum, apr_uint64_t new_value)
{
	if (new_value == 0){
		return;
	}
	*upper_sum+= (new_value >> 8);
	*lower_sum+= (new_value & 0xFF);
	return;
}

/*
* The below macro calculates the average without loosing precision where n is the number of values summed.
* It should be called after using the above function qexec_average_sum_calc_64_unsigned to sum all the values received.
* It works by first calculating the average of the upper_sum by dividing by n.  This value is then left shifted 8 bits
* back to its original position.  Next before it calculates the average of the lower_sum, it needs to add the remainder
* of the upper_sum divide to the lower sum.  Calculating the remainder is done by doing an upper_sum mod n, shifting it to
* the correct position (left shift 8), and adding it to the lower_sum.  After that it calculates the remaining average
* by using the ROUND_DIVIDE macro and adds it to the average of the upper_sum to get the final average.
*/
#define QEXEC_AVERAGE_CALC_64_UNSIGNED(upper_sum, lower_sum, n) (((upper_sum)/(n))<<8) +(ROUND_DIVIDE(((((upper_sum)%(n))<<8)+((apr_uint64_t)lower_sum)),(n)))

/**
 * This aggregates all the qexec packets from the seg DBs; The aggregated packet is returned in the qexec parameter
 */
static void qexec_agg_packets(qexec_agg_t* qexec_agg, apr_hash_t* pidtab, gpmon_qexec_t* qexec)
{
	apr_hash_index_t* hi;
	pidrec_t* pidrec;
	gpmon_qexec_t* qexec_iter;
	unsigned int i, qexec_counter = 0;
	unsigned int p_metrics_counter = 0;
	unsigned int number_metrics = 0;

	// Sum variables used to calculate the average of the 32 bit numbers
	apr_uint64_t tstart_sum = 0;
	apr_uint64_t tduration_sum = 0;
	apr_uint64_t cpu_pct_sum = 0;
	apr_uint64_t fd_cnt_sum = 0;

	// variables used to sum the lower bytes of 64 bit numbers
	apr_uint32_t mem_resident_lower = 0;
	apr_uint32_t mem_share_lower = 0;
	apr_uint32_t mem_size_lower = 0;
	apr_uint32_t cpu_elapsed_lower = 0;
	apr_uint32_t p_mem_lower = 0;
	apr_uint32_t p_memmax_lower = 0;
	apr_uint32_t rowsout_lower = 0;
	apr_uint32_t rowsout_est_lower = 0;
	apr_uint32_t measures_lower[GPMON_QEXEC_M_COUNT] = {0};

	// Init the qexecs
	memset(qexec, 0, sizeof(gpmon_qexec_t));

	/* Loop through the inner hash table and aggregate the packets */
	for (hi = apr_hash_first(0, qexec_agg->qexecaggtab); hi; hi = apr_hash_next(hi)) {
		void* vptr;
		apr_hash_this(hi, 0, 0, &vptr);
		qexec_iter = vptr;
		//TR0(("packet %d: relation_name %s pkttype %d pnid = %d, key is ssid=%d, tmid=%d, ccnt=%d, nid=%d, segid=%d, pid = %d, status = %d\n", (qexec_counter+1),qexec_iter->relation_name,
		//		qexec_iter->nodeType, qexec_iter->pnid, qexec_iter->key.ssid, qexec_iter->key.tmid, qexec_iter->key.ccnt, qexec_iter->key.hash_key.nid, qexec_iter->key.hash_key.segid, qexec_iter->key.hash_key.pid, qexec_iter->status));
		if (0==qexec_counter) { // Only need to do these once since it should be all the same
			qexec->nodeType = qexec_iter->nodeType;
			qexec->pnid = qexec_iter->pnid;
			// set key
			qexec->key.hash_key.nid = qexec_iter->key.hash_key.nid;
			qexec->key.ssid = qexec_iter->key.ssid;
			qexec->key.tmid = qexec_iter->key.tmid;
			qexec->key.ccnt = qexec_iter->key.ccnt;
			// Set to invalid values
			qexec->key.hash_key.segid = -2;
			qexec->key.hash_key.pid = 0;
			memcpy(&qexec->relation_name, &qexec_iter->relation_name, SCAN_REL_NAME_BUF_SIZE);
			number_metrics = gpdb_getnode_number_metrics(qexec_iter->nodeType);
		}

		qexec->status = MIN(qexec->status, qexec_iter->status); // Put the lowest status we get

		/* fill in _p_metrics sums */
		pidrec = apr_hash_get(pidtab, &qexec_iter->key.hash_key.pid, sizeof(qexec_iter->key.hash_key.pid));
		if (pidrec) {
			cpu_pct_sum += (pidrec->p_metrics.cpu_pct *10000); //multiply by 10000 to preserve decimals.  We will divide by 10000 in the end.
			fd_cnt_sum += pidrec->p_metrics.fd_cnt;
			qexec_average_sum_calc_64_unsigned(&qexec->_p_metrics.mem.resident, &mem_resident_lower, pidrec->p_metrics.mem.resident);
			qexec_average_sum_calc_64_unsigned(&qexec->_p_metrics.mem.share, &mem_share_lower, pidrec->p_metrics.mem.share);
			qexec_average_sum_calc_64_unsigned(&qexec->_p_metrics.mem.size, &mem_size_lower, pidrec->p_metrics.mem.size);
			qexec_average_sum_calc_64_unsigned(&qexec->_cpu_elapsed, &cpu_elapsed_lower, pidrec->cpu_elapsed);
			p_metrics_counter++;
		}

		// Sum the rest of the values
		qexec_average_sum_calc_64_unsigned(&qexec->p_mem, &p_mem_lower, qexec_iter->p_mem);
		qexec_average_sum_calc_64_unsigned(&qexec->p_memmax, &p_memmax_lower, qexec_iter->p_memmax);
		tstart_sum += qexec_iter->tstart;
		tduration_sum += qexec_iter->tduration;
		qexec_average_sum_calc_64_unsigned(&qexec->rowsout, &rowsout_lower, qexec_iter->rowsout);
		qexec_average_sum_calc_64_unsigned(&qexec->rowsout_est, &rowsout_est_lower, qexec_iter->rowsout_est);

		for (i=0;i< number_metrics;i++) {
			qexec_average_sum_calc_64_unsigned(&qexec->measures[i], &measures_lower[i], qexec_iter->measures[i]);
		}
		qexec_counter++;
	}

	// Now create the final qexec packet to send by calculating the averages now that we have gathered all the sums
	// First calculate all the p_metrics if p_metrics_counter is greater than 0
	if (p_metrics_counter) {
		qexec->_p_metrics.cpu_pct = (float)((float)ROUND_DIVIDE(cpu_pct_sum,p_metrics_counter)/(float)10000); //divide by 10000 to get back to float
		qexec->_p_metrics.fd_cnt = (apr_uint32_t) ROUND_DIVIDE(fd_cnt_sum,p_metrics_counter);
		qexec->_p_metrics.mem.resident = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->_p_metrics.mem.resident, mem_resident_lower, p_metrics_counter);
		qexec->_p_metrics.mem.share = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->_p_metrics.mem.share, mem_share_lower, p_metrics_counter);
		qexec->_p_metrics.mem.size = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->_p_metrics.mem.size, mem_size_lower, p_metrics_counter);
	}

	if (qexec_counter) {
		qexec->p_mem = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->p_mem, p_mem_lower, qexec_counter);
		qexec->p_memmax = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->p_memmax, p_memmax_lower, qexec_counter);
		qexec->tstart = (apr_uint32_t) ROUND_DIVIDE(tstart_sum,qexec_counter);
		qexec->tduration = (apr_uint32_t) ROUND_DIVIDE(tduration_sum,qexec_counter);
		qexec->rowsout = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->rowsout, p_memmax_lower, qexec_counter);
		qexec->rowsout_est = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->rowsout_est, rowsout_est_lower, qexec_counter);

		for (i=0;i< number_metrics;i++) {
			qexec->measures[i] = QEXEC_AVERAGE_CALC_64_UNSIGNED(qexec->measures[i], measures_lower[i], qexec_counter);
		}
	}

	TR2( ("Aggregated %d qexec packets\n", qexec_counter));
}
static void gx_gettcpcmd(SOCKET sock, short event, void* arg)
{
	char dump;
	int n, e;
	apr_pool_t* oldpool;
	apr_hash_t* qetab;
	apr_hash_t* qdtab;
	apr_hash_t* pidtab;
	apr_hash_t* segtab;
	apr_hash_t* filereptab;
	if (event & EV_TIMEOUT) // didn't get command from gpmmon, quit
	{
		if(gx.tcp_sock)
		{
			close(gx.tcp_sock);
			gx.tcp_sock=0;
		}
		return;
	}
	apr_hash_t* querysegtab;
	n = recv(sock, &dump, 1, 0);
	if (n == 0)
		gx_exit("peer closed");

	if (n == -1)
		gx_exit("socket error");

	if (dump != 'D')
		gx_exit("bad data");

	TR1(("start dump %c\n", dump));

	qetab = gx.qexectab;
	qdtab = gx.qlogtab;
	pidtab = gx.pidtab;
	segtab = gx.segmenttab;
	filereptab = gx.filereptab;
	querysegtab = gx.querysegtab;

	oldpool = apr_hash_pool_get(qetab);

	/* make new  hashtabs for next cycle */
	{
		apr_pool_t* newpool;
		if (0 != (e = apr_pool_create_alloc(&newpool, gx.pool)))
		{
			gpsmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
		}
		/* qexec hash table */
		gx.qexectab = apr_hash_make(newpool);
		CHECKMEM(gx.qexectab);

		/* qlog hash table */
		gx.qlogtab = apr_hash_make(newpool);
		CHECKMEM(gx.qlogtab);

		/* segment hash table */
		gx.segmenttab = apr_hash_make(newpool);
		CHECKMEM(gx.segmenttab);

		/* filerep hash table */
		gx.filereptab = apr_hash_make(newpool);
		CHECKMEM(gx.filereptab);

		/* queryseg hash table */
		gx.querysegtab = apr_hash_make(newpool);
		CHECKMEM(gx.querysegtab);

		/* pidtab hash table */
		gx.pidtab = apr_hash_make(newpool);
		CHECKMEM(gx.pidtab);
	}

	/* push out a metric of the machine */
	send_machine_metrics(sock);
	send_fsinfo(sock);

	/* push out records */
	{
		void* vptr;
		apr_hash_index_t* hi;
		gp_smon_to_mmon_packet_t* ppkt = 0;
		gp_smon_to_mmon_packet_t localPacketObject;
		pidrec_t* pidrec;
		int count = 0;
		apr_hash_t* query_cpu_table = NULL;

		for (hi = apr_hash_first(0, filereptab); hi; hi = apr_hash_next(hi))
		{
			apr_hash_this(hi, 0, 0, &vptr);
			ppkt = vptr;
			if (ppkt->header.pkttype != GPMON_PKTTYPE_FILEREP)
				continue;

			TR2(("sending magic %x, pkttype %d\n", ppkt->header.magic, ppkt->header.pkttype));
			send_smon_to_mon_pkt(sock, ppkt);
			count++;
		}

		for (hi = apr_hash_first(0, querysegtab); hi; hi = apr_hash_next(hi))
		{
			apr_hash_this(hi, 0, 0, &vptr);
			ppkt = vptr;
			if (ppkt->header.pkttype != GPMON_PKTTYPE_QUERYSEG)
				continue;

			TR2(("sending magic %x, pkttype %d\n", ppkt->header.magic, ppkt->header.pkttype));
			send_smon_to_mon_pkt(sock, ppkt);
			count++;
		}

		for (hi = apr_hash_first(0, segtab); hi; hi = apr_hash_next(hi))
		{
			apr_hash_this(hi, 0, 0, &vptr);
			ppkt = vptr;
			if (ppkt->header.pkttype != GPMON_PKTTYPE_SEGINFO)
				continue;

			/* fill in hostname */
			strncpy(ppkt->u.seginfo.hostname, gx.hostname, sizeof(ppkt->u.seginfo.hostname) - 1);
			ppkt->u.seginfo.hostname[sizeof(ppkt->u.seginfo.hostname) - 1] = 0;

			TR2(("sending magic %x, pkttype %d\n", ppkt->header.magic, ppkt->header.pkttype));
			send_smon_to_mon_pkt(sock, ppkt);
			count++;
		}


		for (hi = apr_hash_first(0, qdtab); hi; hi = apr_hash_next(hi))
		{
			apr_hash_this(hi, 0, 0, &vptr);
			ppkt = vptr;
			if (ppkt->header.pkttype != GPMON_PKTTYPE_QLOG)
				continue;
			TR2(("sending magic %x, pkttype %d\n", ppkt->header.magic, ppkt->header.pkttype));
			send_smon_to_mon_pkt(sock, ppkt);
			count++;
		}

		for (hi = apr_hash_first(0, qetab); hi; hi = apr_hash_next(hi))
		{
			gpmon_qexec_t* qexec;
			gpmon_qexec_t qexec_local;
			void *vptr;

			if (opt.iterator_aggregate) { //Create the aggregated packet to send
				qexec_agg_t* qexec_agg;

				apr_hash_this(hi, 0, 0, &vptr);
				qexec_agg = vptr;
				/* Loop through the inner hash table and aggregate the packets */
				qexec_agg_packets(qexec_agg, pidtab, &qexec_local);
				qexec = &qexec_local;
			} else {
				apr_hash_this(hi, 0, 0, &vptr);
				qexec = vptr;
				/* fill in _p_metrics */
				pidrec = apr_hash_get(pidtab, &qexec->key.hash_key.pid, sizeof(qexec->key.hash_key.pid));
				if (pidrec) {
					qexec->_p_metrics = pidrec->p_metrics;
					qexec->_cpu_elapsed = pidrec->cpu_elapsed;
				} else {
					memset(&qexec->_p_metrics, 0, sizeof(qexec->_p_metrics));
				}
			}

			/* fill in _hname */
			strncpy(qexec->_hname, gx.hostname, sizeof(qexec->_hname) - 1);
			qexec->_hname[sizeof(qexec->_hname) - 1] = 0;

			if (0 == create_qexec_packet(qexec, &localPacketObject)) {
				break;
			}

			TR2(("sending qexec, pkttype %d, size_of_line %d\n", localPacketObject.header.pkttype, localPacketObject.u.qexec_packet.data.size_of_line));
			send_smon_to_mon_pkt(sock, &localPacketObject);
			count++;
		}

		// calculate CPU utilization per query for this machine
		query_cpu_table = apr_hash_make(oldpool);
		CHECKMEM(query_cpu_table);

		// loop through PID's and add to Query CPU Hash Table
		for (hi = apr_hash_first(0, pidtab); hi; hi = apr_hash_next(hi))
		{
			pidrec_t* lookup;

			apr_hash_this(hi, 0, 0, &vptr);
			pidrec = vptr;

			TR2(("tmid %d ssid %d ccnt %d pid %d (CPU elapsed %d CPU Percent %.2f)\n",
				pidrec->query_key.tmid, pidrec->query_key.ssid, pidrec->query_key.ccnt, pidrec->pid,
				pidrec->cpu_elapsed, pidrec->p_metrics.cpu_pct));

			// table is keyed on query key
			lookup = apr_hash_get(query_cpu_table, &pidrec->query_key, sizeof(pidrec->query_key));

			if (lookup)
			{
				// found other pids with same query key so add the metrics to that

				lookup->cpu_elapsed += pidrec->cpu_elapsed;
				lookup->p_metrics.cpu_pct += pidrec->p_metrics.cpu_pct;
			}
			else
			{
				// insert existing pid record into table keyed by query key
				apr_hash_set(query_cpu_table, &pidrec->query_key, sizeof(pidrec->query_key), pidrec);
			}

		}

		// reset packet to 0
		ppkt = &localPacketObject;
		memset(ppkt, 0, sizeof(gp_smon_to_mmon_packet_t));
		gp_smon_to_mmon_set_header(ppkt,GPMON_PKTTYPE_QUERY_HOST_METRICS);

		// add the hostname into the packet for DEBUGGING purposes only.  This is not used
		strncpy(ppkt->u.qlog.user, gx.hostname, sizeof(ppkt->u.qlog.user) - 1);
		ppkt->u.qlog.user[sizeof(ppkt->u.qlog.user) - 1] = 0;

		// loop through the query per cpu table and send the metrics
		for (hi = apr_hash_first(0, query_cpu_table); hi; hi = apr_hash_next(hi))
		{
			apr_hash_this(hi, 0, 0, &vptr);
			pidrec = vptr;

			ppkt->u.qlog.key.tmid = pidrec->query_key.tmid;
			ppkt->u.qlog.key.ssid = pidrec->query_key.ssid;
			ppkt->u.qlog.key.ccnt = pidrec->query_key.ccnt;
			ppkt->u.qlog.cpu_elapsed = pidrec->cpu_elapsed;
			ppkt->u.qlog.p_metrics.cpu_pct = pidrec->p_metrics.cpu_pct;

			TR2(("SEND tmid %d ssid %d ccnt %d (CPU elapsed %d CPU Percent %.2f)\n",
				ppkt->u.qlog.key.tmid, ppkt->u.qlog.key.ssid, ppkt->u.qlog.key.ccnt,
				ppkt->u.qlog.cpu_elapsed, ppkt->u.qlog.p_metrics.cpu_pct));

			send_smon_to_mon_pkt(sock, ppkt);
			count++;
		}

		TR1(("end dump ... sent %d entries\n", count));
	}

	/* get rid of the old pool */
	{
		apr_pool_destroy(oldpool);
	}
	struct timeval tv;
	tv.tv_sec = opt.terminate_timeout;
	tv.tv_usec = 0;
	if (event_add(&gx.tcp_event, &tv)) //reset timeout
        {
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "event_add failed");
        }
	return;
}

static void gx_accept(SOCKET sock, short event, void* arg)
{
	SOCKET nsock;
	gp_smon_to_mmon_packet_t pkt;
	struct sockaddr_in a;
	socklen_t alen = sizeof(a);
	char* p;
	char* q;

	if (event & EV_TIMEOUT)
	{
		if (gx.tcp_sock)
		{
			/* start watching connect request again */
			if (event_add(&gx.listen_event, 0))
			{
				gpsmon_fatal(FLINE, "event_add failed");
			}
			return;
		}
		gpmon_fatal(FLINE, "smon terminates due to no requests come after %" FMT64 " seconds\n", opt.terminate_timeout);
	}

	if (0 == (event & EV_READ))
		return;

	if (-1 == (nsock = accept(sock, (void*) &a, &alen)))
	{
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "accept failed");
		return;
	}

	TR1(("accepted\n"));

	/* we do this one at a time */
	if (gx.tcp_sock)
	{
		gpmon_warning(FLINE, "cannot accept new connection before old one dies");
		close(nsock);
		return;
	}

	p = (char*) &pkt;
	q = p + sizeof(pkt);
	while (p < q)
	{
		int n = recv(nsock, p, q - p, 0);
		if (n == -1)
		{
			gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "recv failed");
			close(nsock);
			return;
		}
		p += n;
	}

	if (0 != gpmon_ntohpkt(pkt.header.magic, pkt.header.version, pkt.header.pkttype))
	{
		close(nsock);
		return;
	}

	if (pkt.header.pkttype != GPMON_PKTTYPE_HELLO)
	{
		close(nsock);
		return;
	}

	if (pkt.u.hello.signature != gx.signature)
	{
		gx_exit("bad signature... maybe a new gpmmon has started");
	}

	/* echo the hello */
	pkt.u.hello.pid = getpid();
	TR2(("accepted pkt.magic = %x\n", (int) pkt.header.magic));
	send_smon_to_mon_pkt(nsock, &pkt);

	struct timeval tv;
	tv.tv_sec = opt.terminate_timeout;
	tv.tv_usec = 0;
	event_set(&gx.tcp_event, nsock, EV_READ | EV_PERSIST | EV_TIMEOUT, gx_gettcpcmd, 0);
	if (event_add(&gx.tcp_event, &tv))
	{
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "event_add failed");
		close(nsock);
		return;
	}
	gx.tcp_sock = nsock;
	TR1(("connection established --------------------- \n"));
}

/* got a packet from peer. put it in the queue */
static void gx_recvqlog(gpmon_packet_t* pkt)
{
	gpmon_qlog_t* p;
	gp_smon_to_mmon_packet_t* rec;

	if (pkt->pkttype != GPMON_PKTTYPE_QLOG)
		gpsmon_fatal(FLINE, "assert failed; expected pkttype qlog");

	p = &pkt->u.qlog;
	TR2(("Received qlog packet for query %d-%d-%d.  Status now %d\n", p->key.tmid, p->key.ssid, p->key.ccnt, p->status));
	rec = apr_hash_get(gx.qlogtab, &p->key, sizeof(p->key));
	if (rec)
	{
		memcpy(&rec->u.qlog, p, sizeof(*p));
	}
	else
	{
		rec = gx_pkt_to_smon_to_mmon(apr_hash_pool_get(gx.qlogtab), pkt);
		apr_hash_set(gx.qlogtab, &rec->u.qlog.key, sizeof(rec->u.qlog.key), rec);
	}
}

static void update_max_value(apr_uint32_t* total, apr_uint32_t newdata)
{
	if (newdata > *total)
	{
		*total = newdata;
	}
}

static void update_avg_value(apr_uint32_t totalcount, apr_uint32_t* totalavg, apr_uint32_t newcount, apr_uint32_t newavg)
{
	//OldAverage*oldCount + AverageInNewPacket*countInNewPacket
	//----------------------------------------------------------
	// OldCount + countInNewPacket

	*totalavg = (apr_uint32_t)(( (*totalavg * (double)totalcount) + (newavg * (double)newcount) ) / ((double)(totalcount+newcount)));
}

static void update_count_value(apr_uint32_t* total, apr_uint32_t newdata)
{
	*total += newdata;
}


static void accumulate_filerep_primary_data(gpmon_filerep_primarystats_s* total, gpmon_filerep_primarystats_s* newdata)
{
	// ALWAYS UPDATE AVERAGES BEFORE COUNTS ... AVERAGE UPDATE USES COUNT

	// write_syscall_size_avg
	update_avg_value(total->write_syscall_count, &total->write_syscall_size_avg,
			 newdata->write_syscall_count, newdata->write_syscall_size_avg);

	// write_syscall_size_max
	update_max_value(&total->write_syscall_size_max, newdata->write_syscall_size_max);

	// write_syscall_time_avg
	update_avg_value(total->write_syscall_count, &total->write_syscall_time_avg,
			 newdata->write_syscall_count, newdata->write_syscall_time_avg);

	// write_syscall_time_max
	update_max_value(&total->write_syscall_time_max, newdata->write_syscall_time_max);

	// write_syscall_count
	update_count_value(&total->write_syscall_count, newdata->write_syscall_count);

	// fsync_syscall_time_avg
	update_avg_value(total->fsync_syscall_count, &total->fsync_syscall_time_avg,
			 newdata->fsync_syscall_count, newdata->fsync_syscall_time_avg);

	// fsync_syscall_time_max
	update_max_value(&total->fsync_syscall_time_max, newdata->fsync_syscall_time_max);

	// fsync_syscall_count
	update_count_value(&total->fsync_syscall_count, newdata->fsync_syscall_count);

	// write_shmem_size_avg
	update_avg_value(total->write_shmem_count, &total->write_shmem_size_avg,
			 newdata->write_shmem_count, newdata->write_shmem_size_avg);

	// write_shmem_size_max
	update_max_value(&total->write_shmem_size_max, newdata->write_shmem_size_max);

	// write_shmem_time_avg
	update_avg_value(total->write_shmem_count, &total->write_shmem_time_avg,
			 newdata->write_shmem_count, newdata->write_shmem_time_avg);

	// write_shmem_time_max
	update_max_value(&total->write_shmem_time_max, newdata->write_shmem_time_max);

	// write_shmem_count
	update_count_value(&total->write_shmem_count, newdata->write_shmem_count);

	// fsync_shmem_time_avg
	update_avg_value(total->fsync_shmem_count, &total->fsync_shmem_time_avg,
			 newdata->fsync_shmem_count, newdata->fsync_shmem_time_avg);

	// fsync_shmem_time_max
	update_max_value(&total->fsync_shmem_time_max, newdata->fsync_shmem_time_max);

	// fsync_shmem_count
	update_count_value(&total->fsync_shmem_count, newdata->fsync_shmem_count);

	// roundtrip_fsync_msg_time_avg
	update_avg_value(total->roundtrip_fsync_msg_count, &total->roundtrip_fsync_msg_time_avg,
			 newdata->roundtrip_fsync_msg_count, newdata->roundtrip_fsync_msg_time_avg);

	// roundtrip_fsync_msg_time_max
	update_max_value(&total->roundtrip_fsync_msg_time_max, newdata->roundtrip_fsync_msg_time_max);

	// roundtrip_fsync_msg_count
	update_count_value(&total->roundtrip_fsync_msg_count, newdata->roundtrip_fsync_msg_count);

	// roundtrip_test_msg_time_avg
	update_avg_value(total->roundtrip_test_msg_count, &total->roundtrip_test_msg_time_avg,
			 newdata->roundtrip_test_msg_count, newdata->roundtrip_test_msg_time_avg);

	// roundtrip_test_msg_time_max
	update_max_value(&total->roundtrip_test_msg_time_max, newdata->roundtrip_test_msg_time_max);

	// roundtrip_test_msg_count
	update_count_value(&total->roundtrip_test_msg_count, newdata->roundtrip_test_msg_count);
}

static void accumulate_filerep_mirror_data(gpmon_filerep_mirrorstats_s* total, gpmon_filerep_mirrorstats_s* newdata)
{
	// ALWAYS UPDATE AVERAGES BEFORE COUNTS ... AVERAGE UPDATE USES COUNT

	// write_syscall_size_avg
	update_avg_value(total->write_syscall_count, &total->write_syscall_size_avg,
			 newdata->write_syscall_count, newdata->write_syscall_size_avg);

	// write_syscall_size_max
	update_max_value(&total->write_syscall_size_max, newdata->write_syscall_size_max);

	// write_syscall_time_avg;
	update_avg_value(total->write_syscall_count, &total->write_syscall_time_avg,
			 newdata->write_syscall_count, newdata->write_syscall_time_avg);

	// write_syscall_time_max
	update_max_value(&total->write_syscall_time_max, newdata->write_syscall_time_max);

	// write_syscall_count
	update_count_value(&total->write_syscall_count, newdata->write_syscall_count);

	// fsync_syscall_time_avg;
	update_avg_value(total->fsync_syscall_count, &total->fsync_syscall_time_avg,
			 newdata->fsync_syscall_count, newdata->fsync_syscall_time_avg);

	// fsync_syscall_time_max
	update_max_value(&total->fsync_syscall_time_max, newdata->fsync_syscall_time_max);

	// fsync_syscall_count
	update_count_value(&total->fsync_syscall_count, newdata->fsync_syscall_count);
}

static void accumulate_filerep_data_in_packet(gpmon_filerepinfo_t* total, gpmon_filerepinfo_t* newdata)
{
	if (total->key.isPrimary != newdata->key.isPrimary)
	{
		gpmon_warning(FLINE, "filerep unexpected key mismatch");
		return;
	}

	total->elapsedTime_secs += newdata->elapsedTime_secs;

	if (total->key.isPrimary)
	{
		accumulate_filerep_primary_data(&total->stats.primary, &newdata->stats.primary);
	}
	else
	{
		accumulate_filerep_mirror_data(&total->stats.mirror, &newdata->stats.mirror);
	}
}

static void gx_recvfilerep(gpmon_packet_t* pkt)
{
	gpmon_filerepinfo_t* p;
	gp_smon_to_mmon_packet_t* rec = NULL;

	if (pkt->pkttype != GPMON_PKTTYPE_FILEREP)
		gpsmon_fatal(FLINE, "assert failed; expected pkttype filerep");

	p = &pkt->u.filerepinfo;

	TR2(("Received filerep packet primary %s:%d mirror %s:%d isPrimary(%d)\n",
		p->key.dkey.primary_hostname, p->key.dkey.primary_port, p->key.dkey.mirror_hostname, p->key.dkey.mirror_port));

	rec = apr_hash_get(gx.filereptab, &p->key, sizeof(p->key));
	if (rec)
	{
		accumulate_filerep_data_in_packet(&rec->u.filerepinfo, &pkt->u.filerepinfo);
	}
	else
	{
		rec = gx_pkt_to_smon_to_mmon(apr_hash_pool_get(gx.filereptab), pkt);
		apr_hash_set(gx.filereptab, &rec->u.filerepinfo.key, sizeof(rec->u.filerepinfo.key), rec);
	}
}

static void gx_recvsegment(gpmon_packet_t* pkt)
{
	gpmon_seginfo_t* p;
	gp_smon_to_mmon_packet_t* rec;

	if (pkt->pkttype != GPMON_PKTTYPE_SEGINFO)
		gpsmon_fatal(FLINE, "assert failed; expected pkttype segment");

	p = &pkt->u.seginfo;

	TR2(("Received segment packet for dbid %d (dynamic_memory_used, dynamic_memory_available) (%llu %llu)\n", p->dbid, p->dynamic_memory_used, p->dynamic_memory_available));

	rec = apr_hash_get(gx.segmenttab, &p->dbid, sizeof(p->dbid));
	if (rec)
	{
		memcpy(&rec->u.seginfo, p, sizeof(*p));
	}
	else
	{
		rec = gx_pkt_to_smon_to_mmon(apr_hash_pool_get(gx.segmenttab), pkt);
		apr_hash_set(gx.segmenttab, &rec->u.seginfo.dbid, sizeof(rec->u.seginfo.dbid), rec);
	}
}

#define SINGLE_METRIC_BUFSZ 100
typedef char single_metric_string[SINGLE_METRIC_BUFSZ];

/**
* write the iterator table row the qexec packet.
* @note This function was moved from gpmon_agg.c
* @return 1 if success, 0 if failure
*/
static apr_uint32_t create_qexec_packet(const gpmon_qexec_t* qexec, gp_smon_to_mmon_packet_t* pkt)
{
	single_metric_string metrics[GPMON_QEXEC_M_DBCOUNT];
	char qexec_tstamp[GPMON_DATE_BUF_SIZE];
	int i, len = 0;
	const char* measure_name = NULL;
	const char* unit_name = NULL;
	apr_status_t r = APR_SUCCESS;
	char phase[4] = " ";
	char tname[16] = "Name";
	char nowstr[GPMON_DATE_BUF_SIZE];

	gpmon_datetime_rounded(time(NULL), nowstr);

	if( PMNT_MAXIMUM_ENUM <= qexec->nodeType) {
		TR0( ("Error !!!! node type out of range: %d >= %d\n", qexec->nodeType, PMNT_MAXIMUM_ENUM));
		return 0;
	}

	// Copy over needed values
	memcpy(&pkt->u.qexec_packet.data.key, &qexec->key, sizeof(gpmon_qexeckey_t));
	pkt->u.qexec_packet.data.measures_rows_in = qexec->measures[GPMON_QEXEC_M_ROWSIN];
	pkt->u.qexec_packet.data._cpu_elapsed = qexec->_cpu_elapsed;
	pkt->u.qexec_packet.data.rowsout = qexec->rowsout;

	for (i = 0; i < GPMON_QEXEC_M_DBCOUNT; i++) {
		if (r == APR_SUCCESS) {
			/* after this function returns failure 1 time don't try again for remaining numbers in array */
			/* GPMON_QEXEC_M_DBCOUNT is number of metrics output to the DB; GPMON_QEXEC_M_COUNT is number of metrics in the packets */

			if (i < GPMON_QEXEC_M_COUNT) {
				r = gpdb_getnode_metricinfo(qexec->nodeType, i, &measure_name, &unit_name);
			} else {
				r = APR_NOTFOUND;
			}
		}

		if (r == APR_SUCCESS) {
			snprintf(metrics[i], SINGLE_METRIC_BUFSZ, "%s|%s|%" FMT64 "|0", measure_name, unit_name, qexec->measures[i]);
		} else {
			snprintf(metrics[i], SINGLE_METRIC_BUFSZ, "%s", "null|null|null|null");
		}
	}

	if (opt.iterator_aggregate) { // leave the segid blank
		len = snprintf(qexec_smon_temp_line, QEXEC_MAX_ROW_BUF_SIZE,
			"%s|%d|%d|%d|null|%d|%d|%d|%s|%s|%s|%s|%d|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%.2f|%s|%" FMT64 "|%" FMT64 "|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s" "|%s|%s",
			nowstr,
			qexec->key.tmid,
			qexec->key.ssid,
			qexec->key.ccnt,
			qexec->key.hash_key.pid,
			qexec->key.hash_key.nid,
			qexec->pnid,
			qexec->_hname,
			gpdb_getnodename(qexec->nodeType),
			gpdb_getnodestatus(qexec->status),
			gpmon_datetime((time_t)qexec->tstart, qexec_tstamp),
			qexec->tduration,
			qexec->p_mem,
			qexec->p_memmax,
			qexec->_p_metrics.mem.size,
			qexec->_p_metrics.mem.resident,
			qexec->_p_metrics.mem.share,
			qexec->_cpu_elapsed,
			qexec->_p_metrics.cpu_pct,
			phase,
			qexec->rowsout,
			qexec->rowsout_est,
			metrics[0], metrics[1], metrics[2], metrics[3],
			metrics[4], metrics[5], metrics[6], metrics[7],
			metrics[8], metrics[9], metrics[10], metrics[11],
			metrics[12], metrics[13], metrics[14], metrics[15],
			tname,
			qexec->relation_name);

	} else {
		len = snprintf(qexec_smon_temp_line, QEXEC_MAX_ROW_BUF_SIZE,
			"%s|%d|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s|%d|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%" FMT64 "|%.2f|%s|%" FMT64 "|%" FMT64 "|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s" "|%s|%s",
			nowstr,
			qexec->key.tmid,
			qexec->key.ssid,
			qexec->key.ccnt,
			qexec->key.hash_key.segid,
			qexec->key.hash_key.pid,
			qexec->key.hash_key.nid,
			qexec->pnid,
			qexec->_hname,
			gpdb_getnodename(qexec->nodeType),
			gpdb_getnodestatus(qexec->status),
			gpmon_datetime((time_t)qexec->tstart, qexec_tstamp),
			qexec->tduration,
			qexec->p_mem,
			qexec->p_memmax,
			qexec->_p_metrics.mem.size,
			qexec->_p_metrics.mem.resident,
			qexec->_p_metrics.mem.share,
			qexec->_cpu_elapsed,
			qexec->_p_metrics.cpu_pct,
			phase,
			qexec->rowsout,
			qexec->rowsout_est,
			metrics[0], metrics[1], metrics[2], metrics[3],
			metrics[4], metrics[5], metrics[6], metrics[7],
			metrics[8], metrics[9], metrics[10], metrics[11],
			metrics[12], metrics[13], metrics[14], metrics[15],
			tname,
			qexec->relation_name);
	}


	pkt->u.qexec_packet.line = qexec_smon_temp_line; //Set the line to the temp memory
	if (len < 0) {
		gpmon_warning(FLINE, "iterator line could not copy sprintf returned -1");
		return 0;
	}
	if ((len + 1) == QEXEC_MAX_ROW_BUF_SIZE) {
		gpmon_warning(FLINE, "iterator line too long ... ignored: %s", pkt->u.qexec_packet.line);
		return 0;
	}

	pkt->u.qexec_packet.data.size_of_line = len + 1; // Set the line length
	gp_smon_to_mmon_set_header(pkt,GPMON_PKTTYPE_QEXEC);
	return 1;
}

static void extract_segments_exec(gpmon_packet_t* pkt)
{
	gpmon_qexec_t				*p;
	gp_smon_to_mmon_packet_t	*rec;
	gpmon_query_seginfo_key_t	qseg_key;
	pidrec_t					*pidrec;

	if (pkt->pkttype != GPMON_PKTTYPE_QEXEC)
		gpsmon_fatal(FLINE, "assert failed; expected pkttype qexec");

	p = &pkt->u.qexec;
	qseg_key.qkey.tmid = p->key.tmid;
	qseg_key.qkey.ssid = p->key.ssid;
	qseg_key.qkey.ccnt = p->key.ccnt;
	qseg_key.segid = p->key.hash_key.segid;

	rec = apr_hash_get(gx.querysegtab, &qseg_key, sizeof(qseg_key));
	pidrec = apr_hash_get(gx.pidtab, &p->key.hash_key.pid, sizeof(p->key.hash_key.pid));
	ASSERT(pidrec);

	if (rec)
	{
		rec->u.queryseg.sum_cpu_elapsed += pidrec->cpu_elapsed;
		rec->u.queryseg.sum_measures_rows_in += p->measures[GPMON_QEXEC_M_ROWSIN];
		if (p->key.hash_key.segid == -1 && p->key.hash_key.nid == 1)
		{
			rec->u.queryseg.final_rowsout = p->rowsout;
		}
	}
	else
	{
		rec = apr_palloc(apr_hash_pool_get(gx.querysegtab),
					sizeof(gp_smon_to_mmon_packet_t));
		CHECKMEM(rec);
		gp_smon_to_mmon_set_header(rec, GPMON_PKTTYPE_QUERYSEG);
		rec->u.queryseg.key = qseg_key;
		if (p->key.hash_key.segid == -1 && p->key.hash_key.nid == 1)
		{
			rec->u.queryseg.final_rowsout = p->rowsout;
		}
		else
		{
			rec->u.queryseg.final_rowsout = -1;
		}
		rec->u.queryseg.sum_cpu_elapsed = pidrec->cpu_elapsed;
		rec->u.queryseg.sum_measures_rows_in = p->measures[GPMON_QEXEC_M_ROWSIN];
		apr_hash_set(gx.querysegtab, &rec->u.queryseg.key, sizeof(rec->u.queryseg.key), rec);
	}
}

static void gx_recvqexec(gpmon_packet_t* pkt)
{
	gpmon_qexec_t* p;
	gpmon_qexec_t* rec;

	if (pkt->pkttype != GPMON_PKTTYPE_QEXEC)
		gpsmon_fatal(FLINE, "assert failed; expected pkttype qexec");

	p = &pkt->u.qexec;
	if (opt.do_ignore_qexec_packet)
	{
		get_pid_metrics(p->key.hash_key.pid,
						p->key.tmid,
						p->key.ssid,
						p->key.ccnt);
		// Store some aggregated information somewhere for metrics in
		// queries_* tables, like cpu_elapsed, rows_out, and etc.
		extract_segments_exec(pkt);
		// We don't call gpmon_warning here because the number of
		// packet is big, and we would make log boating.
		return;
	}

	/* fill in the tstart / tduration */
	if (p->tstart == 0) {
		p->tstart = gx.now;
		p->tduration = 0;
	} else {
		p->tduration = gx.now - p->tstart;
	}

	/* If the aggregate flag is set, we need to store the qexec by query id and not by seg db in an outer hash table*/
	if (opt.iterator_aggregate) {
		qexec_agg_t* rec_agg;
		qexec_agg_hash_key_t key;
		/* Set the aggregate key */
		key.ccnt = p->key.ccnt;
		key.tmid = p->key.tmid;
		key.nid = p->key.hash_key.nid;
		key.ssid = p->key.ssid;
		rec_agg = apr_hash_get(gx.qexectab, &key, sizeof(qexec_agg_hash_key_t));
		if (rec_agg) {
			/* agg packet exists, overwrite it*/
			rec = apr_hash_get(rec_agg->qexecaggtab, &p->key, sizeof(p->key));
			if (rec) {
				/* overwrite an old qexec*/
				memcpy(rec, p, sizeof(*p));
			} else {
				/* insert a new qexec */
				rec = apr_palloc(apr_hash_pool_get(gx.qexectab), sizeof(*p));
				CHECKMEM(rec);
				memcpy(rec, p, sizeof(*p));
				apr_hash_set(rec_agg->qexecaggtab, &rec->key, sizeof(rec->key), rec);
			}
		} else {
			/* create a new qexec agg*/
			rec_agg = apr_palloc(apr_hash_pool_get(gx.qexectab), sizeof(*p));
			CHECKMEM(rec_agg);
			memcpy(&rec_agg->key, &key, sizeof(qexec_agg_hash_key_t));

			/* Make the sub hash table */
			rec_agg->qexecaggtab = apr_hash_make(apr_hash_pool_get(gx.qexectab));
			CHECKMEM(rec_agg->qexecaggtab);

			/* insert a new qexec */
			rec = apr_palloc(apr_hash_pool_get(gx.qexectab), sizeof(*p));
			CHECKMEM(rec);
			memcpy(rec, p, sizeof(*p));
			apr_hash_set(rec_agg->qexecaggtab, &rec->key, sizeof(rec->key), rec);

			/* Insert the new qexec agg */
			apr_hash_set(gx.qexectab, &rec_agg->key, sizeof(rec_agg->key), rec_agg);
		}
	} else {
		rec = apr_hash_get(gx.qexectab, &p->key, sizeof(p->key));
		if (rec) {
			/* overwrite an old qexec */
			memcpy(rec, p, sizeof(*p));
		} else {
			/* insert a new qexec */
			rec = apr_palloc(apr_hash_pool_get(gx.qexectab), sizeof(*p));
			CHECKMEM(rec);
			memcpy(rec, p, sizeof(*p));
			apr_hash_set(gx.qexectab, &rec->key, sizeof(rec->key), rec);
		}
	}
}

/* callback from libevent when a udp socket is ready to be read.
 This function determines the packet type, then calls
 gx_recvqlog() or gx_recvqexec().
 */
static void gx_recvfrom(SOCKET sock, short event, void* arg)
{
	gpmon_packet_t pkt;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int n;

	if (!(event & EV_READ))
		return;

	n = recvfrom(sock, &pkt, sizeof(pkt), 0, (void*) &addr, &addrlen);
	if (n == -1)
	{
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), "recvfrom failed");
		return;
	}

	if (n != sizeof(pkt))
	{
		gpmon_warning(FLINE, "bad packet (length %d)", n);
		return;
	}

	/* do some packet marshaling */
	if (0 != gpmon_ntohpkt(pkt.magic, pkt.version, pkt.pkttype))
	{
		gpmon_warning(FLINE, "error with packet marshaling");
		return;
	}

	/* process the packet */
	switch (pkt.pkttype)
	{
	case GPMON_PKTTYPE_QLOG:
		gx_recvqlog(&pkt);
		break;
	case GPMON_PKTTYPE_SEGINFO:
		gx_recvsegment(&pkt);
		break;
	case GPMON_PKTTYPE_QEXEC:
		gx_recvqexec(&pkt);
		break;
	case GPMON_PKTTYPE_FILEREP:
		gx_recvfilerep(&pkt);
		break;
	default:
		gpmon_warning(FLINE, "unexpected packet type %d", pkt.pkttype);
		return;
	}
}

static void setup_tcp(void)
{
	SOCKET sock = 0;

	struct addrinfo hints;
	struct addrinfo *addrs, *rp;
	int  s;
	char service[32];

	/*
	 * we let the system pick the TCP port here so we don't have to
	 * manage port resources ourselves.
	 */
	snprintf(service,32,"%d",gx.port);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; 	/* TCP socket */
	hints.ai_flags = AI_PASSIVE;    	/* For wildcard IP address */
	hints.ai_protocol = 0;          	/* Any protocol */

	s = getaddrinfo(NULL, service, &hints, &addrs);
	if (s != 0)
		gpsmon_fatalx(FLINE, 0, "getaddrinfo says %s",gai_strerror(s));

	/*
	 * getaddrinfo() returns a list of address structures,
	 * one for each valid address and family we can use.
	 *
	 * Try each address until we successfully bind.
	 * If socket (or bind) fails, we (close the socket
	 * and) try the next address.  This can happen if
	 * the system supports IPv6, but IPv6 is disabled from
	 * working, or if it supports IPv6 and IPv4 is disabled.
	 */

	/*
	 * If there is both an AF_INET6 and an AF_INET choice,
	 * we prefer the AF_INET6, because on UNIX it can receive either
	 * protocol, whereas AF_INET can only get IPv4.  Otherwise we'd need
	 * to bind two sockets, one for each protocol.
	 *
	 * Why not just use AF_INET6 in the hints?  That works perfect
	 * if we know this machine supports IPv6 and IPv6 is enabled,
	 * but we don't know that.
	 */

#ifdef HAVE_IPV6
	if (addrs->ai_family == AF_INET && addrs->ai_next != NULL && addrs->ai_next->ai_family == AF_INET6)
	{
		/*
		 * We got both an INET and INET6 possibility, but we want to prefer the INET6 one if it works.
		 * Reverse the order we got from getaddrinfo so that we try things in our preferred order.
		 * If we got more possibilities (other AFs??), I don't think we care about them, so don't
		 * worry if the list is more that two, we just rearrange the first two.
		 */
		struct addrinfo *temp = addrs->ai_next; 	/* second node */
		addrs->ai_next = addrs->ai_next->ai_next; 	/* point old first node to third node if any */
		temp->ai_next = addrs;   					/* point second node to first */
		addrs = temp;								/* start the list with the old second node */
	}
#endif

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		int on = 1;
		struct linger linger;
		/*
		 * getaddrinfo gives us all the parameters for the socket() call
		 * as well as the parameters for the bind() call.
		 */

		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock == -1)
			continue;

		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*) &on, sizeof(on));
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &on, sizeof(on));
		linger.l_onoff = 1;
		linger.l_linger = 5;
		setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*) &linger, sizeof(linger));


		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;              /* Success */

		close(sock);
	}

	if (rp == NULL)
	{               /* No address succeeded */
		gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno),
						"unable to bind tcp socket");
	}

	freeaddrinfo(addrs);

	if (-1 == listen(sock, 5))
	{
		gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "listen failed");
	}

	/* save it */
	gx.listen_sock = sock;
	TR1(("TCP port %d opened\n", gx.port));

	/* set up listen event, and associate with our event_base */
	event_set(&gx.listen_event, sock, EV_READ | EV_PERSIST | EV_TIMEOUT, gx_accept, 0);

	struct timeval accept_timeout;
	accept_timeout.tv_sec = opt.terminate_timeout;
	accept_timeout.tv_usec = 0;

	/* start watching this event */
	if (event_add(&gx.listen_event, opt.terminate_timeout ? &accept_timeout : 0))
	{
		gpsmon_fatal(FLINE, "event_add failed");
	}

}

static void setup_udp()
{
	SOCKET sock = 0;

	struct addrinfo hints;
	struct addrinfo *addrs, *rp;
	int  s;
	char service[32];

	/*
	 * we let the system pick the TCP port here so we don't have to
	 * manage port resources ourselves.
	 */
	snprintf(service,32,"%d",gx.port);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; 	/* UDP socket */
	hints.ai_flags = AI_PASSIVE;    	/* For wildcard IP address */
	hints.ai_protocol = 0;          	/* Any protocol */

	s = getaddrinfo(NULL, service, &hints, &addrs);
	if (s != 0)
		gpsmon_fatalx(FLINE, 0, "getaddrinfo says %s",gai_strerror(s));

	/*
	 * getaddrinfo() returns a list of address structures,
	 * one for each valid address and family we can use.
	 *
	 * Try each address until we successfully bind.
	 * If socket (or bind) fails, we (close the socket
	 * and) try the next address.  This can happen if
	 * the system supports IPv6, but IPv6 is disabled from
	 * working, or if it supports IPv6 and IPv4 is disabled.
	 */

	/*
	 * If there is both an AF_INET6 and an AF_INET choice,
	 * we prefer the AF_INET6, because on UNIX it can receive either
	 * protocol, whereas AF_INET can only get IPv4.  Otherwise we'd need
	 * to bind two sockets, one for each protocol.
	 *
	 * Why not just use AF_INET6 in the hints?  That works perfect
	 * if we know this machine supports IPv6 and IPv6 is enabled,
	 * but we don't know that.
	 */

#ifdef HAVE_IPV6
	if (addrs->ai_family == AF_INET && addrs->ai_next != NULL && addrs->ai_next->ai_family == AF_INET6)
	{
		/*
		 * We got both an INET and INET6 possibility, but we want to prefer the INET6 one if it works.
		 * Reverse the order we got from getaddrinfo so that we try things in our preferred order.
		 * If we got more possibilities (other AFs??), I don't think we care about them, so don't
		 * worry if the list is more that two, we just rearrange the first two.
		 */
		struct addrinfo *temp = addrs->ai_next; 	/* second node */
		addrs->ai_next = addrs->ai_next->ai_next; 	/* point old first node to third node if any */
		temp->ai_next = addrs;   					/* point second node to first */
		addrs = temp;								/* start the list with the old second node */
	}
#endif

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		/*
		 * getaddrinfo gives us all the parameters for the socket() call
		 * as well as the parameters for the bind() call.
		 */

		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock == -1)
			continue;

		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;              /* Success */

		close(sock);
	}

	if (rp == NULL)
	{               /* No address succeeded */
		gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno),
						"unable to bind udp socket");
	}

	/* save it */
	gx.udp_sock = sock;

	freeaddrinfo(addrs);

	/* set up udp event */
	event_set(&gx.udp_event, gx.udp_sock, EV_READ | EV_PERSIST, gx_recvfrom, 0);

	/* start watching this event */
	if (event_add(&gx.udp_event, 0))
	{
		gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_add failed");
	}
}

// return pointer to trimmed hostname
// pass in an allocated buffer and its size
// return NULL if fails to get name
char* get_dca_hostname(char* buffer, size_t buffer_size){

	char* trimmed_hostname;

	FILE* fd = fopen(APPLIANCE_HOSTNAME_FILE, "r");
	if (!fd){
		gpmon_warningx(FLINE, 0, "Cannot open file %s", APPLIANCE_HOSTNAME_FILE);
		return NULL;
	}

	size_t bytes = fread(buffer, 1, buffer_size, fd);
	if (bytes < 1){
		gpmon_warningx(FLINE, 0, "Cannot read hostname from file %s", APPLIANCE_HOSTNAME_FILE);
		return NULL;
	}

	if (bytes >= buffer_size){
		gpmon_warningx(FLINE, 0, "hostname in file %s is too long", APPLIANCE_HOSTNAME_FILE);
		return NULL;
	}

	// ensure we have a null terminated string regardless
	buffer[buffer_size-1] = 0;

	trimmed_hostname = gpmon_trim(buffer);

	fclose(fd);

	return trimmed_hostname;
}


static const char* get_and_allocate_hostname()
{
	char hname[256] = { 0 };

	if (gethostname(hname, sizeof(hname) - 1))
	{
		gx.hostname = strdup("unknown");
		gpmon_warningx(FLINE, 0, "gethostname failed");
	}
	else
	{
		hname[sizeof(hname) - 1] = 0;
		gx.hostname = strdup(hname);
	}

	return gx.hostname;
}

static void setup_gx(int port, apr_int64_t signature)
{
	int e;
	apr_pool_t* subpool;

	/* set up pool */
	if (0 != (e = apr_pool_create_alloc(&gx.pool, 0)))
	{
		gpsmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
	}

	/* set up port, event */
	gx.port = port;
	gx.signature = signature;
	if (!event_init())
	{
		gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_init failed");
	}

	if (0 != (e = apr_pool_create_alloc(&subpool, gx.pool)))
	{
		gpsmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
	}

	/* qexec hash table */
	gx.qexectab = apr_hash_make(subpool);
	CHECKMEM(gx.qexectab);

	/* qlog hash table */
	gx.qlogtab = apr_hash_make(subpool);
	CHECKMEM(gx.qlogtab);

	/* segment hash table */
	gx.segmenttab = apr_hash_make(subpool);
	CHECKMEM(gx.segmenttab);

	/* filerep hash table */
	gx.filereptab = apr_hash_make(subpool);
	CHECKMEM(gx.filereptab);

	/* queryseg hash table */
	gx.querysegtab = apr_hash_make(subpool);
	CHECKMEM(gx.querysegtab);

	/* pidtab */
	gx.pidtab = apr_hash_make(subpool);
	CHECKMEM(gx.pidtab);

	/* device metrics hashes */
	net_devices = apr_hash_make(gx.pool);
	CHECKMEM(net_devices);
	disk_devices = apr_hash_make(gx.pool);
	CHECKMEM(disk_devices);

}

static void setup_sigar(void)
{
	sigar_file_system_list_t sigar_fslist;
	sigar_net_interface_list_t sigar_netlist;
	int i, e, cnt;
	int do_destroy = 0;

	/* initialize sigar */
	if (0 != (e = sigar_open(&gx.sigar)))
	{
		gpsmon_fatalx(FLINE, e, "sigar_open failed");
	}

	TR2(("sigar initialized\n"));
	do_destroy = 1;
	if (0 != sigar_net_interface_list_get(gx.sigar, &sigar_netlist))
	{
		memset(&sigar_netlist, 0, sizeof(sigar_netlist));
		do_destroy = 0;
	}
	gx.netlist = apr_pcalloc(gx.pool, sizeof(const char*) * (1
			+ sigar_netlist.number));
	CHECKMEM(gx.netlist);
	for (i = 0; i < sigar_netlist.number; i++)
	{
		gx.netlist[i] = apr_pstrdup(gx.pool, sigar_netlist.data[i]);
		CHECKMEM(gx.netlist[i]);
		TR2(("sigar net %d: %s\n", i, gx.netlist[i]));
	}
	if (do_destroy)
		sigar_net_interface_list_destroy(gx.sigar, &sigar_netlist);

	do_destroy = 1;
	if (0 != sigar_file_system_list_get(gx.sigar, &sigar_fslist))
	{
		memset(&sigar_fslist, 0, sizeof(sigar_fslist));
		do_destroy = 0;
	}
	cnt = 0;
	TR2(("sigar fsnumber: %d\n", sigar_fslist.number));
	for (i = 0; i < sigar_fslist.number; i++)
	{
		if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK)
		{
			TR2(("sigar cnt: %d\n", cnt + 1));
			cnt++;
		}
	}
	gx.fslist = apr_pcalloc(gx.pool, sizeof(const char*) * (cnt + 1));
	CHECKMEM(gx.fslist);
	gx.devlist = apr_pcalloc(gx.pool, sizeof(const char*) * (cnt + 1));
	CHECKMEM(gx.devlist);
	cnt = 0;
	for (i = 0; i < sigar_fslist.number; i++)
	{
		if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK)
		{
			gx.fslist[cnt]
					= apr_pstrdup(gx.pool, sigar_fslist.data[i].dir_name);
			CHECKMEM(gx.fslist[cnt]);
			TR2(("fs: %s\n", gx.fslist[cnt]));
			gx.devlist[cnt] = apr_pstrdup(gx.pool,
					sigar_fslist.data[i].dev_name);
			CHECKMEM(gx.devlist[cnt]);
			cnt++;
		}
	}

	cnt = 0;
	for (i = 0; i < sigar_fslist.number; i++)
	{
		if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK || sigar_fslist.data[i].type == SIGAR_FSTYPE_NETWORK)
		{
			TR2(("sigar cnt: %d\n", cnt + 1));
			cnt++;
		}
	}
	gx.allfslist = apr_pcalloc(gx.pool, sizeof(const char*) * (cnt + 1));
	CHECKMEM(gx.allfslist);

	cnt = 0;
	for (i = 0; i < sigar_fslist.number; i++)
	{
		if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK || sigar_fslist.data[i].type == SIGAR_FSTYPE_NETWORK)
		{
			gx.allfslist[cnt]
					= apr_pstrdup(gx.pool, sigar_fslist.data[i].dir_name);
			CHECKMEM(gx.allfslist[cnt]);
			TR2(("allfs: %s\n", gx.allfslist[cnt]));
			cnt++;
		}
	}

	if (do_destroy)
		sigar_file_system_list_destroy(gx.sigar, &sigar_fslist);
}

void gx_main(int port, apr_int64_t signature)
{
	/* set up our log files */
	if (opt.log_dir)
	{
		mkdir(opt.log_dir, S_IRWXU | S_IRWXG);

		if (0 != chdir(opt.log_dir))
		{
			/* Invalid dir for log file, try home dir */
			char *home_dir = NULL;
			if (0 == apr_env_get(&home_dir, "HOME", gx.pool))
			{
				if (home_dir)
					chdir(home_dir);
			}
		}
	}

	update_log_filename();
	freopen(log_filename, "w", stdout);
	setlinebuf(stdout);

	gx.is_appliance = is_appliance();
	if (!get_and_allocate_hostname())
		gpsmon_fatalx(FLINE, 0, "failed to allocate memory for hostname");
	TR0(("HOSTNAME = '%s'\n", gx.hostname));



	// first chace to write to log file
	TR2(("signature = %" FMT64 "\n", signature));
	TR1(("detected %d cpu cores\n", number_cpu_cores));

	setup_gx(port, signature);
	setup_sigar();
	setup_udp();
	setup_tcp();

	gx.tick = 0;
	for (;;)
	{
		struct timeval tv;
		apr_hash_index_t* hi;

		/* serve events every 2 second */
		gx.tick++;
		gx.now = time(NULL);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if (-1 == event_loopexit(&tv))
		{
			gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno),
					"event_loopexit failed");
		}
		if (-1 == event_dispatch())
		{
			gpsmon_fatalx(FLINE, APR_FROM_OS_ERROR(errno), "event_dispatch failed");
		}

		/* get pid metrics */
		for (hi = apr_hash_first(0, gx.qexectab); hi; hi = apr_hash_next(hi))
		{
			if (opt.iterator_aggregate) {
				qexec_agg_t* qexec_agg;
				void* vptr;
				apr_hash_index_t* hj;

				apr_hash_this(hi, 0, 0, &vptr);
				qexec_agg = vptr;

				for (hj = apr_hash_first(0, qexec_agg->qexecaggtab); hj; hj = apr_hash_next(hj)) {
					gpmon_qexec_t* rec;

					apr_hash_this(hj, 0, 0, &vptr);
					rec = vptr;
					get_pid_metrics(rec->key.hash_key.pid,
							rec->key.tmid,
							rec->key.ssid,
							rec->key.ccnt);
				}
			} else {
				void* vptr;
				gpmon_qexec_t* rec;
				apr_hash_this(hi, 0, 0, &vptr);
				rec = vptr;
				get_pid_metrics(rec->key.hash_key.pid,
						rec->key.tmid,
						rec->key.ssid,
						rec->key.ccnt);
			}
		}

		/* check log size */
		if (gx.tick % 60 == 0)
		{
			apr_finfo_t finfo;
			if (0 == apr_stat(&finfo, log_filename, APR_FINFO_SIZE, gx.pool))
			{
				if (opt.max_log_size != 0 && finfo.size > opt.max_log_size)
				{
					update_log_filename();
					freopen(log_filename, "w", stdout);
					setlinebuf(stdout);
				}
			}
		}
	}
}

static void usage(const char* msg)
{
	fprintf(stdout, "\nusage: %s [options] port\n\n", opt.pname);
	fprintf(stdout, "options:\n");
	fprintf(stdout, "\t-?:\tprint this help screen\n");
	fprintf(stdout, "\t-v:\tverbose\n");
	fprintf(stdout, "\t-D:\trun in debug mode; don't run as daemon\n");
	fprintf(stdout, "\t-l:\tlog directory\n");
	fprintf(stdout, "\t-m:\tmax log size\n");
	fprintf(stdout, "\t-t:\tterminate timeout\n"),
	fprintf(stdout, "\t-a:\titerator aggregate\n");
	fprintf(stdout, "\t-i:\tignore qexec packet\n");
	if (msg)
		fprintf(stdout, "%s\n\n", msg);

	exit(msg ? 1 : 0);
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
	{ NULL, '?', 0, "print help screen" },
	{ NULL, 'v', 1, "verbose" },
	{ NULL, 'D', 0, "debug mode" },
	{ NULL, 'l', 1, "log directory" },
	{ NULL, 'm', 1, "max log size" },
	{ NULL, 't', 1, "terminate timeout" },
	{ NULL, 'a', 0, "iterator aggregate" },
	{ NULL, 'i', 0, "ignore qexec packet" },
	{ NULL, 0, 0, NULL } };
	apr_pool_t* pool;

	if (0 != (e = apr_pool_create_alloc(&pool, 0)))
	{
		gpsmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
	}

	bin_start = argv[0] + strlen(argv[0]) - 1;
	while (bin_start != argv[0] && *bin_start != '/')
		bin_start--;
	if (bin_start[0] == '/')
		bin_start++;

	opt.pname = bin_start;
	opt.v = opt.D = 0;
	opt.max_log_size = 0;
	opt.terminate_timeout = 0;
	opt.do_ignore_qexec_packet = false;

	if (0 != (e = apr_getopt_init(&os, pool, argc, argv)))
	{
		gpsmon_fatalx(FLINE, e, "apr_getopt_init failed");
	}

	while (0 == (e = apr_getopt_long(os, option, &ch, &arg)))
	{
		switch (ch)
		{
		case '?':
			usage(0);
			break;
		case 'v':
			opt.v = atoi(arg);
			break;
		case 'D':
			opt.D = 1;
			break;
		case 'l':
			opt.log_dir = strdup(arg);
			break;
		case 'm':
			opt.max_log_size = apr_atoi64(arg);
			break;
		case 'a':
			opt.iterator_aggregate = 1;
			break;
		case 't':
			opt.terminate_timeout = apr_atoi64(arg);
			break;
		case 'i':
			opt.do_ignore_qexec_packet = true;
			break;
		}
	}

	if (e != APR_EOF)
		usage("Error: illegal arguments");

	if (os->ind >= argc)
		usage("Error: missing port argument");
	opt.arg_port = argv[os->ind++];

	apr_pool_destroy(pool);

	verbose = opt.v;
	very_verbose = opt.V;
}

int main(int argc, const char* const argv[])
{
	int port, e;
	apr_int64_t signature;

	if (0 != (e = apr_initialize()))
	{
		gpsmon_fatalx(FLINE, e, "apr_initialize failed");
	}

	parse_command_line(argc, argv);

	port = atoi(opt.arg_port);
	if (!(0 < port && port < (1 << 16)))
		usage("Error: bad port number");

	if (1 != fscanf(stdin, "%" FMT64, &signature))
	{
		gpsmon_fatal(FLINE, "cannot read signature");
	}

	if (!opt.D)
	{
		if (0 != (e = apr_proc_detach(1)))
			gpsmon_fatalx(FLINE, e, "apr_proc_detach failed");
	}

#if defined (sun)
	if (NULL == (kc = kstat_open()))
	gpsmon_fatal(FLINE, "failed to open kstat");
#endif /* sun */

	number_cpu_cores = (int)sysconf(_SC_NPROCESSORS_CONF);

	// sanity check this number a little
	if (number_cpu_cores < 1)
	{
		number_cpu_cores = 1;
	}

	cpu_cores_utilization_multiplier = 100.0 / (float)number_cpu_cores;

	gx_main(port, signature);
	return 0;
}

