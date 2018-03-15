#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "gpmonlib.h"
#include "gpmondb.h"
#include "libpq-fe.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "time.h"

int gpdb_exec_search_for_at_least_one_row(const char*, PGconn*);
apr_status_t empty_harvest_file(const char*, apr_pool_t*, PGconn*);
apr_status_t truncate_tail_file(const char*, apr_pool_t*, PGconn*);
void upgrade_log_alert_table_distributed_key(PGconn*);

#define GPMON_HOSTTTYPE_HDW 1
#define GPMON_HOSTTTYPE_HDM 2
#define GPMON_HOSTTTYPE_ETL 3
#define GPMON_HOSTTTYPE_HBW 4
#define GPMON_HOSTTTYPE_HDC 5

#define MAX_SMON_PATH_SIZE (1024)
#define MAX_OWNER_LENGTH   (100)

#define GPDB_CONNECTION_STRING "dbname='" GPMON_DB "' user='" GPMON_DBUSER "' connect_timeout='30'"

int find_token_in_config_string(char* buffer, char**result, const char* token)
{
	return 1;
}

// assumes a valid connection already exists
static const char* gpdb_exec_only(PGconn* conn, PGresult** pres, const char* query)
{
	PGresult* res = 0;
	ExecStatusType status;

	TR1(("Query: %s\n", query));

	res = PQexec(conn, query);
	status = PQresultStatus(res);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
		return PQerrorMessage(conn);

	*pres = res;
	return 0;
}

static const bool gpdb_exec_ddl(PGconn* conn, const char* ddl_query)
{
	PGresult *result = NULL;
	const char *errmsg = gpdb_exec_only(conn, &result, ddl_query);
	PQclear(result);
	if (errmsg)
	{
		gpmon_warning(FLINE, "failed to execute query '%s': %s\n", ddl_query, errmsg);
	}
	return errmsg == NULL;
}

// creates a connection and then runs the query
static const char* gpdb_exec(PGconn** pconn, PGresult** pres, const char* query)
{
	const char *connstr = "dbname='" GPMON_DB "' user='" GPMON_DBUSER
	"' connect_timeout='30'";
	PGconn *conn = NULL;

	conn = PQconnectdb(connstr);
	// early assignment to pconn guarantees connection available to get freed by the caller
	*pconn = conn;

	if (PQstatus(conn) != CONNECTION_OK)
		return PQerrorMessage(conn);

	return gpdb_exec_only(conn, pres, query);
}

// persistant_conn is optional if you are already holding an open connectionconn
// return 1 if more than 0 rows are returned from query
// return 0 if zero rows are returned from query
int gpdb_exec_search_for_at_least_one_row(const char* QUERY, PGconn* persistant_conn)
{
	PGconn* conn = 0;
	PGresult* result = 0;
	int rowcount;
	int res = 0;
	const char* errmsg;

	if (persistant_conn)
	    errmsg = gpdb_exec_only(persistant_conn, &result, QUERY);
	else
	    errmsg = gpdb_exec(&conn, &result, QUERY);

	if (errmsg)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, QUERY);
	}
	else
	{
		rowcount = PQntuples(result);
		if (rowcount > 0)
			res = 1;
	}

	PQclear(result);

	if (conn)
	    PQfinish(conn);

	return res;
}

static bool should_recreate_from_result(PGresult	*result,
										const char	*encoding,
										bool		script_exist,
										int			expected_encoding_num)
{
	ASSERT(result);
	ASSERT(encoding);
	int rowcount = PQntuples(result);
	if (rowcount > 0)
	{
		const char* cmd = PQgetvalue(result, 0, 0);
		int encoding_num = atoi(PQgetvalue(result, 0, 1));
		ASSERT(cmd);
		const int MAX_ICONV_CMD_LEN = 50;
		char iconv[MAX_ICONV_CMD_LEN];
		snprintf(iconv, sizeof(iconv), "iconv -f %s -t %s -c", encoding, encoding);
		const char gpperfmoncat[] = "gpperfmoncat.sh";
		if (strncmp(cmd, iconv, sizeof(iconv)) != 0)
		{
			if (!script_exist && encoding_num == expected_encoding_num)
			{
				return false;
			}
		}
		else if (strncmp(cmd, gpperfmoncat, sizeof(gpperfmoncat)))
		{
			if (script_exist && encoding_num == expected_encoding_num)
			{
				return false;
			}
		}
	}
	return true;
}

// Whether log_alert_table needs to be recreated. The order command is
// EXECUTE 'cat ...' which would crash if gpdb-alert log files contain invalid
// character. We use 'iconv' instead of 'cat' to fix it.
// returns true if given table exist and uses 'cat' instead of 'iconv', then should be
// recreated.
// return false otherwise
static bool gpdb_should_recreate_log_alert(PGconn		*conn,
										   const char	*table_name,
										   const char	*encoding,
										   int			expected_encoding_num,
										   bool			script_exist)
{
	ASSERT(conn);
	ASSERT(strcasecmp(table_name, "log_alert_tail") == 0 ||
		   strcasecmp(table_name, "log_alert_now") == 0);

	PGresult *result = 0;

	const char* errmsg = NULL;

	const char* pattern = "select a.command, a.encoding from pg_exttable a, pg_class b "
		"where a.reloid = b.oid and b.relname='%s'";

	const int QRYBUFSIZ = 2000;
	char query[QRYBUFSIZ];
	snprintf(query, QRYBUFSIZ, pattern, table_name);

	bool ret = true;

	if (conn)
	    errmsg = gpdb_exec_only(conn, &result, query);

	if (errmsg)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, query);
	}
	else
	{
		ret = should_recreate_from_result(result,
										  encoding,
										  script_exist,
										  expected_encoding_num);
	}

	PQclear(result);
	return ret;
}

int gpdb_validate_gpperfmon(void)
{
	/* Check db */
	if (!gpdb_gpperfmon_db_exists())
		return 0;

	/* check post */
	if (!gpdb_get_gpmon_port())
		return 0;

	/* check external tables are accessable by gpmon user */
	if (!gpdb_validate_ext_table_access())
		return 0;

	return 1;
}

int gpdb_gpperfmon_db_exists(void)
{
	const char *connstr = "dbname='" GPMON_DB "' user='" GPMON_DBUSER "' connect_timeout='30'";
	int db_exists = 0;

	PGconn *conn = PQconnectdb(connstr);
	if (PQstatus(conn) == CONNECTION_OK)
	{
		db_exists = 1;
	}
	else
	{
		char *errmsg = PQerrorMessage(conn);
		fprintf(stderr, "Performance Monitor - failed to connect to gpperfmon database: %s",
						(errmsg == NULL ? "unknown reason" : errmsg));
	}

	PQfinish(conn);

	return db_exists;
}

int gpdb_gpperfmon_enabled(void)
{
	const char* QUERY = "SELECT 1 FROM pg_settings WHERE name = 'gp_enable_gpperfmon' and setting='on'";
	return gpdb_exec_search_for_at_least_one_row(QUERY, NULL);
}

int gpdb_validate_ext_table_access(void)
{
	const char* QUERY = "select * from master_data_dir";
	return gpdb_exec_search_for_at_least_one_row(QUERY, NULL);
}

int gpdb_get_gpmon_port(void)
{
	PGconn* conn = 0;
	PGresult* result = 0;
	char* curport = 0;
	int rowcount;
	const char* QUERY =
			"SELECT setting FROM pg_settings WHERE name = 'gpperfmon_port'";
	int port = 0;
	const char* errmsg = gpdb_exec(&conn, &result, QUERY);
	if (errmsg)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, QUERY);
	}
	else
	{
		rowcount = PQntuples(result);
		if (rowcount > 0)
		{
			curport = PQgetvalue(result, 0, 0);
			if (curport)
			{
				port = atoi(curport);
			}
		}
	}
	PQclear(result);
	PQfinish(conn);

	if (!port)
	{
		gpmon_warning(FLINE, "Unable to retrieve gpperfmon_port GUC from GPDB\n");
	}
	return port;
}


struct hostinfo_holder_t
{
	addressinfo_holder_t* addressinfo_head;
	addressinfo_holder_t* addressinfo_tail;
	apr_uint32_t address_count;

	char* datadir;
	char* smon_dir;
	char* hostname;
	int is_master;
	int is_hdm;
	int is_hdw;
	int is_hbw;
	int is_hdc;
	int is_etl;
};

void initializeHostInfoDataWithAddress(struct hostinfo_holder_t*, char*, int);
void initializeHostInfoDataFromFileEntry(apr_pool_t*, struct hostinfo_holder_t*,char*, char*, int, char*, char*);


void initializeHostInfoDataWithAddress(struct hostinfo_holder_t* holder, char* address, int firstAddress)
{
	// USE permenant memory to store this data

	addressinfo_holder_t* aiholder = calloc(1, sizeof(addressinfo_holder_t));
	CHECKMEM(aiholder);

	aiholder->address = strdup(address);
	CHECKMEM(aiholder->address);

	if (firstAddress)
	{
		holder->addressinfo_head = holder->addressinfo_tail = aiholder;
	}
	else
	{
		holder->addressinfo_tail->next = aiholder;
		holder->addressinfo_tail = aiholder;
	}
}

void initializeHostInfoDataFromFileEntry(apr_pool_t* tmp_pool, struct hostinfo_holder_t* holder,
		char* primary_hostname, char* hostEntry, int hostType, char* smon_bin_dir, char* smon_log_dir)
{
	holder->hostname = apr_pstrdup(tmp_pool, primary_hostname);
	CHECKMEM(holder->hostname);

	holder->smon_dir = apr_pstrdup(tmp_pool, smon_bin_dir);
	CHECKMEM(holder->smon_dir);

	holder->datadir = apr_pstrdup(tmp_pool, smon_log_dir);
	CHECKMEM(holder->datadir);

	switch(hostType)
	{
		case GPMON_HOSTTTYPE_HDW:
			holder->is_hdw = 1;
		break;
		case GPMON_HOSTTTYPE_HDM:
			holder->is_hdm = 1;
		break;
		case GPMON_HOSTTTYPE_ETL:
			holder->is_etl = 1;
		break;
		case GPMON_HOSTTTYPE_HBW:
			holder->is_hbw = 1;
		break;
		case GPMON_HOSTTTYPE_HDC:
			holder->is_hdc = 1;
		break;
	}

	holder->address_count = 0;
	int firstAddress = 1;

	while(*hostEntry)
	{
		char* location = strchr(hostEntry, ',');
		if (location)
			*location = 0;

		initializeHostInfoDataWithAddress(holder, hostEntry, firstAddress);
		holder->address_count++;
		if (!location)
			return; // there were no commas so this is the last address in the hostEntry
		*location = ',';
		hostEntry = location+1;
		firstAddress = 0;
	}
}

void process_line_in_hadoop_cluster_info(apr_pool_t* tmp_pool, apr_hash_t* htab, char* line, char* smon_bin_location, char* smon_log_location)
{
	if (!line)
	{
		gpmon_warningx(FLINE, 0, "Line in hadoop cluster info file is null, skipping");
		return;
	}

	char* host;
	char* category;

	char primary_hostname[64];

	char* location = strchr(line, '#');
	if (location)
	{
		*location = 0; // remove comments from the line
	}

	// we do these in reverse order so inserting null chars does not prevent finding other tokens
	if (find_token_in_config_string(line, &category, "Categories"))
	{
		return;
	}
	location = strchr(category, ','); //remove the comma and extra categories
	if (location)
	{
		*location = 0;
	}

	if (find_token_in_config_string(line, &host, "Hostname"))
	{
		return;
	}
	TR1(("Found hadoop host %s\n",host ));
	// look for the 3 hadoop host types
	int monitored_device = 0;
	int hostType = 0;
	if (strcmp(category, "hdm") == 0)
	{
		monitored_device = 1;
		hostType = GPMON_HOSTTTYPE_HDM;
	}

	if (strcmp(category, "hdw") == 0)
	{
		monitored_device = 1;
		hostType = GPMON_HOSTTTYPE_HDW;
	}

	if (strcmp(category, "hdc") == 0)
	{
		monitored_device = 1;
		hostType = GPMON_HOSTTTYPE_HDC;
	}
	// The below code is the same as the devices file parsing code

	// segment host, switch, etc ... we are only adding additional hosts required for performance monitoring
	if (!monitored_device)
	{
		return;
	}

	strncpy(primary_hostname, host, sizeof(primary_hostname));
	primary_hostname[sizeof(primary_hostname) - 1] = 0;
	location = strchr(primary_hostname, ',');
	if (location)
	{
		*location = 0;
	}

	struct hostinfo_holder_t* hostinfo_holder = apr_hash_get(htab, primary_hostname, APR_HASH_KEY_STRING);
	if (hostinfo_holder)
	{
		gpmon_warningx(FLINE, 0, "Host '%s' is duplicated in clusterinfo.txt", primary_hostname);
		return;
	}

	// OK Lets add this record at this point
	hostinfo_holder = apr_pcalloc(tmp_pool, sizeof(struct hostinfo_holder_t));
	CHECKMEM(hostinfo_holder);

	apr_hash_set(htab, primary_hostname, APR_HASH_KEY_STRING, hostinfo_holder);

	initializeHostInfoDataFromFileEntry(tmp_pool, hostinfo_holder, primary_hostname, host, hostType, smon_bin_location, smon_log_location);
}

//Return 1 if not a hadoop software only cluster and 0 it is a hadoop software only cluster
int get_hadoop_hosts_and_add_to_hosts(apr_pool_t* tmp_pool, apr_hash_t* htab, mmon_options_t* opt)
{
	if (!opt->smon_hadoop_swonly_binfile)
	{
		TR0(("hadoop_smon_path not specified in gpmmon config. not processing hadoop nodes\n"));
		return 1;
	}

	char* smon_log_dir;
	char* hadoop_cluster_file;
	if (opt->smon_hadoop_swonly_logdir)
	{
		smon_log_dir = opt->smon_hadoop_swonly_logdir;
	}
	else
	{
		smon_log_dir = (char*)PATH_TO_HADOOP_SMON_LOGS;
	}
	if (opt->smon_hadoop_swonly_clusterfile)
	{
		hadoop_cluster_file = opt->smon_hadoop_swonly_clusterfile;
	}
	else
	{
		hadoop_cluster_file = (char*)DEFAULT_PATH_TO_HADOOP_HOST_FILE;
	}

	FILE* fd = fopen(hadoop_cluster_file, "r");
	if (!fd)
	{
		TR0(("not a hadoop software only cluster ... not reading %s\n", hadoop_cluster_file));
		return 1;
	}

	char* line;
	char buffer[1024];

	// process the hostlines
	while (NULL != fgets(buffer, sizeof(buffer), fd))
	{
		line = gpmon_trim(buffer);// remove new line
		process_line_in_hadoop_cluster_info(tmp_pool, htab, line, opt->smon_hadoop_swonly_binfile, smon_log_dir);
	}

	fclose(fd);
	return 0;
}


void gpdb_get_hostlist(int* hostcnt, host_t** host_table, apr_pool_t* global_pool, mmon_options_t* opt)
{
	apr_pool_t* pool;
	PGconn* conn = 0;
	PGresult* result = 0;
	int rowcount, i;
	unsigned int unique_hosts = 0;
	apr_hash_t* htab;
	struct hostinfo_holder_t* hostinfo_holder = NULL;
	host_t* hosts = NULL;
	int e;

	// 0 -- hostname, 1 -- address, 2 -- datadir, 3 -- is_master,
	const char *QUERY = "SELECT distinct hostname, address, case when content < 0 then 1 else 0 end as is_master, MAX(datadir) as datadir FROM gp_segment_configuration "
		  	    "GROUP BY (hostname, address, is_master) order by hostname";

	if (0 != (e = apr_pool_create_alloc(&pool, NULL)))
	{
		gpmon_fatalx(FLINE, e, "apr_pool_create_alloc failed");
	}

	const char* errmsg = gpdb_exec(&conn, &result, QUERY);

	TR2((QUERY));
	TR2(("\n"));

	if (errmsg)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, QUERY);
	}
	else
	{
		// hash of hostnames to addresses
		htab = apr_hash_make(pool);

		rowcount = PQntuples(result);

		for (i = 0; i < rowcount; i++)
		{
			char* curr_hostname = PQgetvalue(result, i, 0);

			hostinfo_holder = apr_hash_get(htab, curr_hostname, APR_HASH_KEY_STRING);

			if (!hostinfo_holder)
			{
				hostinfo_holder = apr_pcalloc(pool, sizeof(struct hostinfo_holder_t));
				CHECKMEM(hostinfo_holder);

				apr_hash_set(htab, curr_hostname, APR_HASH_KEY_STRING, hostinfo_holder);

				hostinfo_holder->hostname = curr_hostname;
				hostinfo_holder->is_master = atoi(PQgetvalue(result, i, 2));
				hostinfo_holder->datadir = PQgetvalue(result, i, 3);

				// use permenant memory for address list -- stored for duration

				// populate 1st on list and save to head and tail
				hostinfo_holder->addressinfo_head = hostinfo_holder->addressinfo_tail = calloc(1, sizeof(addressinfo_holder_t));
				CHECKMEM(hostinfo_holder->addressinfo_tail);

				// first is the hostname
				hostinfo_holder->addressinfo_tail->address = strdup(hostinfo_holder->hostname);
				CHECKMEM(hostinfo_holder->addressinfo_tail->address);


				// add a 2nd to the list
				hostinfo_holder->addressinfo_tail->next = calloc(1, sizeof(addressinfo_holder_t));
				CHECKMEM(hostinfo_holder->addressinfo_tail);
				hostinfo_holder->addressinfo_tail = hostinfo_holder->addressinfo_tail->next;

				// second is address
				hostinfo_holder->addressinfo_tail->address = strdup(PQgetvalue(result, i, 1));
				CHECKMEM(hostinfo_holder->addressinfo_tail->address);

				// one for hostname one for address
				hostinfo_holder->address_count = 2;
			}
			else
			{
				// permenant memory for address list -- stored for duration
				hostinfo_holder->addressinfo_tail->next = calloc(1, sizeof(addressinfo_holder_t));
				CHECKMEM(hostinfo_holder->addressinfo_tail);

				hostinfo_holder->addressinfo_tail = hostinfo_holder->addressinfo_tail->next;

				// permenant memory for address list -- stored for duration
				hostinfo_holder->addressinfo_tail->address = strdup(PQgetvalue(result, i, 1));
				CHECKMEM(hostinfo_holder->addressinfo_tail->address);

				hostinfo_holder->address_count++;
			}

		}

		TR0(("checking for SW Only hadoop hosts.\n"));
		get_hadoop_hosts_and_add_to_hosts(pool, htab, opt);

		unique_hosts = apr_hash_count(htab);

		// allocate memory for host list (not freed ever)
		hosts = calloc(unique_hosts, sizeof(host_t));

		apr_hash_index_t* hi;
		void* vptr;
		int hostcounter = 0;
		for (hi = apr_hash_first(0, htab); hi; hi = apr_hash_next(hi))
		{
			// sanity check
			if (hostcounter >= unique_hosts)
			{
				gpmon_fatalx(FLINE, 0, "host counter exceeds unique hosts");
			}

			apr_hash_this(hi, 0, 0, &vptr);
			hostinfo_holder = vptr;

			hosts[hostcounter].hostname = strdup(hostinfo_holder->hostname);
			hosts[hostcounter].data_dir = strdup(hostinfo_holder->datadir);
			if (hostinfo_holder->smon_dir)
			{
				hosts[hostcounter].smon_bin_location = strdup(hostinfo_holder->smon_dir);
			}
			hosts[hostcounter].is_master = hostinfo_holder->is_master;
			hosts[hostcounter].addressinfo_head = hostinfo_holder->addressinfo_head;
			hosts[hostcounter].addressinfo_tail = hostinfo_holder->addressinfo_tail;
			hosts[hostcounter].address_count = hostinfo_holder->address_count;
			hosts[hostcounter].connection_hostname.current = hosts[hostcounter].addressinfo_head;

			if (hostinfo_holder->is_hdm)
				hosts[hostcounter].is_hdm = 1;

			if (hostinfo_holder->is_hdw)
				hosts[hostcounter].is_hdw = 1;

			if (hostinfo_holder->is_etl)
				hosts[hostcounter].is_etl = 1;

			if (hostinfo_holder->is_hbw)
				hosts[hostcounter].is_hbw = 1;

			if (hostinfo_holder->is_hdc)
				hosts[hostcounter].is_hdc = 1;

			apr_thread_mutex_create(&hosts[hostcounter].mutex, APR_THREAD_MUTEX_UNNESTED, global_pool); // use the global pool so the mutexes last beyond this function

			hostcounter++;
		}

		*hostcnt = hostcounter;
	}

	apr_pool_destroy(pool);
	PQclear(result);
	PQfinish(conn);

	if (!hosts || *hostcnt < 1)
	{
		gpmon_fatalx(FLINE, 0, "no valid hosts found");
	}

	*host_table = hosts;
}

void gpdb_get_master_data_dir(char** hostname, char** mstrdir, apr_pool_t* pool)
{
	PGconn* conn = 0;
	PGresult* result = 0;
	const char* QUERY = "select * from master_data_dir";
	char* dir = 0;
	char* hname = 0;
	int rowcount;
	const char* errmsg = gpdb_exec(&conn, &result, QUERY);
	if (errmsg)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, QUERY);
	}
	else
	{
		rowcount = PQntuples(result);
		if (rowcount > 0)
		{
			hname = PQgetvalue(result, 0, 0);
			dir = PQgetvalue(result, 0, 1);
		}

		if (!hname || !dir)
		{
			gpmon_warning(FLINE, "unable to get master data directory");
		}
		else
		{
			hname = apr_pstrdup(pool, gpmon_trim(hname));
			CHECKMEM(hname);

			dir = apr_pstrdup(pool, gpmon_trim(dir));
			CHECKMEM(dir);
		}
	}

	PQclear(result);
	PQfinish(conn);

	*hostname = hname;
	*mstrdir = dir;
}

void gpdb_get_single_string_from_query(const char* QUERY, char** resultstring, apr_pool_t* pool)
{
	PGconn* conn = 0;
	PGresult* result = 0;
	char* tmpoutput = 0;
	int rowcount;
	const char* errmsg = gpdb_exec(&conn, &result, QUERY);
	if (errmsg)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, QUERY);
	}
	else
	{
		rowcount = PQntuples(result);
		if (rowcount == 1)
		{
			tmpoutput = PQgetvalue(result, 0, 0);
		}
		else if (rowcount > 1)
		{
			gpmon_warning(FLINE, "unexpected number of rows returned from query %s", QUERY);
		}

		if (tmpoutput)
		{
			tmpoutput = apr_pstrdup(pool, gpmon_trim(tmpoutput));
			CHECKMEM(tmpoutput);
		}
	}

	PQclear(result);
	PQfinish(conn);

	*resultstring = tmpoutput;
}


static void check_and_add_partition(PGconn* conn, const char* tbl, int begin_year, int begin_month, int end_year, int end_month)
{
	PGresult* result = 0;
	const char* errmsg;
	const int QRYBUFSIZ = 1024;

	char qry[QRYBUFSIZ];
	const char* CHK_QRYFMT = "select dt from (select substring(partitionrangestart from 2 for 7) as dt from pg_partitions where tablename = '%s_history' ) as TBL where TBL.dt = '%d-%02d';";
	const char* ADD_QRYFMT = "alter table %s_history add partition start ('%d-%02d-01 00:00:00'::timestamp without time zone) inclusive end ('%d-%02d-01 00:00:00'::timestamp without time zone) exclusive;";

	snprintf(qry, QRYBUFSIZ, CHK_QRYFMT, tbl, begin_year, begin_month);
	if (!gpdb_exec_search_for_at_least_one_row(qry, conn))
	{
		// this partition does not exist, create it

		snprintf(qry, QRYBUFSIZ, ADD_QRYFMT, tbl, begin_year, begin_month, end_year, end_month);
		TR0(("Add partition table '%s\n'", qry));
		errmsg = gpdb_exec_only(conn, &result, qry);
		if (errmsg)
		{
			gpmon_warning(FLINE, "partition add response from server: %s\n", errmsg);
		}

		PQclear(result);
	}
}

// Drop old partitions if partition_age option is set.
static void drop_old_partitions(PGconn* conn, const char* tbl, mmon_options_t *opt)
{
	const int QRYBUFSIZ = 1024;
	PGresult* result = NULL;
	const char* errmsg;
	char qry[QRYBUFSIZ];

	const char* SELECT_QRYFMT = "SELECT partitiontablename, partitionrangestart FROM pg_partitions "
						        "WHERE tablename = '%s_history' "
								"ORDER BY partitionrangestart DESC OFFSET %d;";
	const char* DROP_QRYFMT   = "ALTER TABLE %s_history DROP PARTITION IF EXISTS FOR (%s);";

	int partition_age = opt->partition_age;

	if (partition_age <= 0) {
		TR0(("partition_age turned off\n"));
		return;
	}

	// partition_age + 1 because we always add 2 partitions for the boundary case
	snprintf(qry, QRYBUFSIZ, SELECT_QRYFMT, tbl, partition_age + 1);

	errmsg = gpdb_exec_only(conn, &result, qry);
	if (errmsg)
	{
		gpmon_warning(FLINE, "drop partition: select query '%s' response from server: %s\n", qry, errmsg);
	}
	else
	{
		int rowcount = PQntuples(result);
		int i = 0;
		for (; i < rowcount; i++)
		{
			PGresult* dropResult = NULL;
			char* partitiontablename  = PQgetvalue(result, i, 0);
			char* partitionrangestart = PQgetvalue(result, i, 1);

			// partitionrangestart comes out looking like `'2017-02-01 00:00:00'::timestamp(0) without time zone`
			//                                       or   `'2010-01-01 00:00:00-08'::timestamp with time zone`
			char *unwanted = strstr(partitionrangestart, "::" );

			size_t substring_size = unwanted - partitionrangestart + 1;
			char *substring = (char *) malloc(substring_size);
			memcpy(substring, partitionrangestart, substring_size);
			substring[substring_size - 1] = '\0';

			snprintf(qry, QRYBUFSIZ, DROP_QRYFMT, tbl, substring);

			free(substring);
			TR0(("Dropping partition table '%s'\n", partitiontablename));
			errmsg = gpdb_exec_only(conn, &dropResult, qry);
			PQclear(dropResult);
			if (errmsg)
			{
				gpmon_warning(FLINE, "drop partition: drop query '%s' response from server: %s\n", qry, errmsg);
				break;
			}
		}
	}
	PQclear(result);
}


static apr_status_t check_partition(const char* tbl, apr_pool_t* pool, PGconn* conn, mmon_options_t *opt)
{
	struct tm tm;
	time_t now;

	unsigned short year[3];
	unsigned char month[3];

	TR0(("check partitions on %s_history\n", tbl));

	if (!conn)
		return APR_ENOMEM;

	now = time(NULL);
	if (!localtime_r(&now, &tm))
	{
		gpmon_warning(FLINE, "error in check_partition getting current time\n");
		return APR_EGENERAL;
	}

	year[0] = 1900 + tm.tm_year;
	month[0] = tm.tm_mon+1;

	if (year[0] < 1 || month[0] < 1 || year[0] > 2030 || month[0] > 12)
	{
		gpmon_warning(FLINE, "invalid current month/year in check_partition %u/%u\n", month, year);
		return APR_EGENERAL;
	}

	if (month[0] < 11)
	{
		month[1] = month[0] + 1;
		month[2] = month[0] + 2;

		year[1] = year[0];
		year[2] = year[0];
	}
	else if (month[0] == 11)
	{
		month[1] = 12;
		month[2] = 1;

		year[1] = year[0];
		year[2] = year[0] + 1;
	}
	else
	{
		month[1] = 1;
		month[2] = 2;

		year[1] = year[0] + 1;
		year[2] = year[0] + 1;
	}

	check_and_add_partition(conn, tbl, year[0], month[0], year[1], month[1]);
	check_and_add_partition(conn, tbl, year[1], month[1], year[2], month[2]);

	drop_old_partitions(conn, tbl, opt);

	TR0(("check partitions on %s_history done\n", tbl));
	return APR_SUCCESS;
}

static apr_status_t harvest(const char* tbl, apr_pool_t* pool, PGconn* conN)
{
	PGconn* conn = 0;
	PGresult* result = 0;
	const int QRYBUFSIZ = 255;
	char qrybuf[QRYBUFSIZ];
	const char* QRYFMT = "insert into %s_history select * from _%s_tail;";
	const char* errmsg;
	apr_status_t res = APR_SUCCESS;

	snprintf(qrybuf, QRYBUFSIZ, QRYFMT, tbl, tbl);

	errmsg = gpdb_exec(&conn, &result, qrybuf);
	if (errmsg)
	{
		res = 1;
		gpmon_warningx(FLINE, 0, "---- HARVEST %s FAILED ---- on query %s with error %s\n", tbl, qrybuf, errmsg);
	}
	else
	{
		TR1(("load completed OK: %s\n", tbl));
	}

	PQclear(result);
	PQfinish(conn);
	return res;
}

/**
 * This function removes the not null constraint from the segid column so that
 * we can set it to null when the segment aggregation flag is true
 */
apr_status_t remove_segid_constraint(void)
{
	PGconn* conn = 0;
	PGresult* result = 0;
	const char* ALTERSTR = "alter table iterators_history alter column segid drop not null;";
	const char* errmsg;
	apr_status_t res = APR_SUCCESS;

	errmsg = gpdb_exec(&conn, &result, ALTERSTR);
	if (errmsg)
	{
		res = 1;
		gpmon_warningx(FLINE, 0, "---- Alter FAILED ---- on command: %s with error %s\n", ALTERSTR, errmsg);
	}
	else
	{
		TR1(("remove_segid_constraint: alter completed OK\n"));
	}

	PQclear(result);
	PQfinish(conn);
	return res;
}

apr_status_t gpdb_harvest_one(const char* table)
{
	return harvest(table, NULL, NULL);
}



apr_status_t truncate_file(char* fn, apr_pool_t* pool)
{
	apr_file_t *fp = NULL;
	apr_status_t status;

	status = apr_file_open(&fp, fn, APR_WRITE|APR_CREATE|APR_TRUNCATE, APR_UREAD|APR_UWRITE, pool);

	if (status == APR_SUCCESS)
	{
		status = apr_file_trunc(fp, 0);
		apr_file_close(fp);
	}

	if (status != APR_SUCCESS)
	{
		gpmon_warningx(FLINE, 0, "harvest process truncate file %s failed", fn);
	}
	else
	{
		TR1(("harvest truncated file %s: ok\n", fn));
	}

	return status;
}


/* rename tail to stage */
static apr_status_t rename_tail_files(const char* tbl, apr_pool_t* pool, PGconn* conn)
{
	char srcfn[PATH_MAX];
	char dstfn[PATH_MAX];

	apr_status_t status;

	/* make the file names */
	snprintf(srcfn, PATH_MAX, "%s%s_tail.dat", GPMON_DIR, tbl);
	snprintf(dstfn, PATH_MAX, "%s%s_stage.dat", GPMON_DIR, tbl);

	status = apr_file_rename(srcfn, dstfn, pool);
	if (status != APR_SUCCESS)
	{
		gpmon_warningx(FLINE, status, "harvest failed renaming %s to %s", srcfn, dstfn);
		return status;
	}
	else
	{
		TR1(("harvest rename %s to %s success\n", srcfn, dstfn));
	}

	return status;
}

/* append stage data to _tail file */
static apr_status_t append_to_harvest(const char* tbl, apr_pool_t* pool, PGconn* conn)
{
	char srcfn[PATH_MAX];
	char dstfn[PATH_MAX];

	apr_status_t status;

	/* make the file names */
	snprintf(srcfn, PATH_MAX, "%s%s_stage.dat", GPMON_DIR, tbl);
	snprintf(dstfn, PATH_MAX, "%s_%s_tail.dat", GPMON_DIR, tbl);

	status = apr_file_append(srcfn, dstfn, APR_FILE_SOURCE_PERMS, pool);
	if (status != APR_SUCCESS)
	{
		gpmon_warningx(FLINE, status, "harvest failed appending %s to %s", srcfn, dstfn);
	}
	else
	{
		TR1(("harvest append %s to %s: ok\n", srcfn, dstfn));
	}

	return status;
}

typedef apr_status_t eachtablefunc(const char* tbl, apr_pool_t*, PGconn*);
typedef apr_status_t eachtablefuncwithopt(const char* tbl, apr_pool_t*, PGconn*, mmon_options_t*);

apr_status_t call_for_each_table(eachtablefunc, apr_pool_t*, PGconn*);
apr_status_t call_for_each_table_with_opt(eachtablefuncwithopt, apr_pool_t*, PGconn*, mmon_options_t*);


char* all_tables[] = { "system", "queries", "database", "segment", "diskspace" };

apr_status_t call_for_each_table(eachtablefunc func, apr_pool_t* pool, PGconn* conn)
{
	apr_status_t status = APR_SUCCESS;
	apr_status_t r;
	int num_tables = sizeof(all_tables) / sizeof (char*);
	int i;

	for (i = 0; i < num_tables; ++i)
	{
		r = func(all_tables[i], pool, conn);
		if (r != APR_SUCCESS)
		{
			status = r;
		}
	}

	return status;
}

apr_status_t call_for_each_table_with_opt(eachtablefuncwithopt func, apr_pool_t* pool, PGconn* conn, mmon_options_t *opt)
{
	apr_status_t status = APR_SUCCESS;
	apr_status_t r;
	int num_tables = sizeof(all_tables) / sizeof (char*);
	int i;

	for (i = 0; i < num_tables; ++i)
	{
		r = func(all_tables[i], pool, conn, opt);
		if (r != APR_SUCCESS)
		{
			status = r;
		}
	}

	return status;
}

/* rename tail files to stage files */
apr_status_t gpdb_rename_tail_files(apr_pool_t* pool)
{
	return call_for_each_table(rename_tail_files, pool, NULL);
}

/* copy data from stage files to harvest files */
apr_status_t gpdb_copy_stage_to_harvest_files(apr_pool_t* pool)
{
	return call_for_each_table(append_to_harvest, pool, NULL);
}

/* truncate _tail files */
apr_status_t empty_harvest_file(const char* tbl, apr_pool_t* pool, PGconn* conn)
{
	char fn[PATH_MAX];
	snprintf(fn, PATH_MAX, "%s_%s_tail.dat", GPMON_DIR, tbl);
	return truncate_file(fn, pool);
}

/* truncate tail files */
apr_status_t truncate_tail_file(const char* tbl, apr_pool_t* pool, PGconn* conn)
{
	char fn[PATH_MAX];
	snprintf(fn, PATH_MAX, "%s%s_tail.dat", GPMON_DIR, tbl);
	return truncate_file(fn, pool);
}

/* truncate _tail files to clear data already loaded into the DB */
apr_status_t gpdb_truncate_tail_files(apr_pool_t* pool)
{
	return call_for_each_table(truncate_tail_file, pool, NULL);
}

/* truncate _tail files to clear data already loaded into the DB */
apr_status_t gpdb_empty_harvest_files(apr_pool_t* pool)
{
	return call_for_each_table(empty_harvest_file, pool, NULL);
}

/* insert _tail data into history table */
apr_status_t gpdb_harvest(void)
{
	return call_for_each_table(harvest, NULL, NULL);
}

static bool gpdb_insert_alert_log()
{
	PGconn* conn = 0;
	PGresult* result = 0;
	const char* QRY = "insert into log_alert_history select * from log_alert_tail;";
	const char* errmsg;
	errmsg = gpdb_exec(&conn, &result, QRY);

	bool success = true;
	if (errmsg)
	{
		gpmon_warningx(
			FLINE, 0,
			"---- ARCHIVING HISTORICAL ALERT DATA FAILED ---- on query %s with error %s\n",
			QRY, errmsg);
		success = false;
	}
	else
	{
		TR1(("load completed OK: alert_log\n"));
	}

	PQclear(result);
	PQfinish(conn);
	return success;
}

static void gpdb_remove_success_files(apr_array_header_t *success_append_files, apr_pool_t *pool)
{
	void *file_slot = NULL;
	while ((file_slot = apr_array_pop(success_append_files)))
	{
		const char *file_name = (*(char**)file_slot);
		if (file_name)
		{
			if (apr_file_remove(file_name, pool) != APR_SUCCESS)
			{
				gpmon_warningx(FLINE, 0, "failed removing file:%s", file_name);
			}
		}
	}
}

static int cmp_string(const void *left, const void *right)
{
	const char *op1 = *(const char**)left;
	const char *op2 = *(const char**)right;
	return strcmp(op1, op2);
}

// Find all files start with 'gpdb-alert' under GPMON_LOG directory, sort it and
// remove the latest one 'gpdb-alert-*.csv' as it is still used by GPDB.
static void get_alert_log_tail_files(apr_array_header_t *tail_files, apr_pool_t *pool)
{
	apr_dir_t *dir;
	apr_status_t status = apr_dir_open(&dir, GPMON_LOG, pool);
	if (status != APR_SUCCESS)
	{
		gpmon_warningx(FLINE, status, "failed opening directory:%s", GPMON_LOG);
		return;
	}

	apr_finfo_t dirent;
	static const char gpdb_prefix[] = "gpdb-alert";
	while (apr_dir_read(&dirent, APR_FINFO_DIRENT, dir) == APR_SUCCESS)
	{
		if (strncmp(dirent.name, gpdb_prefix, sizeof(gpdb_prefix) - 1) == 0)
		{
			void *file_slot = apr_array_push(tail_files);
			if (! file_slot)
			{
				gpmon_warningx(FLINE, 0, "failed getting alert tail log:%s due to out of memory", dirent.name);
				continue;
			}
			(*(const char**)file_slot) = apr_pstrcat(pool, GPMON_LOG, "/", dirent.name, NULL);
		}
	}

	// We only want to use qsort in stdlib.h, not the macro qsort in port.h.
	(qsort)(tail_files->elts, tail_files->nelts, tail_files->elt_size, cmp_string);
	(void)apr_array_pop(tail_files);
	apr_dir_close(dir);
}

void gpdb_import_alert_log(apr_pool_t *pool)
{
	// Get alert log files to be imported.
	apr_array_header_t* tail_files = apr_array_make(pool, 10, sizeof(char*));
	apr_array_header_t* success_append_files = apr_array_make(pool, 10, sizeof(char*));
	get_alert_log_tail_files(tail_files, pool);

	// Create or truncate stage file.
	char *dst_file = apr_pstrcat(pool, GPMON_LOG, "/", GPMON_ALERT_LOG_STAGE, NULL);
	apr_status_t status = truncate_file(dst_file, pool);
	if (status != APR_SUCCESS)
	{
	    gpmon_warningx(FLINE, 0, "failed truncating stage file:%s", dst_file);
	    return;
	}

	// Append alert log tail file to stage file
	void *tail_file = NULL;
	while ((tail_file = apr_array_pop(tail_files)))
	{
		char *filename = *(char**)tail_file;
		void *success_file_slot = apr_array_push(success_append_files);
		if (!success_file_slot)
		{
			gpmon_warningx(
				FLINE, 0, "failed appending file:%s to stage file:%s due to out of memory",
				filename, dst_file);
			continue;
		}
		(*(char**)success_file_slot) = NULL;

	    status = apr_file_append(filename, dst_file, APR_FILE_SOURCE_PERMS, pool);
	    if (status != APR_SUCCESS)
	    {
			gpmon_warningx(FLINE, status, "failed appending file:%s to stage file:%s", filename, dst_file);
			continue;
		}
	    else
	    {
			(*(char**)success_file_slot) = filename;
	    	TR1(("success appending file:%s to stage file:%s\n", filename, dst_file));
	    }
	}

	// Insert tail file to history table.
	if (!gpdb_insert_alert_log())
	{
		// Failure might happen on malformed log entries
		time_t now;
		char timestr[20];
		char *bad_file;

		// Copy failed log into separate file for user attention
		now = time(NULL);
		strftime(timestr, 20, "%Y-%m-%d_%H%M%S", localtime(&now));
		bad_file = apr_pstrcat(pool, GPMON_LOG, "/", GPMON_ALERT_LOG_STAGE, "_broken_", timestr,  NULL);
		if (apr_file_copy(dst_file, bad_file, APR_FPROT_FILE_SOURCE_PERMS, pool) == APR_SUCCESS)
		{
			gpmon_warningx(FLINE, status, "Staging file with broken entries is archived to %s", bad_file);
		}
		else
		{
			gpmon_warningx(FLINE, status, "failed copying stage file:%s to broken file:%s", dst_file, bad_file);
		}
	}

	// Delete tail file regardless of load success, as keeping too many tail files
	// might cause serious harm to the system
	gpdb_remove_success_files(success_append_files, pool);
	truncate_file(dst_file, pool);
}


/* insert _tail data into history table */
apr_status_t gpdb_check_partitions(mmon_options_t *opt)
{
	apr_status_t result;

	PGconn *conn = NULL;
	conn = PQconnectdb(GPDB_CONNECTION_STRING);

	if (PQstatus(conn) != CONNECTION_OK) {
		gpmon_warning(
				FLINE,
				"error creating GPDB client connection to dynamically "
						"check/create gpperfmon partitions: %s",
				PQerrorMessage(conn));
		result = APR_EINVAL;
	} else {
		result = call_for_each_table_with_opt(check_partition, NULL, conn, opt);

		// make sure to run check_partition even if we just got a failure from the previous call
		apr_status_t temp_result;
		temp_result = check_partition("log_alert", NULL, conn, opt);

		// use the first error that occurred, if any
		if (result == APR_SUCCESS) {
			result = temp_result;
		}
	}

	// close connection
	PQfinish(conn);
	return result;
}

static void convert_tuples_to_hash(PGresult *result, apr_hash_t *hash, apr_pool_t *pool)
{
	int rowcount = PQntuples(result);
	int i = 0;
	for (; i < rowcount; i++)
	{
		char* sessid = PQgetvalue(result, i, 0);
		char* query  = PQgetvalue(result, i, 1);

		char *sessid_copy = apr_pstrdup(pool, sessid);
		char *query_copy  = apr_pstrdup(pool, query);
		if (sessid_copy == NULL || query_copy == NULL)
		{
			gpmon_warning(FLINE, "Out of memory");
			continue;
		}
		apr_hash_set(hash, sessid_copy, APR_HASH_KEY_STRING, query_copy);
	}
}

apr_hash_t *get_active_queries(apr_pool_t *pool)
{
	PGresult   *result = NULL;
	apr_hash_t *active_query_tab = NULL;

	PGconn *conn = PQconnectdb(GPDB_CONNECTION_STRING);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		gpmon_warning(
			FLINE,
			"error creating GPDB client connection to dynamically "
			"check/create gpperfmon partitions: %s",
		PQerrorMessage(conn));
		PQfinish(conn);
		return NULL;
	}

	const char *qry= "SELECT sess_id, current_query FROM pg_stat_activity;";
	const char *errmsg = gpdb_exec_only(conn, &result, qry);
	if (errmsg)
	{
		gpmon_warning(FLINE, "check query status failed : %s", errmsg);
	}
	else
	{
		active_query_tab = apr_hash_make(pool);
		if (! active_query_tab)
		{
			gpmon_warning(FLINE, "Out of memory");
		}
		else
		{
			convert_tuples_to_hash(result, active_query_tab, pool);
		}
	}

	PQclear(result);
	PQfinish(conn);

	return active_query_tab;
}

const char *iconv_encodings[] = {
	NULL, // SQL_ASCII, not supported as server encoding.
	"EUC-JP",
	"EUC-CN",
	"EUC-KR",
	"EUC-TW",
	"EUC-JISX0213",
	"UTF8",
	NULL, // MULE_INTERNAL, not supported in iconv.
	"LATIN1",
	"LATIN2",
	"LATIN3",
	"LATIN4",
	"LATIN5",
	"LATIN6",
	"LATIN7",
	"LATIN8",
	"LATIN9",
	"LATIN10",
	"WINDOWS-1256",
	"WINDOWS-1258",
	NULL, // WIN866, not supported in iconv.
	"WINDOWS-874",
	"KOI8-R",
	"WINDOWS-1251",
	"WINDOWS-1252",
	"ISO_8859-5",
	"ISO_8859-6",
	"ISO_8859-7",
	"ISO_8859-8",
	"WINDOWS-1250",
	"WINDOWS-1253",
	"WINDOWS-1254",
	"WINDOWS-1255",
	"WINDOWS-1257",
	"KOI8-U",
	"SJIS",
	NULL, // BIG5, not supported in server encoding.
	NULL, // GBK, not supported in server encoding.
	NULL, // UHC, not supported in server encoding.
	NULL, // GB18030, not supported in server encoding.
	"JOHAB",
	NULL // SJIS, not supported in server encoding.
};

static const char* find_encoding(int encoding_num)
{
	// Because encodings here are in consistant with those
	// in gpdb, we have assertion here.
	ASSERT(encoding_num >= 0 &&
		   encoding_num < (sizeof(iconv_encodings) / sizeof(char*)));
	return iconv_encodings[encoding_num];
}

static bool get_encoding_from_result(PGresult	*result,
									 char		*encoding,
									 size_t		encoding_len,
									 int		*encoding_num)
{
	ASSERT(result);
	ASSERT(encoding);
	ASSERT(encoding_num);
	if (PQntuples(result) > 0)
	{
		const char* encoding_str = PQgetvalue(result, 0, 0);
		*encoding_num = atoi(encoding_str);
		const char *encoding_item = find_encoding(*encoding_num);
		if (encoding_item)
		{
			strncpy(encoding, encoding_item, encoding_len);
		}
		else
		{
			gpmon_warning(FLINE, "GPDB bad encoding: %d\n", *encoding_num);
			return false;
		}
	}
	else
	{
		TR0(("could not find owner for 'gpperfmon' database\n"));
		return false;
	}
	return true;
}

static bool gpdb_get_server_encoding(PGconn	*conn,
									 char	*encoding,
									 size_t	encoding_len,
									 int	*encoding_num)
{
	ASSERT(conn);
	ASSERT(encoding);
	ASSERT(encoding_num);

	PGresult *result = NULL;
	const char *query = "SELECT encoding FROM pg_catalog.pg_database "
						"d WHERE d.datname = 'gpperfmon'";
	const char* errmsg = gpdb_exec_only(conn, &result, query);
	bool ret = true;

	if (errmsg != NULL)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, query);
		ret = false;
	}
	else
	{
		ret = get_encoding_from_result(result,
									   encoding,
									   encoding_len,
									   encoding_num);
	}

	PQclear(result);
	return ret;
}

static bool create_alert_table_with_script(PGconn *conn, const char *encoding)
{
	ASSERT(conn);
	ASSERT(encoding);
	const char query_pattern[] = "BEGIN;"
		"DROP EXTERNAL TABLE IF EXISTS public.log_alert_tail;"
		"CREATE EXTERNAL WEB TABLE public.log_alert_tail (LIKE "
		"public.log_alert_history) EXECUTE 'gpperfmoncat.sh "
		"gpperfmon/logs/alert_log_stage 2> /dev/null || true' "
		"ON MASTER FORMAT 'csv' (DELIMITER e',' NULL e'' "
		"ESCAPE e'\"' QUOTE e'\"') ENCODING '%s';"
		"DROP EXTERNAL TABLE IF EXISTS public.log_alert_now;"
		"CREATE EXTERNAL WEB TABLE public.log_alert_now "
		"(LIKE public.log_alert_history) "
		"EXECUTE 'gpperfmoncat.sh gpperfmon/logs/*.csv 2> /dev/null "
		"|| true' ON MASTER FORMAT 'csv' (DELIMITER e',' NULL "
		"e'' ESCAPE e'\"' QUOTE e'\"') ENCODING '%s'; COMMIT;";

	char query[sizeof(query_pattern) + 100];
	snprintf(query, sizeof(query), query_pattern, encoding, encoding);

	return gpdb_exec_ddl(conn, query);
}

static bool create_alert_table_without_script(PGconn *conn, const char *encoding)
{
	ASSERT(conn);
	ASSERT(encoding);
	const char query_pattern[] = "BEGIN;"
		"DROP EXTERNAL TABLE IF EXISTS public.log_alert_tail;"
		"CREATE EXTERNAL WEB TABLE public.log_alert_tail (LIKE "
		"public.log_alert_history) EXECUTE 'iconv -f %s -t %s -c "
		"gpperfmon/logs/alert_log_stage 2> /dev/null || true' ON MASTER FORMAT "
		"'csv' (DELIMITER e',' NULL e'' ESCAPE e'\"' QUOTE e'\"') ENCODING '%s';"
		"DROP EXTERNAL TABLE IF EXISTS public.log_alert_now;"
		"CREATE EXTERNAL WEB TABLE public.log_alert_now (LIKE "
		"public.log_alert_history) EXECUTE 'iconv -f %s -t %s -c "
		"gpperfmon/logs/*.csv 2> /dev/null || true' ON MASTER FORMAT 'csv' "
		"(DELIMITER e',' NULL e'' ESCAPE e'\"' QUOTE e'\"') ENCODING '%s'; COMMIT;";

	char query[sizeof(query_pattern) + 100];
	snprintf(query, sizeof(query), query_pattern, encoding, encoding,
			 encoding, encoding, encoding, encoding);
	return gpdb_exec_ddl(conn, query);
}

static bool recreate_alert_tables_if_needed(PGconn *conn, const char *owner)
{
	ASSERT(conn);

	const int max_encoding_length = 20;
	char encoding[max_encoding_length];
	int encoding_num;
	bool success_get_encoding = gpdb_get_server_encoding(conn,
														 encoding,
														 sizeof(encoding),
														 &encoding_num);
	if (!success_get_encoding)
	{
		gpmon_warning(FLINE, "GPDB failed to get server encoding.\n");
		return false;
	}

	bool script_exist = (system("which gpperfmoncat.sh > /dev/null 2>&1") == 0);
	bool should_recreate = gpdb_should_recreate_log_alert(
														conn,
														"log_alert_tail",
														encoding,
														encoding_num,
														script_exist);
	if (should_recreate)
	{
		return (script_exist ?
				create_alert_table_with_script(conn, encoding) :
				create_alert_table_without_script(conn, encoding));
	}

	return true;
}

static bool gpdb_get_gpperfmon_owner(PGconn *conn, char *owner, size_t owner_length)
{
	ASSERT(conn);
	ASSERT(owner);

	PGresult *result = NULL;
	const char *query = "select pg_catalog.pg_get_userbyid(d.datdba) as "
						"owner from pg_catalog.pg_database d where "
						"d.datname = 'gpperfmon'";
	const char *errmsg = gpdb_exec_only(conn, &result, query);
	bool ret = true;

	if (errmsg != NULL)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, query);
		ret = false;
	}
	else
	{
		if (PQntuples(result) > 0)
		{
			const char* owner_field = PQgetvalue(result, 0, 0);
			strncpy(owner, owner_field, owner_length);
			ret = true;
		}
		else
		{
			TR0(("could not find owner for 'gpperfmon' database\n"));
			ret = false;
		}
	}
	PQclear(result);
	return ret;
}

static void gpdb_change_alert_table_owner(PGconn *conn, const char *owner)
{
	ASSERT(conn);
	ASSERT(owner);

	// change owner from gpmon, otherwise, gpperfmon_install
	// might quit with error when execute 'drop role gpmon if exists'
	const char* query_pattern = "ALTER TABLE public.log_alert_history owner to %s;"
								"ALTER EXTERNAL TABLE public.log_alert_tail "
								"owner to %s;ALTER EXTERNAL TABLE public.log_alert_now"
								" owner to %s;";
	const int querybufsize = 512;
	char query[querybufsize];
	snprintf(query, querybufsize, query_pattern, owner, owner, owner);
	TR0(("change owner to %s\n", owner));
	gpdb_exec_ddl(conn, query);
}

/*
 * Upgrade: alter distributed key of log_alert_history from logsegment to logtime
 */
void upgrade_log_alert_table_distributed_key(PGconn* conn)
{
	if (conn == NULL)
	{
		TR0(("Can't upgrade log_alert_history: conn is NULL\n"));
		return;
	}

	const char* qry = "SELECT d.nspname||'.'||a.relname as tablename, b.attname as distributed_key\
	    FROM pg_class  a\
	    INNER JOIN pg_attribute b on a.oid=b.attrelid\
	    INNER JOIN gp_distribution_policy c on a.oid = c.localoid\
	    INNER JOIN pg_namespace d on a.relnamespace = d.oid\
	    WHERE a.relkind = 'r' AND b.attnum = any(c.attrnums) AND a.relname = 'log_alert_history'";

	PGresult* result = NULL;
	const char* errmsg = gpdb_exec_only(conn, &result, qry);

	if (errmsg != NULL)
	{
		gpmon_warning(FLINE, "GPDB error %s\n\tquery: %s\n", errmsg, qry);
	}
	else
	{
		if (PQntuples(result) > 0)
		{
			// check if current distributed key is logsegment
			const char* current_distributed_key = PQgetvalue(result, 0, 1);
			if (current_distributed_key == NULL)
			{
				TR0(("could not find distributed key of log_alert_history\n"));
				PQclear(result);
				return;
			}
			if (strcmp(current_distributed_key, "logsegment") == 0)
			{
				TR0(("[INFO] log_alert_history: Upgrading log_alert_history table to use logsegment as distributed key\n"));
				qry = "alter table public.log_alert_history set distributed by (logtime);";
				gpdb_exec_ddl(conn, qry);
			}
		}
	}

	PQclear(result);
	return;
}



// to mitigate upgrade hassle.
void create_log_alert_table()
{
	PGconn *conn = PQconnectdb(GPDB_CONNECTION_STRING);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		gpmon_warning(FLINE,
			"error creating gpdb client connection to dynamically "
			"check/create gpperfmon partitions: %s",
		PQerrorMessage(conn));
		PQfinish(conn);
		return;
	}

	const char *qry= "SELECT tablename FROM pg_tables "
					"WHERE tablename = 'log_alert_history' "
					"AND schemaname = 'public' ;";

	const bool has_history_table = gpdb_exec_search_for_at_least_one_row(qry, conn);

	char owner[MAX_OWNER_LENGTH] = {};
	bool success_get_owner = gpdb_get_gpperfmon_owner(conn, owner, sizeof(owner));

	// log_alert_history: create table if not exist or alter it to use correct
	// distribution key.
	if (!has_history_table)
	{
		qry = "BEGIN; CREATE TABLE public.log_alert_history (LIKE "
			"gp_toolkit.__gp_log_master_ext) DISTRIBUTED BY (logtime) "
			"PARTITION BY range (logtime)(START (date '2010-01-01') "
			"END (date '2010-02-01') EVERY (interval '1 month')); COMMIT;";

		TR0(("sounds like you have just upgraded your database, creating"
			 " newer tables\n"));

		gpdb_exec_ddl(conn, qry);
	}
	else
	{
		/*
		* Upgrade: alter distributed key of log_alert_history from logsegment to logtime
		*/
		upgrade_log_alert_table_distributed_key(conn);
	}

	// log_alert_now/log_alert_tail: change to use 'gpperfmoncat.sh' from 'iconv/cat' to handle
	// encoding issue.
	if (recreate_alert_tables_if_needed(conn, owner))
	{
		if (success_get_owner)
		{
			gpdb_change_alert_table_owner(conn, owner);
		}
	}
	else
	{
		TR0(("recreate alert_tables failed\n"));
	}

	PQfinish(conn);
	return;
}
