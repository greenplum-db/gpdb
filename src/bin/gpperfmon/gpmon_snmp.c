#include <sys/param.h>

#include "gpmonlib.h"
#include "gpmondb.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "apr_hash.h"
#include "apr_file_io.h"
#include "emcconnect/api.h"

#define GPMMON_SNMP_COMMUNITY_NAME "public"
#define GPMMON_SNMP_PROTOCOL_VERSION SNMP_VERSION_2c
#define GPMMON_SNMP_OID_STRING_BUF_SIZE 512
#define GPMMON_SNMP_HOSTNAME_MAX_SIZE 512
#define GPMON_SNMP_ALLOW_REALLOC 0
#define GPMON_SNMP_CONFIG_MAX_STRING_LENGTH 128
#define GPMON_SNMP_CLEANUP_ALERT_STATUS_TABLE_INTERVAL 15 // every X reports loop through the alerts table and clear out any thing that is no longer in error
#define SNMP_ARRAY_DISPLAY_LENGTH 256
#define GPMON_SNMP_MAX_EMC_CONNECT_MSG_STRING_SIZE 1024
#define GPMON_EMC_CONNECT_POLLDIR "/opt/connectemc/poll"
#define GPMON_EMC_CONNECT_GPDB_FIND_PATTERN "GPMON_*.txt"
#define GPMON_EMC_CONNECT_MAX_PENDING_EVENTS 100000
#define GPMON_EMC_CONNECT_TESTINFO_ALERT_FILE "/opt/connectemc/poll/testinfo"
#define GPMON_EMC_CONNECT_TESTERROR_ALERT_FILE "/opt/connectemc/poll/testerror"

//****************************************************************************************//
// DECLARATIONS SECTION
// Put structure definitions, typedefs, globals, function declarations in this section
//****************************************************************************************//

// these are defined in order of good to bad
typedef enum GPDB_SNMP_STATUS_T
{
	GPDB_SNMP_STATUS_NORMAL=0,
	GPDB_SNMP_STATUS_UNKNOWN=1,
	GPDB_SNMP_STATUS_WARNING=2,
	GPDB_SNMP_STATUS_ERROR=3,
	GPDB_SNMP_STATUS_UNREACHABLE=4
} GPDB_SNMP_STATUS;

const char* snmp_status_prt(GPDB_SNMP_STATUS status)
{
	switch(status)
	{
		case GPDB_SNMP_STATUS_NORMAL:
			return "normal";
		case GPDB_SNMP_STATUS_WARNING:
			return "warning";
		case GPDB_SNMP_STATUS_ERROR:
			return "error";
		case GPDB_SNMP_STATUS_UNREACHABLE:
			return "unreachable";
		case GPDB_SNMP_STATUS_UNKNOWN:
		default:
			return "unknown";
	}
}

typedef void gpdb_snmp_format_function(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars);
typedef GPDB_SNMP_STATUS gpdb_status_code_conversion_function(int statusCodeIn, char** msgout);
typedef void gpdb_snmp_device_function(const char* category, host_t* hostp);

struct SnmpConfigEntry
{
	char category[GPMON_SNMP_CONFIG_MAX_STRING_LENGTH];
	host_t* hostp;
	gpdb_snmp_device_function* function;

	// linked list
	struct SnmpConfigEntry* next;
};

struct AlertStatusKey
{
	char hostname[GPMMON_SNMP_HOSTNAME_MAX_SIZE];
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE];
};
typedef struct AlertStatusKey AlertStatusKey;

struct AlertStatusData
{
	GPDB_SNMP_STATUS status;
};
typedef struct AlertStatusData AlertStatusData;




static struct 
{
	unsigned char snmpInitialized;
	struct SnmpConfigEntry* snmpConfigHead;
	struct SnmpConfigEntry* snmpConfigTail;
	gpdb_snmp_device_function* master_function;
	gpdb_snmp_device_function* segment_function;
	char outputdir[PATH_MAX];
	char lastreport_file[PATH_MAX];
	char lastreport_tmpfile[PATH_MAX];
	char hostlist_file[PATH_MAX];
	char hostlist_tmpfile[PATH_MAX];
	char emcconnect_tailfile[PATH_MAX];

	FILE* hostlist_fd;
	unsigned int emc_connect_events_pending_load;

	time_t lastreport_started;
	time_t lastreport_done;
	time_t last_health_harvest;
	unsigned int health_harvest_interval;
	char reporttime_string[GPMON_DATE_BUF_SIZE];

	snmp_module_params_t* params;

	apr_pool_t* alert_status_table_pool;
	apr_hash_t* alert_status_table;

	// these are per host vars reset for each host
	GPDB_SNMP_STATUS worst_status_per_host;
	FILE* hostfd;
	AlertStatusKey key;
	host_t* hostt;

} si = { 0 };

#define MAX_NIC_MAP_SIZE 16
#define MAX_POWER_PROBE_MAP_SIZE 8

static struct
{
	bool tenGigCard[MAX_NIC_MAP_SIZE];

} nic_map = { { 0 } }; 

static struct
{
	// map from index into table to type value
	unsigned int map[MAX_POWER_PROBE_MAP_SIZE];
} power_probe_map = { { 0 } };

static struct
{
	int dell_high_critical_temp_threshold;
	int dell_low_critical_temp_threshold;
} thermal_thresholds = {600, 0}; // Initialization to some extreme values (in tenths of centigrade)

void record_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, const char* oidstring, const char* displayname, GPDB_SNMP_STATUS status, const char* message);
int init_host(const char* hostname, host_t* host);
void write_line_in_hostlist_file(const char* category);
int snmp_array_collection(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, const char* oidname, const char* displayname_begin, const char* displayname_end, gpdb_snmp_format_function format_func, int retry_possible, int command);
int dual_port_host_snmp_array_collection(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, const char* oidname, const char* displayname_begin, const char* displayname_end, gpdb_snmp_format_function format_func, int command);
void close_snmp_file_for_host();
void remove_vertical_bar_from_null_terminated_string(char* str, unsigned int buffer_size);
char* get_accessible_hostname();
int mv_gpdb_emcconnect_records_to_perfmon();
void load_emcconnect_events_history();
int harvest_health_data_required(int num_dial_homes);

//****************************************************************************************//
// EMC CONNECT CONVERSION FUNCTIONS SECTION
// functions that convert from a type in SNMP monitoring to a type in EMC CONNECT
//****************************************************************************************//

EmcConnectStatus convert_gpdb_snmp_status_to_emc_connect_status(GPDB_SNMP_STATUS status)
{
	if (status == GPDB_SNMP_STATUS_UNREACHABLE)
		return EMC_CONNECT_STATUS_FAILED;
	else 
		return EMC_CONNECT_STATUS_OK;
}

EmcConnectSeverity convert_gpdb_snmp_status_to_emc_connect_severity(GPDB_SNMP_STATUS status)
{
	switch(status)
	{
		case GPDB_SNMP_STATUS_NORMAL:
			return EMC_CONNECT_SEVERITY_INFO;
		break;
		case GPDB_SNMP_STATUS_WARNING:
			return EMC_CONNECT_SEVERITY_WARNING;
		break;
		case GPDB_SNMP_STATUS_ERROR:
			return EMC_CONNECT_SEVERITY_ERROR;
		break;
		default:
		break;
	}

	return EMC_CONNECT_SEVERITY_UNKNOWN;
}

//****************************************************************************************//
// CONVERSION FUNCTIONS SECTION
// functions that convert a proprietary hardware device status into a GPDB SNMP status and message
// conversion functions are of type gpdb_status_code_conversion_function (see gpdb_status_code_conversion_function for parameters)
//****************************************************************************************//

GPDB_SNMP_STATUS brocade_8000_switch_sensor_status_code_conversion(int statusCodeIn, char** msgout)
{
    // unknown (1) 
    // other (2) 
    // ok (3) 
    // Warning (4)
    // failed (5)
	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
		break;

		case 2:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "other";
		break;

		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "ok";
		break;

		case 4:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Warning";
		break;

		case 5:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "failed";
		break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected status from device";
		break;
	}
	return status;
}

GPDB_SNMP_STATUS brocade_8000_switch_status_code_conversion(int statusCodeIn, char** msgout)
{
	//1. up
	//2. down
	//3. testing
	//4. faulty
	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "";
		break;

		case 2:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "down";
		break;

		case 3:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "testing";
		break;

		case 4:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "faulty";
		break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected status from device";
		break;
	}
	return status;
}

GPDB_SNMP_STATUS snmp_network_interface_status_code_conversion(int statusCodeIn, char** msgout)
{
	//1. up
	//2. down
	//3. testing
	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "up";
		break;

		case 2:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "down";
		break;

		case 3:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "testing";
		break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected status from device";
		break;
	}
	return status;
}

GPDB_SNMP_STATUS dell_network_device_connection_status_conversion(int statusCodeIn, char** msgout)
{
    //unknown(0) Unable to determine connection status. 
    //connected(1) Media reports that device is connected. 
    //disconnected(2) Media reports that device is disconnected. 
    //driverBad(3) Driver cannot be opened to determine status. 
    //driverDisabled(4) Driver is disabled. 
    //hardwareInitalizing(10) Hardware is initializing. 
    //hardwareResetting(11) Hardware is resetting. 
    //hardwareClosing(12) Hardware is closing down. 
    //hardwareNotReady(13) Hardware is not ready. 

	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 0:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
        break;

		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "connected";
        break;

		case 2:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "disconnected";
        break;

		case 3:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "driver bad";
        break;

		case 4:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "driver disabled";
        break;

		case 10:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware initalizing";
        break;

		case 11:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware resetting";
        break;

		case 12:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware closing";
        break;

		case 13:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware not ready";
        break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}
	return status;
}

// interprets same oid as dell_network_device_connection_status_conversion, except for an interface nic
GPDB_SNMP_STATUS dell_interconnect_device_connection_status_conversion(int statusCodeIn, char** msgout)
{
    //unknown(0) Unable to determine connection status. 
    //connected(1) Media reports that device is connected. 
    //disconnected(2) Media reports that device is disconnected. 
    //driverBad(3) Driver cannot be opened to determine status. 
    //driverDisabled(4) Driver is disabled. 
    //hardwareInitalizing(10) Hardware is initializing. 
    //hardwareResetting(11) Hardware is resetting. 
    //hardwareClosing(12) Hardware is closing down. 
    //hardwareNotReady(13) Hardware is not ready. 

	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 0:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
        break;

		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "connected";
        break;

		case 2:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "disconnected";
        break;

		case 3:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "driver bad";
        break;

		case 4:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "driver disabled";
        break;

		case 10:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware initalizing";
        break;

		case 11:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware resetting";
        break;

		case 12:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware closing";
        break;

		case 13:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "hardware not ready";
        break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}
	return status;
}



GPDB_SNMP_STATUS dell_status_code_conversion(int statusCodeIn, char** msgout)
{
		//other(1) The object’s status is not one of the following: 
		//unknown(2) The object’s status is unknown. 
		//ok(3) The object’s status is OK. 
		//nonCritical(4) The object’s status is warning, noncritical. 
		//critical(5) The object’s status is critical (failure). 
		//nonRecoverable(6) The object’s status is nonrecoverable (dead). :
	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "other";
		break;
		case 2:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
		break;
		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "";
		break;
		case 4:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "nonCritical";
		break;

		case 5:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "critical";
		break;

		case 6:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "nonRecoverable";
		break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected status from device";
		break;
	}
	return status;
}

GPDB_SNMP_STATUS dell_controller_status_code_conversion(int statusCodeIn, char** msgout)
{
	//0: Unknown 
	//1: Ready 
	//2: Failed 
	//3: Online 
	//4: Offline 
	//6: Degraded 

	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 0:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
		break;
		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;;
			*msgout = "";
		break;
		case 2:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Failed";
		break;
		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "";
		break;
		case 4:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Offline";
		break;
		case 6:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Degraded";
		break;
		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}

	return status;
}

GPDB_SNMP_STATUS lsi_virtual_disk_state_status_code_conversion(int statusCodeIn, char** msgout)
{
    // 0 offline
    // 1 partially-degraded
    // 2 degraded
    // 3 optimal

	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 0:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "offline";
		break;
		case 1:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "partially degraded";
		break;
		case 2:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "degraded";
		break;
		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "optimal";
		break;
		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
		break;
	}

	return status;

}

GPDB_SNMP_STATUS dell_virtual_disk_state_status_code_conversion(int statusCodeIn, char** msgout)
{
	//0: Unknown 
	//1: Ready - The disk is accessible and has no known problems. 
	//2: Failed - The data on the virtual disk is no longer fault tolerant because one of the underlying disks is not online. 
	//3: Online 
	//4: Offline - The disk is not accessible. The disk may be corrupted or intermittently unavailable. 
	//6: Degraded - The data on the virtual disk is no longer fault tolerant because one of the underlying disks is not online. 
	//15: Resynching 
	//16: Regenerating 
	//24: Rebuilding 
	//26: Formatting 
	//32: Reconstructing 
	//35: Initializing 
	//36: Background Initialization 
	//38: Resynching Paused 
	//52: Permanently Degraded 
	//54: Degraded Redundancy 

	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 0:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unknown";
		break;
		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "Ready";
		break;
		case 2:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Failed";
		break;
		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "Online";
		break;
		case 4:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Offline";
		break;
		case 6:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Degraded";
		break;
		case 15:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Resynching";
		break;
		case 16:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Regenerating";
		break;
		case 24:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Rebuilding";
		break;
		case 26:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Formatting";
		break;
		case 32:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Reconstructing";
		break;
		case 35:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Initialization";
		break;
		case 36:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Background Initialization";
		break;
		case 38:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Resynching Paused";
		;
		case 52:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Permanently Degraded";
		break;
		case 54:
			status = GPDB_SNMP_STATUS_ERROR;
			*msgout = "Degraded Redundancy";
		break;
		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}

	return status;
}


GPDB_SNMP_STATUS dell_virtual_disk_layout_code_conversion(int statusCodeIn, char** msgout)
{
	//1: Concatenated 
	//2: RAID-0 
	//3: RAID-1 
	//7: RAID-5 
	//8: RAID-6 
	//10: RAID-10 
	//12: RAID-50 
	//19: Concatenated RAID 1 
	//24: RAID-60 

	GPDB_SNMP_STATUS status;

	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "Concatenated";
		break;
		case 2:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-0";
		break;
		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-1";
		break;
		case 7:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-5";
		break;
		case 8:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-6";
		break;
		case 10:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-10";
		break;
		case 12:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-50";
		break;
		case 19:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "Concatenated RAID 1";
		break;
		case 24:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "RAID-60";
		break;
		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}

	return status;
}

GPDB_SNMP_STATUS dell_virtual_disk_read_policy_code_conversion(int statusCodeIn, char** msgout)
{
	GPDB_SNMP_STATUS status;

    //1: Enabled - Adaptec Read Cache Enabled 
    //2: Disabled - Adaptec Read Cache Disabled 
    //3: LSI Read Ahead 
    //4: LSI Adaptive Read Ahead 
    //5: LSI No Read Ahead 

	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Enabled Adaptec Read Cache Enabled";
		break;

		case 2:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Disabled Adaptec Read Cache Disabled";
		break;

		case 3:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "LSI Read Ahead";
		break;

		case 4:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "LSI Adaptive Read Ahead";
		break;

		case 5:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "LSI No Read Ahead";
		break;

		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}

	return status;
}


GPDB_SNMP_STATUS dell_virtual_disk_write_policy_code_conversion(int statusCodeIn, char** msgout)
{
	GPDB_SNMP_STATUS status;

	//1: Enabled - Adaptec Write Cache Enabled Protected 
	//2: Disabled - Adaptec Write Cache Disabled 
	//3: LSI Write Back 
	//4: LSI Write Through 
	//5: Enabled Always - (Adaptec only) 
	//6: Enabled Always - (SAS only) 
	
	switch(statusCodeIn)
	{
		case 1:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Enabled Adaptec Write Cache Enabled Protected";
		break;
		case 2:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Disabled Adaptec Write Cache Disabled";
		break;
		case 3:
			status = GPDB_SNMP_STATUS_NORMAL;
			*msgout = "LSI Write Back";
		break;
		case 4:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "LSI Write Through";
		break;
		case 5:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Enabled Always (Adaptec)";
		break;
		case 6:
			status = GPDB_SNMP_STATUS_WARNING;
			*msgout = "Enabled Always (SAS)";
		break;
		default:
			status = GPDB_SNMP_STATUS_UNKNOWN;
			*msgout = "unexpected code from device";
		break;
	}

	return status;
}

//****************************************************************************************//
// FORMAT FUNCTIONS SECTION
// functions that take the output from netsnmp and convert that into the output format for gpperfmon snmp reporting
// add new format functions to this section
// format functions are of type gpdb_snmp_format_function (see gpdb_snmp_format_function for parameters)
//****************************************************************************************//

#define FORMATIP_SIZE 64
void gpmon_format_snmp_ip(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	unsigned char *ip;
	char buffer[FORMATIP_SIZE] = { 0 };
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);

	if (vars->type != ASN_IPADDRESS)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}
	ip = vars->val.string;
	snprintf(buffer, FORMATIP_SIZE, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, buffer);
}

#define FORMAT_SNMP_INT_BUFSIZE 64
void gpmon_format_snmp_int(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	char outbuf[FORMAT_SNMP_INT_BUFSIZE] = { 0 };

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	snprintf(outbuf, FORMAT_SNMP_INT_BUFSIZE, "%ld", *vars->val.integer);
	record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, outbuf);
}

void gpmon_format_snmp_int_expect_value(int expected, GPDB_SNMP_STATUS status, EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	char outbuf[FORMAT_SNMP_INT_BUFSIZE] = { 0 };
	long int value;

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	value = *vars->val.integer;

	snprintf(outbuf, FORMAT_SNMP_INT_BUFSIZE, "%ld", value);

	if (value == expected)
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, outbuf);
	else
		record_status(symptom, dSymptom, oidstring, displayname, status, outbuf);
}

void gpmon_format_snmp_int_expect_range(long lowrange, long highrange, GPDB_SNMP_STATUS status, EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	char outbuf[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	long int value;

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	value = *vars->val.integer;


	if (value >= lowrange && value <= highrange)
    {
	    snprintf(outbuf, GPMMON_SNMP_OID_STRING_BUF_SIZE, "%ld", value);
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, outbuf);
    }
	else
    {
	    snprintf(outbuf, GPMMON_SNMP_OID_STRING_BUF_SIZE, "value %ld outside of range %ld to %ld", value, lowrange, highrange);
		record_status(symptom, dSymptom, oidstring, displayname, status, outbuf);
    }
}

void gpmon_format_disk_space(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	int df_value;
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);

	if (vars->type != ASN_INTEGER)
	{
        	record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
	        return;
	}

	df_value = *vars->val.integer;

	if (df_value >= si.params->error_disk_space_percentage) {
	        gpmon_format_snmp_int_expect_range(0, si.params->warning_disk_space_percentage - 1, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
	} else {
	        gpmon_format_snmp_int_expect_range(0, si.params->warning_disk_space_percentage - 1, GPDB_SNMP_STATUS_WARNING, symptom, dSymptom, oidname, oidlen, displayname, vars);
	}
}


void gpmon_format_snmp_int_warn_not_zero(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_value(0, GPDB_SNMP_STATUS_WARNING, symptom, dSymptom, oidname, oidlen, displayname, vars);
}


void gpmon_format_snmp_int_error_not_zero(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_value(0, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void format_max_temp_60_tenth_celcius(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_range(0, 600, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void format_max_temp_65_tenth_celcius(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_range(0, 650, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void format_max_temp_75_tenth_celcius(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_range(0, 750, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void format_max_temp_115_tenth_celcius(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_range(0, 1150, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void format_arista_fan_speed(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	gpmon_format_snmp_int_expect_range(5000, 18000, GPDB_SNMP_STATUS_ERROR, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void gpmon_store_thermal_threshold(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
        snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);

	if (vars->type != ASN_INTEGER)
        {
                record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
                return;
        }
	if (dSymptom == EMC_CONNECT_DETAILED_SYMPTOM_COOLING_DEVICE_SYSTEM_HIGH_THRESHOLD_TEMP) {
		thermal_thresholds.dell_high_critical_temp_threshold = *vars->val.integer;
		TR2(("Temperature high threshold %d %s\n", thermal_thresholds.dell_high_critical_temp_threshold, get_accessible_hostname()));
	} else if (dSymptom == EMC_CONNECT_DETAILED_SYMPTOM_COOLING_DEVICE_SYSTEM_LOW_THRESHOLD_TEMP) {
		thermal_thresholds.dell_low_critical_temp_threshold = *vars->val.integer;
		TR2(("Temperature low threshold %d %s\n", thermal_thresholds.dell_low_critical_temp_threshold, get_accessible_hostname()));
	} else {

		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "SNMP monitoring error : Unexpected symptom type");
                return;
	}
}

#define FORMAT_SNMP_CENT_BUFSIZE 64
void gpmon_format_dell_tenth_centigrade(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	char outbuf[FORMAT_SNMP_CENT_BUFSIZE] = { 0 };
	float farenheit = 0; 

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	// normally you multiply by 9/5 however and then add 32
	// our number is in tenths of degrees celcius ... so we use .18 instead of 9/5
	farenheit = (.18 * ((float)*vars->val.integer)) + 32.0;

	snprintf(outbuf, FORMAT_SNMP_CENT_BUFSIZE, "%0.2f", farenheit);
	record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, outbuf);
}

void gpmon_format_dell_network_device_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char* msg;
	int status;

	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	bool isInterconnectDevice = false;
	int nicIndex;

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	nicIndex = oidname[oidlen - 1];
    if (nicIndex < MAX_NIC_MAP_SIZE)
	{
		if (nic_map.tenGigCard[nicIndex])
			isInterconnectDevice = true;
	}

	if (isInterconnectDevice)
	{
		status = dell_interconnect_device_connection_status_conversion(*vars->val.integer, &msg);
	}
	else
	{
		status = dell_network_device_connection_status_conversion(*vars->val.integer, &msg);
	}

	record_status(symptom, dSymptom, oidstring, displayname, status, msg);
}

void format_generic_status_code(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars, gpdb_status_code_conversion_function conversion_func)
{
	char* msg;
	int status;

	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	status = conversion_func(*vars->val.integer, &msg);

	record_status(symptom, dSymptom, oidstring, displayname, status, msg);
}

void gpmon_format_dell_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, dell_status_code_conversion);
}

void format_dell_controller_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, dell_controller_status_code_conversion);
}

void format_dell_virtual_disk_layout(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, dell_virtual_disk_layout_code_conversion);
}

void format_dell_virtual_disk_state(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, dell_virtual_disk_state_status_code_conversion);
}

void format_dell_virtual_disk_write_policy(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, dell_virtual_disk_write_policy_code_conversion);
}

void format_dell_virtual_disk_read_policy(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, dell_virtual_disk_read_policy_code_conversion);
}

void format_network_interface_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, snmp_network_interface_status_code_conversion);
}

void format_brocade_8000_switch_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, brocade_8000_switch_status_code_conversion);
}

void format_brocade_8000_switch_sensor_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, brocade_8000_switch_sensor_status_code_conversion);
}

void format_lsi_virtual_disk_state(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	format_generic_status_code(symptom, dSymptom, oidname, oidlen, displayname, vars, lsi_virtual_disk_state_status_code_conversion);
}

#define FORMAT_DELL_SCALAR_STRING_BUF_SIZE 256
unsigned int calculate_snmp_string_size(unsigned int val_len, const char* oidstring)
{
	unsigned int snprintf_size = FORMAT_DELL_SCALAR_STRING_BUF_SIZE;
	if (val_len < FORMAT_DELL_SCALAR_STRING_BUF_SIZE)
	{
		snprintf_size = val_len + 1;

		// paranoid check ... should not get here
		if (snprintf_size < 1 || snprintf_size > FORMAT_DELL_SCALAR_STRING_BUF_SIZE)
		{
			// verbose debugging
			if (oidstring)
				gpmon_warningx(FLINE, 0, "buffer calculation warning on %s %s\n", si.key.hostname, oidstring);

			snprintf_size = FORMAT_DELL_SCALAR_STRING_BUF_SIZE;
		}
	}

	return snprintf_size;
}

void gpmon_format_snmp_string(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char buffer[FORMAT_DELL_SCALAR_STRING_BUF_SIZE] = { 0 };

	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);

	if (vars->type != ASN_OCTET_STR)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	unsigned int snprintf_size = calculate_snmp_string_size(vars->val_len, oidstring);

	// get rid of vertical bars
	remove_vertical_bar_from_null_terminated_string(buffer, snprintf_size);

	snprintf(buffer, snprintf_size, "%s", (char*)vars->val.string);
	record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, buffer);
}

void gpmon_format_expected_string(char* expected, int length, EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char buffer[FORMAT_DELL_SCALAR_STRING_BUF_SIZE] = { 0 };
	char buffer_final[FORMAT_DELL_SCALAR_STRING_BUF_SIZE] = { 0 };
    
    GPDB_SNMP_STATUS status = GPDB_SNMP_STATUS_NORMAL;

	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);

	if (vars->type != ASN_OCTET_STR)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	unsigned int snprintf_size = calculate_snmp_string_size(vars->val_len, oidstring);

	// get rid of vertical bars
	remove_vertical_bar_from_null_terminated_string(buffer, snprintf_size);

	snprintf(buffer, snprintf_size, "%s", (char*)vars->val.string);

    if (snprintf_size != length)
    {
        status = GPDB_SNMP_STATUS_ERROR;
    }
    else
    {
        if (strncmp(buffer, expected, length))
        {
            status = GPDB_SNMP_STATUS_ERROR;
        }
    }

    if (status != GPDB_SNMP_STATUS_NORMAL)
    {
	    snprintf(buffer_final, FORMAT_DELL_SCALAR_STRING_BUF_SIZE, "CURRENT: '%s' EXPECTED: '%s'", buffer, expected);
	    record_status(symptom, dSymptom, oidstring, displayname, status, buffer_final);
    }
    else
    {
	    record_status(symptom, dSymptom, oidstring, displayname, status, buffer);
    }
}

void format_lsi_virtual_disk_cachemode(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
    char expected[] = "WriteBack,ReadAdaptive,Cached,Write Cache OK if Bad BBU";
    int length = sizeof(expected);
    gpmon_format_expected_string(expected, length, symptom, dSymptom, oidname, oidlen, displayname, vars);
}

void gpmon_format_dell_volts(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	char outbuf[FORMAT_SNMP_INT_BUFSIZE] = { 0 };

	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	// number is millivolts -- thousanths of volts
	snprintf(outbuf, FORMAT_SNMP_INT_BUFSIZE, "%0.2f", .001 * (*vars->val.integer));

	record_status(symptom, dSymptom, oidstring, displayname, GPDB_SNMP_STATUS_NORMAL, outbuf);
}

// this format function does not generate any output
// it just stores the type of the Power Probe into a map by index
void gpmon_format_dell_power_probe_type(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	unsigned int index = oidname[oidlen - 1];
	unsigned int probeType;

	if (vars->type != ASN_INTEGER)
		return;

	if (index >= MAX_POWER_PROBE_MAP_SIZE)
		return;

	probeType = *vars->val.integer;

	// 23 for Type DellAmperageType == amperageProbeTypeIsPowerSupplyAmps
	// 26 for Type DellAmperageType == amperageProbeTypeIsSystemWatts
	if (probeType != 26 && probeType != 23)
		return;

	// this index into the table has Watts... 
	power_probe_map.map[index] = probeType;
}

// this format function uses a value set when querying the Amperage Probe table to determine correct index into the table to utilize
void gpmon_format_dell_power_probe(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
	snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, oidname, oidlen);
	char outbuf[FORMAT_SNMP_INT_BUFSIZE] = { 0 };
	unsigned int index = oidname[oidlen - 1];
	unsigned int probeType;

	// display buffer is normally provided but we will create our own for this field
	// since we dont know how to display until now
	char display_buffer[SNMP_ARRAY_DISPLAY_LENGTH] = { 0 };

	if (index >= MAX_POWER_PROBE_MAP_SIZE)
		return;

	probeType = power_probe_map.map[index];

	if (probeType == 23)
	{
		dSymptom = EMC_CONNECT_DETAILED_SYMPTOM_POWER_PROBE_PS_AMPS;
		snprintf(display_buffer, SNMP_ARRAY_DISPLAY_LENGTH, "Power Supply %u Amps", index);
	}
	else if (probeType == 26)
	{
		dSymptom = EMC_CONNECT_DETAILED_SYMPTOM_POWER_PROBE_SYSTEM_WATTS;
		snprintf(display_buffer, SNMP_ARRAY_DISPLAY_LENGTH, "System Watts Measurement");
	}
	else
	{
		return;
	}
		
	if (vars->type != ASN_INTEGER)
	{
		record_status(symptom, dSymptom, oidstring, display_buffer, GPDB_SNMP_STATUS_UNKNOWN, "wrong var type");
		return;
	}

	if (probeType == 23)
	{
		// number is 10ths of amps
		snprintf(outbuf, FORMAT_SNMP_INT_BUFSIZE, "%0.2f", .1 * (*vars->val.integer));
	}
	else if (probeType == 26)
	{
		snprintf(outbuf, FORMAT_SNMP_INT_BUFSIZE, "%ld", *vars->val.integer);
	}

	record_status(symptom, dSymptom, oidstring, display_buffer, GPDB_SNMP_STATUS_NORMAL, outbuf);
}

// this format function will both format and generate the output and
// store the state of which nic cards are 10 gig nic cards
void gpmon_format_dell_nic_card(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, oid oidname[], size_t oidlen, const char* displayname, struct variable_list *vars)
{
	unsigned int snprintf_size;
	char buffer[FORMAT_DELL_SCALAR_STRING_BUF_SIZE] = { 0 };
	unsigned int index;

	// update the nic mapping so we know which interfaces are the 10gig interconnect ports
	if (vars->type == ASN_OCTET_STR)
	{
		snprintf_size = calculate_snmp_string_size(vars->val_len, NULL);
		snprintf(buffer, snprintf_size, "%s", (char*)vars->val.string);
		if (strstr(buffer, "10Gbps"))
		{
			index = oidname[oidlen - 1];
			if (index < MAX_NIC_MAP_SIZE)
			{
				nic_map.tenGigCard[index] = true;
			}
		}
	}

	// reuse the generic string logic after extracting the necessary state
	gpmon_format_snmp_string(symptom, dSymptom, oidname, oidlen, displayname, vars);
}

//****************************************************************************************//
// DEVICE OID SECTION
// Change this code when adding a new device type 
// or modifying the oids to be collected.
// also in this section for each oid is listed a display name and format parameters
// device functions are of type gpdb_snmp_device_function (see gpdb_snmp_device_function for parameters)
//****************************************************************************************//

void run_snmp_report_for_dell_host(const char* categoryname, host_t* host)
{
	if (init_host(host->hostname, host))
		return;

	// for each dell host we have nic map to store information about the interfaces
	// this information is used as part of the status checking
	// we also have a power probe map that links power probe types to indexes into SNMP table
	memset(&nic_map, 0, sizeof(nic_map));
	memset(&power_probe_map, 0, sizeof(power_probe_map));

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_POWER_SUPPLY, EMC_CONNECT_DETAILED_SYMPTOM_POWER_SUPPLY_STATUS, "1.3.6.1.4.1.674.10892.1.600.12.1.5", "Power Supply", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_POWER_SUPPLY, EMC_CONNECT_DETAILED_SYMPTOM_POWER_PROBE_TYPE, "1.3.6.1.4.1.674.10892.1.600.30.1.7", "Power Probe", "Type", gpmon_format_dell_power_probe_type, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_POWER_SUPPLY, EMC_CONNECT_DETAILED_SYMPTOM_NULL, "1.3.6.1.4.1.674.10892.1.600.30.1.6", "Power Probe", "Value", gpmon_format_dell_power_probe, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_POWER_SUPPLY, EMC_CONNECT_DETAILED_SYMPTOM_POWER_PROBE_PS_VOLTS, "1.3.6.1.4.1.674.10892.1.600.20.1.6", "Power Supply Volts", NULL, gpmon_format_dell_volts, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_BATTERY, EMC_CONNECT_DETAILED_SYMPTOM_BATTERY_STATUS, "1.3.6.1.4.1.674.10892.1.600.50.1.5", "Battery", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_COOLING_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_COOLING_DEVICE_NAME, "1.3.6.1.4.1.674.10892.1.700.12.1.8", "Cooling Device", "Name", gpmon_format_snmp_string, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_COOLING_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_COOLING_DEVICE_STATUS, "1.3.6.1.4.1.674.10892.1.700.12.1.5", "Cooling Device", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_COOLING_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_COOLING_DEVICE_SYSTEM_TEMPERATURE, "1.3.6.1.4.1.674.10892.1.700.20.1.6", "System Temperature", NULL, gpmon_format_dell_tenth_centigrade, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_PROCESSOR, EMC_CONNECT_DETAILED_SYMPTOM_PROCESSOR_STATUS, "1.3.6.1.4.1.674.10892.1.1100.30.1.5", "Processor Status", NULL, gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_CACHE_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_CACHE_DEVICE_SIZE, "1.3.6.1.4.1.674.10892.1.1100.40.1.13", "Cache Device", "Size", gpmon_format_snmp_int, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_CACHE_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_CACHE_DEVICE_STATUS, "1.3.6.1.4.1.674.10892.1.1100.40.1.5", "Cache Device", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_OS_MEMORY, EMC_CONNECT_DETAILED_SYMPTOM_OS_MEMORY_STATUS, "1.3.6.1.4.1.674.10892.1.400.20.1.4", "OS Memory", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_MEMORY_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_MEMORY_DEVICE_STATUS, "1.3.6.1.4.1.674.10892.1.1100.50.1.5", "Memory Device", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_NETWORK_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_NETWORK_DEVICE_NAME, "1.3.6.1.4.1.674.10892.1.1100.90.1.5", "Network Device", "Name", gpmon_format_dell_nic_card, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_NETWORK_DEVICE, EMC_CONNECT_DETAILED_SYMPTOM_NETWORK_DEVICE_STATUS, "1.3.6.1.4.1.674.10892.1.1100.90.1.4", "Network Device", "Status", gpmon_format_dell_network_device_status, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_IO_CONTROLLER, EMC_CONNECT_DETAILED_SYMPTOM_CONTROLLER_NAME, "1.3.6.1.4.1.674.10893.1.20.130.1.1.2", "Controller", "Name", gpmon_format_snmp_string, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_IO_CONTROLLER, EMC_CONNECT_DETAILED_SYMPTOM_CONTROLLER_STATUS, "1.3.6.1.4.1.674.10893.1.20.130.1.1.5", "Controller", "Status", format_dell_controller_status, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_IO_CONTROLLER, EMC_CONNECT_DETAILED_SYMPTOM_CONTROLLER_BATTERY_STATUS, "1.3.6.1.4.1.674.10893.1.20.130.15.1.5", "Controller Battery", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;

	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_NAME, "1.3.6.1.4.1.674.10893.1.20.140.1.1.2", "Virtual Disk", "Name", gpmon_format_snmp_string, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_DEVICE_NAME, "1.3.6.1.4.1.674.10893.1.20.140.1.1.3", "Virtual Disk Device", "Name", gpmon_format_snmp_string, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_STATUS, "1.3.6.1.4.1.674.10893.1.20.140.1.1.19", "Virtual Disk", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_LAYOUT, "1.3.6.1.4.1.674.10893.1.20.140.1.1.13", "Virtual Disk", "Layout", format_dell_virtual_disk_layout, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_MB, "1.3.6.1.4.1.674.10893.1.20.140.1.1.6", "Virtual Disk", "MB", gpmon_format_snmp_int, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_WRITE_POLICY, "1.3.6.1.4.1.674.10893.1.20.140.1.1.10", "Virtual Disk", "Write Policy", format_dell_virtual_disk_write_policy, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_READ_POLICY, "1.3.6.1.4.1.674.10893.1.20.140.1.1.11", "Virtual Disk", "Read Policy", format_dell_virtual_disk_read_policy, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_VIRTUAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_VIRTUAL_DISK_STATE, "1.3.6.1.4.1.674.10893.1.20.140.1.1.4", "Virtual Disk",  "State", format_dell_virtual_disk_state, SNMP_MSG_GETNEXT)) goto BAIL;

    if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_PHYSICAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_ARRAY_DISK_NAME, "1.3.6.1.4.1.674.10893.1.20.130.4.1.2", "Array Disk", "Name", gpmon_format_snmp_string, SNMP_MSG_GETNEXT)) goto BAIL;
	if (dual_port_host_snmp_array_collection(EMC_CONNECT_SYMPTOM_PHYSICAL_DISK, EMC_CONNECT_DETAILED_SYMPTOM_ARRAY_DISK_STATUS, "1.3.6.1.4.1.674.10893.1.20.130.4.1.23", "Array Disk", "Status", gpmon_format_dell_status, SNMP_MSG_GETNEXT)) goto BAIL;

BAIL:
	close_snmp_file_for_host();
	write_line_in_hostlist_file(categoryname);
}


void run_snmp_report_for_brocade_vdx_switch(const char* categoryname, host_t* hostp)
{
	char* hostname = hostp->hostname;
	if (init_host(hostname, NULL))
		return;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_DESCRIPTION, "1.3.6.1.2.1.2.2.1.2", "Interface", "Description", gpmon_format_snmp_string, 0, SNMP_MSG_GETNEXT)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_STATUS, "1.3.6.1.2.1.2.2.1.8", "Interface", "Status", format_network_interface_status, 0, SNMP_MSG_GETNEXT)) goto BAIL;

BAIL:
	close_snmp_file_for_host();
	write_line_in_hostlist_file(categoryname);
}


void run_snmp_report_for_brocade_8000_switch(const char* categoryname, host_t* hostp)
{
	char* hostname = hostp->hostname;
	if (init_host(hostname, NULL))
		return;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_OPERATIONAL_STATUS, "1.3.6.1.4.1.1588.2.1.1.1.1.7", "Operational Status", NULL, format_brocade_8000_switch_status, 0, SNMP_MSG_GETNEXT)) goto BAIL;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_SENSOR_NAME, "1.3.6.1.3.94.1.8.1.3", "Sensor", "Name", gpmon_format_snmp_string, 0, SNMP_MSG_GETNEXT)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_SENSOR_STATUS, "1.3.6.1.3.94.1.8.1.4", "Sensor", "Status", format_brocade_8000_switch_sensor_status, 0, SNMP_MSG_GETNEXT)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_SENSOR_MESSAGE, "1.3.6.1.3.94.1.8.1.6", "Sensor", "Message", gpmon_format_snmp_string, 0, SNMP_MSG_GETNEXT)) goto BAIL;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_DESCRIPTION, "1.3.6.1.2.1.2.2.1.2", "Interface", "Description", gpmon_format_snmp_string, 0, SNMP_MSG_GETNEXT)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_STATUS, "1.3.6.1.2.1.2.2.1.8", "Interface", "Status", format_network_interface_status, 0, SNMP_MSG_GETNEXT)) goto BAIL;

BAIL:
	close_snmp_file_for_host();
	write_line_in_hostlist_file(categoryname);
}

void run_snmp_report_for_allied_telesis_switch(const char* categoryname, host_t* hostp)
{
	char* hostname = hostp->hostname;
	if (init_host(hostname, NULL))
		return;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_DESCRIPTION, "1.3.6.1.2.1.2.2.1.2", "Interface", "Description", gpmon_format_snmp_string, 0, SNMP_MSG_GETNEXT)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_STATUS, "1.3.6.1.2.1.2.2.1.8", "Interface", "Status", format_network_interface_status, 0, SNMP_MSG_GETNEXT)) goto BAIL;

BAIL:
	close_snmp_file_for_host();
	write_line_in_hostlist_file(categoryname);
}

void run_snmp_report_for_arista_7048_switch(const char* categoryname, host_t* hostp)
{
	char* hostname = hostp->hostname;
	if (init_host(hostname, NULL))
		return;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_DESCRIPTION, "1.3.6.1.2.1.2.2.1.2", "Interface", "Description", gpmon_format_snmp_string, 0, SNMP_MSG_GETNEXT)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_INTERFACE_STATUS, "1.3.6.1.2.1.2.2.1.8", "Interface", "Status", format_network_interface_status, 0, SNMP_MSG_GETNEXT)) goto BAIL;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FRONT_PANEL_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006001", "Front Panel Temp Tenths Degrees Celsius", NULL, format_max_temp_65_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_BOARD_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006002", "Board Temp Tenths Degrees Celsius", NULL, format_max_temp_60_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_PHY_CHIP_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006003", "Phy Chip 1 Temp Tenths Degrees Celsius", NULL, format_max_temp_115_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_PHY_CHIP_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006004", "Phy Chip 2 Temp Tenths Degrees Celsius", NULL, format_max_temp_115_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_PHY_CHIP_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006005", "Phy Chip 3 Temp Tenths Degrees Celsius", NULL, format_max_temp_115_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_PHY_CHIP_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006006", "Phy Chip 4 Temp Tenths Degrees Celsius", NULL, format_max_temp_115_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_PHY_CHIP_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006007", "Phy Chip 5 Temp Tenths Degrees Celsius", NULL, format_max_temp_115_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_PHY_CHIP_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006008", "Phy Chip 6 Temp Tenths Degrees Celsius", NULL, format_max_temp_115_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FAN_CONTROLLER_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006009", "Fan Controller 1 Temp Tenths Degrees Celsius", NULL, format_max_temp_75_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FAN_CONTROLLER_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006010", "Fan Controller 2 Temp Tenths Degrees Celsius", NULL, format_max_temp_75_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_BOARD_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006011", "Board Sensor 1 Temp Tenths Degrees Celsius", NULL, format_max_temp_75_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_BOARD_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006012", "Board Sensor 2 Temp Tenths Degrees Celsius", NULL, format_max_temp_75_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_BOARD_SENSOR, "1.3.6.1.2.1.99.1.1.1.4.100006013", "Board Sensor 3 Temp Tenths Degrees Celsius", NULL, format_max_temp_75_tenth_celcius, 0, SNMP_MSG_GET)) goto BAIL;

	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FAN_SPEED, "1.3.6.1.2.1.99.1.1.1.4.100601111", "Fan 1 Speed RPM", NULL, format_arista_fan_speed, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FAN_SPEED, "1.3.6.1.2.1.99.1.1.1.4.100602111", "Fan 2 Speed RPM", NULL, format_arista_fan_speed, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FAN_SPEED, "1.3.6.1.2.1.99.1.1.1.4.100603111", "Fan 3 Speed RPM", NULL, format_arista_fan_speed, 0, SNMP_MSG_GET)) goto BAIL;
	if (snmp_array_collection(EMC_CONNECT_SYMPTOM_SWITCH, EMC_CONNECT_DETAILED_SYMPTOM_SWITCH_FAN_SPEED, "1.3.6.1.2.1.99.1.1.1.4.100604111", "Fan 4 Speed RPM", NULL, format_arista_fan_speed, 0, SNMP_MSG_GET)) goto BAIL;

BAIL:
	close_snmp_file_for_host();
	write_line_in_hostlist_file(categoryname);
}


//****************************************************************************************//
// MAIN FLOW CODE SECTION
// Change this code when modifying the overall flow of the snmp reporting
//****************************************************************************************//

void remove_vertical_bar_from_null_terminated_string(char* str, unsigned int buffer_size)
{
	int i;
	for (i = 0; i < buffer_size; ++i)
	{
		if (str[i] == '\0')
			break;

		if (str[i] == '|')
		{
			str[i] = 0x20; // space char
		}
	}
}

// 0 is success
int alert_status_table_creation()
{
	int e;
	if (0 != (e = apr_pool_create_alloc(&si.alert_status_table_pool, 0)))
	{
		gpmon_warningx(FLINE, e, "unable to allocate memory pool from alert_status_table_creation in snmp module\n");
		return 1;
	}

    si.alert_status_table = apr_hash_make(si.alert_status_table_pool);
	if (!si.alert_status_table)
	{
		gpmon_warningx(FLINE, 0, "unable to allocate hash table from alert_status_table_creation in snmp module\n");
		apr_pool_destroy(si.alert_status_table_pool);
		si.alert_status_table_pool = NULL;
		return 1;
	}

	return 0;
}

apr_status_t snmp_msg_send(const char* hostname, struct snmp_pdu *pdu_to_send, struct snmp_pdu** pdu_response, char **error_msg)
{
	struct snmp_session session, *ss = NULL;
	apr_status_t retVal = APR_SUCCESS;
	int status;

	snmp_sess_init( &session );
	session.peername = (char*)hostname;
	session.version = GPMMON_SNMP_PROTOCOL_VERSION;
	session.community = (unsigned char*)GPMMON_SNMP_COMMUNITY_NAME;
	session.community_len = strlen((char*)session.community);
	ss = snmp_open(&session);

	if (!ss)
	{
		gpmon_warningx(FLINE, 0, "snmp_open session failure to host %s\n", hostname);
		return APR_ECONNABORTED;
	}

	status = snmp_synch_response(ss, pdu_to_send, pdu_response);

	if (status == STAT_TIMEOUT)
	{
		retVal = APR_ETIMEDOUT;
	}
	else if (status == STAT_SUCCESS)
	{
		if ((*pdu_response)->errstat == SNMP_ERR_NOSUCHNAME ||
			!((*pdu_response)->variables) ||
			(*pdu_response)->variables->type == SNMP_NOSUCHOBJECT ||
			(*pdu_response)->variables->type == SNMP_NOSUCHINSTANCE ||
			(*pdu_response)->variables->type == SNMP_ENDOFMIBVIEW)
		{
			retVal = APR_NOTFOUND;
		}
		if ((*pdu_response)->errstat != SNMP_ERR_NOERROR)
		{
			gpmon_warningx(FLINE, 0, "snmp_synch_response response has error '%s' from host %s\n", snmp_errstring((*pdu_response)->errstat), hostname);
		}
	}
	else
	{
		snmp_error(ss, 0, 0, error_msg);
		gpmon_warningx(FLINE, 0, "snmp_synch_response error %d from host %s\n", status, hostname);
		retVal = APR_EPROC_UNKNOWN;
	}

   	snmp_close(ss);
	return retVal;
}

int dual_port_host_snmp_array_collection(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, const char* oidname, const char* displayname_begin, const char* displayname_end, gpdb_snmp_format_function format_func, int command)
{
	int retcode = APR_ETIMEDOUT;
	int retry_possible;
	int i;

	for (i = 0; i < si.hostt->address_count; ++i)
	{
		retry_possible = ((i + 1) < si.hostt->address_count) ? 1 : 0;

		retcode = snmp_array_collection(symptom, dSymptom, oidname, displayname_begin, displayname_end, format_func, retry_possible, command);

		if ( retcode != APR_ETIMEDOUT )
		{	
			break;
		}

		// timeout trying to use this hostname, try the next name for same host
		advance_snmp_hostname(si.hostt);
	}

	return retcode;
}

int snmp_array_collection(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, const char* oidname, const char* displayname_begin, const char* displayname_end, gpdb_snmp_format_function format_func, int retry_possible, int command)
{
	apr_status_t retVal;
	oid originalOID[MAX_OID_LEN] = {};
	size_t originalOID_len = MAX_OID_LEN;
	size_t originalOID_comparebytes = 0;
	
	oid nextOID[MAX_OID_LEN] = {};
	size_t nextOID_len;

	char display_buffer[SNMP_ARRAY_DISPLAY_LENGTH];
	char* accessible_hostname;

	accessible_hostname = get_accessible_hostname();

	// initialize the display buffer in case no valid objects are found and we need to print an error using the display_buffer
	if (!displayname_end)
	{
		snprintf(display_buffer, SNMP_ARRAY_DISPLAY_LENGTH, "%s", displayname_begin);
	}
	else
	{
		snprintf(display_buffer, SNMP_ARRAY_DISPLAY_LENGTH, "%s %s", displayname_begin, displayname_end);
	}

	TR2(("collect %s %s from %s\n", display_buffer, oidname, accessible_hostname));

	read_objid(oidname, originalOID, &originalOID_len);
	originalOID_comparebytes = originalOID_len * sizeof(oid);

	int array_counter = 0; // how many items were in the array requested.... if 0 then we should should this as an error

	// start with original oid
	memmove(nextOID, originalOID, originalOID_comparebytes);
	nextOID_len = originalOID_len;
	
	char *snmp_error_msg = NULL;
	while (1)
	{
		struct snmp_pdu *pdu = NULL;
		struct snmp_pdu *response = NULL;
		int last_item_in_subtree;
	
        if (command == SNMP_MSG_GETNEXT)
            last_item_in_subtree = 0;
        else
            last_item_in_subtree = 1; // SNMP_MSG_GET does not travese a tree

		pdu = snmp_pdu_create(command);
		snmp_add_null_var(pdu, nextOID, nextOID_len);

		retVal = snmp_msg_send(accessible_hostname, pdu, &response, &snmp_error_msg);
	
		if (retVal == APR_SUCCESS)
		{
			if (response->variables->name_length < originalOID_len)
			{
				last_item_in_subtree = 1;
			}
			else if (memcmp(originalOID, response->variables->name, originalOID_comparebytes) != 0)
			{
				last_item_in_subtree = 1;
			}
			else
			{
				// append the index into array into the display name
				if (displayname_end)
				{
					snprintf(display_buffer, SNMP_ARRAY_DISPLAY_LENGTH, "%s %ld %s", displayname_begin, (long)response->variables->name[response->variables->name_length-1], displayname_end);
				}
				else
				{
					snprintf(display_buffer, SNMP_ARRAY_DISPLAY_LENGTH, "%s", displayname_begin);
				}

				format_func(symptom, dSymptom, response->variables->name, response->variables->name_length, display_buffer, response->variables);
				memmove(nextOID, response->variables->name, response->variables->name_length * sizeof(oid));
				nextOID_len = response->variables->name_length;
				array_counter++;
			}
		}
		else
		{
			// ERROR HANDLING 

			char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
			snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, nextOID, nextOID_len);

			if (retVal == APR_ETIMEDOUT)
			{
				// retry on another nic interface if possible
				if (!retry_possible)
				{
					record_status(EMC_CONNECT_SYMPTOM_HOSTUP, EMC_CONNECT_DETAILED_SYMPTOM_TIMEOUT, oidstring, display_buffer, GPDB_SNMP_STATUS_UNREACHABLE, "timeout");
				}
			}
			else if (retVal == APR_NOTFOUND)
			{
				record_status(EMC_CONNECT_SYMPTOM_SNMP, EMC_CONNECT_DETAILED_SYMPTOM_SNMP_CONFIGURATION, oidstring, display_buffer, GPDB_SNMP_STATUS_ERROR, "SNMP configuration issue on host");
			}
			else if (retVal == APR_EPROC_UNKNOWN)
			{
				record_status(EMC_CONNECT_SYMPTOM_SNMP, EMC_CONNECT_DETAILED_SYMPTOM_SNMP_OTHER, oidstring, display_buffer, GPDB_SNMP_STATUS_ERROR, "Got unexpected error looking for snmp OID");
			}
			else if (retVal == APR_ECONNABORTED)
			{
				record_status(EMC_CONNECT_SYMPTOM_HOSTUP, EMC_CONNECT_DETAILED_SYMPTOM_SNMP_CONNECTION_ABORTED, oidstring, display_buffer, GPDB_SNMP_STATUS_UNREACHABLE, "could not open session to host");
			}
			else
			{
				if (snmp_error_msg) {
					record_status(EMC_CONNECT_SYMPTOM_SNMP, EMC_CONNECT_DETAILED_SYMPTOM_SNMP_UNEXPECTED_ERROR, oidstring, display_buffer, GPDB_SNMP_STATUS_ERROR, snmp_error_msg);
				} else {
					record_status(EMC_CONNECT_SYMPTOM_SNMP, EMC_CONNECT_DETAILED_SYMPTOM_SNMP_UNEXPECTED_ERROR, oidstring, display_buffer, GPDB_SNMP_STATUS_ERROR, "Got unknown error looking for snmp OID");
				}
			}

			array_counter++; // we did not actually find an item, but we already displayed an error so we can increment to avoid printing error 2 x
			last_item_in_subtree = 1;
		}

		if (response)
		{
			snmp_free_pdu(response);
		}
		if (snmp_error_msg) {
                	free(snmp_error_msg);
			snmp_error_msg = NULL;
        	}

		if (last_item_in_subtree)
		{
			if (array_counter < 1)
			{
				char oidstring[GPMMON_SNMP_OID_STRING_BUF_SIZE] = { 0 };
				snprint_objid(oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, nextOID, nextOID_len);
				record_status(EMC_CONNECT_SYMPTOM_SNMP, EMC_CONNECT_DETAILED_SYMPTOM_SNMP_MISSING_OID, oidstring, display_buffer, GPDB_SNMP_STATUS_ERROR, "Data not found for expected snmp OID");
			}

			break;
		}
	}

	if (retVal == APR_ETIMEDOUT)
	{
		return APR_ETIMEDOUT;
	}
	return APR_SUCCESS;
}

void close_snmp_file_for_host()
{
	char filename[PATH_MAX] = { 0 };
	char filename_tmp[PATH_MAX] = { 0 };

	fclose(si.hostfd);

	snprintf(filename,     PATH_MAX, "%s/snmp.host.%s.txt",  si.outputdir, si.key.hostname);
	snprintf(filename_tmp, PATH_MAX, "%s/_snmp.host.%s.txt", si.outputdir, si.key.hostname);

	rename(filename_tmp, filename);
}
void update_per_host_status(GPDB_SNMP_STATUS componentStatus)
{
	if (componentStatus > si.worst_status_per_host)
		si.worst_status_per_host = componentStatus;
}

void record_status(EmcConnectSymptom symptom, EmcConnectDetailedSymptom dSymptom, const char* oidstring, const char* displayname, GPDB_SNMP_STATUS status, const char* message)
{
	AlertStatusData* alertData;
	int retVal;
	char msg_to_send[GPMON_SNMP_MAX_EMC_CONNECT_MSG_STRING_SIZE];
	bool sendAlert = false;

/*
-- TABLE: health_history
--   ctime                      event time
--   hostname                   hostname of system this metric belongs to
--   symptom_code               code for this type of event
--   detailed_symptom_code      finer grain symptom code
--   description                text description of event type
--   snmp_oid                   snmp oid of event type
--   status                     indication of status at time of event
--   message                    text message associated with this event
*/

	fprintf(si.hostfd, "%s|%s|%d|%d|%s|%s|%s|%s\n", si.reporttime_string, si.key.hostname, (int)symptom, (int)dSymptom, displayname,
							 oidstring, snmp_status_prt(status), message);
	update_per_host_status(status);

	// this oid does not have EMC CONNECT enabled
	if (symptom == EMC_CONNECT_SYMPTOM_NULL)
		return;

	// find the entry in the hash table
	snprintf(si.key.oidstring, GPMMON_SNMP_OID_STRING_BUF_SIZE, "%s", oidstring);
	alertData = apr_hash_get(si.alert_status_table, &si.key, sizeof (si.key));

	if (status == GPDB_SNMP_STATUS_NORMAL)
	{
		// was bad now good
		if (alertData)
		{
			apr_hash_set(si.alert_status_table, &si.key, sizeof(si.key), NULL); // remove the entry from table
			sendAlert = true;  // we send alerts when going bad to good
		}
	}
	else
	{
		// Status is not good

		if (!alertData)
		{
			// status was good add new entry to hash

			alertData = apr_palloc(si.alert_status_table_pool, sizeof(AlertStatusData));
			if (!alertData)
			{
				gpmon_warningx(FLINE, 0, "Memory not allocated in snmp module function record_status. EMC connect message not sent\n");
				return;
			}

			apr_hash_set(si.alert_status_table, &si.key, sizeof(si.key), alertData);
			alertData->status = status; // set the status
			sendAlert = true;
		}
		else
		{
			// Status was not good
			if (alertData->status != status)
			{
				alertData->status = status; // set the status
				sendAlert = true; // send alert for status change
			}
		}
	}

	/* emcconnect is off */
	if (si.params->emcconnect_mode == EMCCONNECT_MODE_TYPE_OFF)
	{
		sendAlert = false;
	}

	// send the alert to EMC Connect if required
	if (sendAlert)
	{
		snprintf(msg_to_send, GPMON_SNMP_MAX_EMC_CONNECT_MSG_STRING_SIZE, "%s: %s", displayname, message);

		// send EMC connect message
		retVal =  gpmon_emcconnect_send_alert(symptom, dSymptom,
								convert_gpdb_snmp_status_to_emc_connect_status(status),
								convert_gpdb_snmp_status_to_emc_connect_severity(status),
								EMC_CONNECT_ORIGIN_GPMMON,
								si.params->emcconnect_mode,
								GP_VERSION,
								(char*)displayname, 
								(char*)oidstring,
								si.key.hostname,  
								(char*)msg_to_send);

		if (retVal)
		{
			gpmon_warningx(FLINE, 0, "Error (%d) creating EMC Connect Event File from snmp module", retVal);
			fflush(stdout);
		}
	}
}

void process_connect_emc_test_alert()
{
	FILE* fd;
	int retVal = 0;

	/* emcconnect is off */
	if (si.params->emcconnect_mode == EMCCONNECT_MODE_TYPE_OFF)
	{
		return;
	}

	fd = fopen(GPMON_EMC_CONNECT_TESTINFO_ALERT_FILE, "r");
	if (fd)
	{
		fclose(fd);

		unlink(GPMON_EMC_CONNECT_TESTINFO_ALERT_FILE);

		retVal =  gpmon_emcconnect_send_alert(EMC_CONNECT_SYMPTOM_TEST, EMC_CONNECT_DETAILED_SYMPTOM_NULL,
								EMC_CONNECT_STATUS_OK,
								EMC_CONNECT_SEVERITY_INFO,
								EMC_CONNECT_ORIGIN_GPMMON,
								si.params->emcconnect_mode,
								GP_VERSION,
								"EMC Connect Test",
								(char*)"",
								"mdw",
								(char*)"EMC Connect Test Info Alert");

		if (retVal)
		{
			gpmon_warningx(FLINE, 0, "Error (%d) creating EMC Connect Event File from snmp module", retVal);
			fflush(stdout);
		}
	}

	fd = fopen(GPMON_EMC_CONNECT_TESTERROR_ALERT_FILE, "r");
	if (fd)
	{
		fclose(fd);

		unlink(GPMON_EMC_CONNECT_TESTERROR_ALERT_FILE);

		retVal =  gpmon_emcconnect_send_alert(EMC_CONNECT_SYMPTOM_TEST, EMC_CONNECT_DETAILED_SYMPTOM_NULL,
								EMC_CONNECT_STATUS_OK,
								EMC_CONNECT_SEVERITY_ERROR,
								EMC_CONNECT_ORIGIN_GPMMON,
								si.params->emcconnect_mode,
								GP_VERSION,
								"EMC Connect Test",
								(char*)"",
								"mdw",
								(char*)"EMC Connect Test Error Alert");

		if (retVal)
		{
			gpmon_warningx(FLINE, 0, "Error (%d) creating EMC Connect Event File from snmp module", retVal);
			fflush(stdout);
		}
	}
}

FILE* open_snmp_file_for_host(const char* hostname)
{
	FILE* fd;
	char filename[PATH_MAX] = { 0 };
	snprintf(filename, PATH_MAX, "%s/_snmp.host.%s.txt", si.outputdir, hostname);
	fd = fopen(filename, "w");
	if (!fd)
	{
		gpmon_warningx(FLINE, 0, "could not open snmp data file: %s", filename);
	}
	return fd;
}

// 0 is success
int init_host(const char* hostname, host_t* host)
{
	si.hostt = host;

	snprintf(si.key.hostname, GPMMON_SNMP_HOSTNAME_MAX_SIZE, "%s", hostname);

	si.worst_status_per_host = GPDB_SNMP_STATUS_NORMAL;

	si.hostfd = open_snmp_file_for_host(hostname);
	if (!si.hostfd)
		return 1;

	return 0;
}

char* get_accessible_hostname()
{
	if (!si.hostt)
	{
		return si.key.hostname;
	}
	else
	{
		return get_snmp_hostname(si.hostt);
	}
}

FILE* snmp_open_hostlist_file()
{
	if (si.hostlist_fd)
	{
		gpmon_warningx(FLINE, 0, "attempt to reopen already opened snmp hostlist file not expected");
	}

	si.hostlist_fd = fopen(si.hostlist_tmpfile, "w");
	return si.hostlist_fd;
}

void write_line_in_hostlist_file(const char* category)
{
	if (si.hostlist_fd)
	{
		fprintf(si.hostlist_fd, "%s|%s|%s\n", category, si.key.hostname, snmp_status_prt(si.worst_status_per_host));
	}
	else
	{
		gpmon_warningx(FLINE, 0, "unexpected closed hostlist file\n");
	}
}

void snmp_close_hostlist_file()
{
	if (si.hostlist_fd)
	{
		fclose(si.hostlist_fd);
		si.hostlist_fd = NULL;
		rename(si.hostlist_tmpfile, si.hostlist_file);
	}
	else
	{
		gpmon_warningx(FLINE, 0, "attempt to close non-open snmp hostlist file not expected");
	}
}

void snmp_close_tmphostlist_file()
{
	if (si.hostlist_fd)
	{
		fclose(si.hostlist_fd);
		si.hostlist_fd = NULL;
	}
}


void addSnmpConfigEntry(const char* category, const char* host, gpdb_snmp_device_function* function)
{
	struct SnmpConfigEntry* newEntry = NULL;

	// use calloc for initialization to 0
	newEntry = calloc(1, sizeof(struct SnmpConfigEntry));
	CHECKMEM(newEntry);

	// initialize list of hostnames with only a single hostname for snmp
	newEntry->hostp = calloc(1, sizeof(struct host_t)); 
	CHECKMEM(newEntry->hostp);
	newEntry->hostp->addressinfo_head = newEntry->hostp->addressinfo_tail = calloc(1, sizeof(struct addressinfo_holder_t));
	CHECKMEM(newEntry->hostp->addressinfo_head);
	newEntry->hostp->hostname = strdup(host);
	CHECKMEM(newEntry->hostp->hostname);
	newEntry->hostp->addressinfo_head->address = strdup(host);
	CHECKMEM(newEntry->hostp->addressinfo_head->address);
	newEntry->hostp->snmp_hostname.current = newEntry->hostp->addressinfo_head;
	newEntry->hostp->snmp_hostname.counter = 1;
	newEntry->hostp->address_count = 1;

	snprintf(newEntry->category, GPMON_SNMP_CONFIG_MAX_STRING_LENGTH, "%s", category);
	newEntry->function = function;

	if (si.snmpConfigTail)
	{
		si.snmpConfigTail->next = newEntry;
		si.snmpConfigTail = newEntry;
	}
	else
	{
		si.snmpConfigHead = si.snmpConfigTail = newEntry;
	}
}
void write_lastreport_status()
{
	FILE* fd = fopen(si.lastreport_tmpfile, "w");
	if (!fd)
	{
		gpmon_warningx(FLINE, 0, "could not open snmp lastreport file: %s", si.lastreport_tmpfile);
		return;
	}

	fprintf(fd, "%" FMT64 "|%" FMT64 "\n", (apr_int64_t)si.lastreport_started, (apr_int64_t)si.lastreport_done);
	fclose(fd);
	rename(si.lastreport_tmpfile, si.lastreport_file);
}

void snmp_start_report()
{
	si.lastreport_started = time(NULL);

	// update string that is used to print time of this report
	if (!gpmon_datetime(si.lastreport_started, si.reporttime_string))
	{	
		si.reporttime_string[0] = 0;
	}
	
	write_lastreport_status();
}

void snmp_done_report()
{
	si.lastreport_done = time(NULL);
	write_lastreport_status();
}

/* seperate thread for snmp */
/* 0 is success */
int snmp_report(host_t* tab, int tabsz)
{
	int i;
	int num_dial_homes;
	
	struct SnmpConfigEntry* iter;


	if (!si.snmpInitialized)
		return 1; // there was a failure in the setup phase ... do not collect data

	if (si.params->healthmon_running_separately) {
		goto harvest;
	}

	snmp_start_report();

	if (!snmp_open_hostlist_file())
	{
		gpmon_warningx(FLINE, 0, "could not open snmp hostlist output file(%s) not updating health or connectemc info", si.hostlist_file);
		return 2;
	}

	process_connect_emc_test_alert();

	// call snmp hosts from config file
	iter = si.snmpConfigHead;
	while (iter)
	{
		iter->function(iter->category, iter->hostp);
		iter = iter->next;
	}

	// call snmp hosts from gp_segment_configuration
	for (i = 0; i < tabsz; i++)
	{
		if (tab[i].is_hdw)
			continue;
		if (tab[i].is_hbw)
			continue;
		if (tab[i].is_hdc)
			continue;
		if (tab[i].is_hdm)
			continue;
		if (tab[i].is_etl)
			continue;
		
		if (tab[i].is_master)
		{
			si.master_function("Master Server", &tab[i]);
		}
		else
		{
			si.segment_function("Segment Server", &tab[i]);
		}
	}

	snmp_close_hostlist_file();

harvest:

	num_dial_homes = mv_gpdb_emcconnect_records_to_perfmon();

	if (si.emc_connect_events_pending_load)
	{
		load_emcconnect_events_history();
	}

	if (harvest_health_data_required(num_dial_homes))
	{
		gpdb_harvest_healthdata();
		si.last_health_harvest = time(NULL);
	}

	if (!si.params->healthmon_running_separately) {
		snmp_done_report();
	}

	return 0;
}

// 1 is yes, 0 is false
int harvest_health_data_required(int num_dial_homes)
{
	time_t now = time(NULL);

	if (num_dial_homes)
	{
		return 1;
	}

	if (now > si.last_health_harvest + si.params->health_harvest_interval)
	{
		return 1;
	}
	
	return 0;
}

void load_emcconnect_events_history()
{
	int r = 0;
	int i;
	static apr_status_t retVal;

	// run the SQL
	retVal = gpdb_harvest_one("emcconnect");
	if (retVal)
	{
		return;
	}

	// try to delete 10 times, there is no reason the file should not be able to be deleted
	for (i = 0; i < 10; ++i)
	{
		r = unlink(si.emcconnect_tailfile);
		if (!r)
		{
			break;
		}
		sleep(2);
	}

	if (r)
	{
		gpmon_warningx(FLINE, 0, "could not delete emcconnect staging file %s.  Duplicate events may be loaded to connectemc_history table", 
			si.emcconnect_tailfile);
	}
	
	si.emc_connect_events_pending_load = 0;
}

// return number of records transferred
int mv_gpdb_emcconnect_records_to_perfmon()
{	 
	int e=0;
	FILE* fp = NULL;
	int r;
	apr_status_t ret;
	int count = 0;
	const int cmdbuflen = 512;
	char cmd[cmdbuflen];
	snprintf(cmd, cmdbuflen, "find %s -name %s", 
		GPMON_EMC_CONNECT_POLLDIR, 
		GPMON_EMC_CONNECT_GPDB_FIND_PATTERN);

	fp = popen(cmd, "r");
	if (!fp)
	{
		gpmon_warningx(FLINE, APR_FROM_OS_ERROR(errno), 
			"error running find command to locate emc connect files: %s",
			cmd);
		return 0;
	}

	// for each file do:
	// 	mv file from /opt/connectemc/poll to $MASTER_DATA_DIRECTORY/data
	//	append file to external table file
	// 	rm file
	TR2(("Running command: %s\n", cmd));

	while ( 1 )
	{
		char line[1024] = { 0 };
		char* p;

		line[sizeof(line) - 1] = 0;
		if (! (p = fgets(line, sizeof(line), fp)))
			break;
		if (line[sizeof(line) - 1])
			continue; 	/* fname too long */

		p = gpmon_trim(p);

		if ( si.emc_connect_events_pending_load >= GPMON_EMC_CONNECT_MAX_PENDING_EVENTS)
		{
			gpmon_warningx(FLINE, e, "too many pending EMC connect events .... stop buffering and some events may not be recorded in history table");
			continue;
		}

		apr_pool_t* pool = NULL;

		if (0 != (e = apr_pool_create_alloc(&pool, 0)))
		{
			gpmon_warningx(FLINE, e, "apr_pool_create_alloc failed while processing emc connect history");
			pclose(fp);
			return 0;
		}

		ret = apr_file_append(p, si.emcconnect_tailfile, APR_OS_DEFAULT, pool);	
		apr_pool_destroy(pool);
		pool = NULL;

		if (ret != APR_SUCCESS)
		{
			gpmon_warningx(FLINE, ret, "failed appending file %s to %s", p, si.emcconnect_tailfile);
			continue;
		}

		count++;
		si.emc_connect_events_pending_load++;

		r = unlink(p);
		if (r)
		{
			gpmon_warningx(FLINE, 0, "failed removing file %s", p);
		}
	}

	pclose(fp);

	TR2(("found %d files\n", count));

	return count;
}


// return 0 for success 1 for failure
int find_token_in_config_string(char* buffer, char**result, const char* token)
{
	char lookfor[128];
	char* end;

	snprintf(lookfor, 128, "%s=\'", token);
	*result = strstr(buffer, lookfor);

	if (!*result)
		return 1;
	
	// within string find first single quote
	*result = strchr(*result, '\'');
	if (!result)
		return 1;

	// advance beyond first single quote
	(*result)++;

	if (!*result)
		return 1;

	// within string find second single quote
	end = strchr(*result, '\'');
	if (!end)
		return 1;

	// set second single quote to null
	*end = '\0';
	return 0;
}

void parse_snmp_config_line(char* line)
{
	// a line starting with a # is a comment
	// Within a line we look for following tokens in order:
	// 		Category='FOO'
	// 		Device='FOO'
	// 		Host='FOO'
	// Here is an exmaple:
	//Category='Admin Switch'         Device='AdminSwitch0001'   		  Host='a-sw-1-app1'
	//Category='Interconnect Switch'  Device='InterconnectSwitch0001'     Host='i-sw-1-app1'
	//Category='Interconnect Switch'  Device='InterconnectSwitch0001'     Host='i-sw-2-app1'
	//Category='ETL Host'             Device='EtlHost'                    Host='etl1'

	char* host;
	char* device;
	char* category;

	if (!line)
		return;

	if (line[0] == '#')
		return;

	// we do these in reverse order so inserting null chars does not prevent finding other tokens
	if (find_token_in_config_string(line, &host, "Host"))
		return;

	if (find_token_in_config_string(line, &device, "Device"))
		return;

	if (find_token_in_config_string(line, &category, "Category"))
		return;

	gpdb_snmp_device_function* fPtr = NULL;

	if (strcmp(device, "InterconnectSwitch0001") == 0) // Brocade 8000
	{
		fPtr = run_snmp_report_for_brocade_8000_switch;
	}
	else if (strcmp(device, "InterconnectSwitch0002") == 0) // Brocade VDX Switch
	{
		fPtr = run_snmp_report_for_brocade_vdx_switch;
	}
	else if (strcmp(device, "AdminSwitch0001") == 0) // Allied Telesis TL
	{
		fPtr = run_snmp_report_for_allied_telesis_switch;
	}
	else if (strcmp(device, "EtlHost") == 0) // EtlHost
	{    
		fPtr = run_snmp_report_for_dell_host;
	}    

	if (!fPtr)
	{
		gpmon_warningx(FLINE, 0, "snmp config line has unrecognized device.  CATEGORY='%s' DEVICE='%s' HOST='%s'\n", category, device, host);
		return;
	}

	addSnmpConfigEntry(category, host, fPtr);
}

void read_snmp_config(const char* configfile)
{
	char buffer[1024] = { 0 };
	char *line = NULL;
	FILE *fp = NULL;
	struct SnmpConfigEntry* iter;

	// if user does not specify we assume the original DCA machines
	// in the future we should remove this and force the config to have a value
	// it is here so we don't have to update the file
	si.segment_function = run_snmp_report_for_dell_host;
	si.master_function = run_snmp_report_for_dell_host;

	if (!configfile)
	{
		gpmon_warningx(FLINE, 0, "no snmp config file.  Only report SNMP data for segment and master servers.");
		return;
	}

	fp = fopen(configfile, "r");
	if (!fp)
	{
		gpmon_warningx(FLINE, 0, "could not open snmp config file: %s", configfile);
		return;
	}

	while (NULL != fgets(buffer, 1024, fp))
	{
		// remove new line 
		line = gpmon_trim(buffer);
		parse_snmp_config_line(line);
	}

	fclose(fp);

	iter = si.snmpConfigHead;
	while (iter)
	{
		TR0(("SNMP COLLECTION CATEGORY: '%s' HOST: '%s' func: '%p'\n", iter->category, iter->hostp->hostname, iter->function));
		iter = iter->next;
	}
}

void read_state_from_last_report_file()
{
	char buffer[1024] = { 0 };
	FILE* fd = fopen(si.lastreport_file, "r");
	char* p;
	if (!fd)
		return; // can't open file, no state to obtain
	
	while (NULL != fgets(buffer, 1024, fd))
	{
		p = strchr(buffer, '|');
		if (!p)
			return; // did not find expected delimiter

		p++;

		si.lastreport_done = atoi(p);

		break; // only read one line
	}

	fclose(fd);
}

apr_status_t gpmmon_init_snmp(snmp_module_params_t* params)
{
	si.params = params;

	if (!si.params->healthmon_running_separately)
		read_snmp_config(params->configfile);

	snprintf(si.outputdir, 		PATH_MAX, "%s", si.params->outdir);
	snprintf(si.lastreport_file,    PATH_MAX, "%s/lastreport.txt", si.params->outdir);
	snprintf(si.lastreport_tmpfile, PATH_MAX, "%s/_lastreport.txt", si.params->outdir);
	snprintf(si.hostlist_file,  	PATH_MAX, "%s/hostlistreport.txt", si.params->outdir);
	snprintf(si.hostlist_tmpfile, 	PATH_MAX, "%s/_hostlistreport.txt", si.params->outdir);
	snprintf(si.emcconnect_tailfile,PATH_MAX, "%s/_emcconnect_tail.dat", si.params->externaltable_dir);

	if (gpmon_recursive_mkdir(si.outputdir))
	{
		gpmon_warningx(FLINE, 0, "could not make the output directory for snmp data: %s", si.outputdir);
		return APR_ENOMEM;
	}
	
	read_state_from_last_report_file();

	if (!si.params->healthmon_running_separately)
	{
		init_snmp("gpmmon");
		snmp_out_toggle_options("n"); // make output of oids numeric only
	}

	if (alert_status_table_creation())
		return APR_ENOMEM;

	si.snmpInitialized=true;


	if (si.params->healthmon_running_separately) {
		TR0(("Not doing regular health monitoring..\n"));
	} else {
		TR0(("Performing the regular health monitoring..\n"));
	}	

	return APR_SUCCESS;
}

static int
compare_versions(char *first, char *second)
{
        char *p = first;
        char *q = second;


        int num_parts_cmp;
        int result = 0;

        if (!first || !second) {
                TR0(("Invalid argumnets to compare_versions"));
                goto bail;
        }

        /* Find parts in each version number. Each '.' adds a new part. */
        int parts_in_first = 1; // We have at least one part to compare in a non null string
        while (p && *p) {
                p = strchr(p, '.');
                if (p) {
                        p = p + 1;
                        parts_in_first++;
                }
        }

        int parts_in_second = 1;
        while (q && *q) {
                q = strchr(q, '.');
                if (q) {
                        q = q + 1;
                        parts_in_second++;
                }
        }

        num_parts_cmp = (parts_in_first > parts_in_second) ? parts_in_second : parts_in_first;

        TR0(("Parts to compare are %d\n", num_parts_cmp));

        p = first;
        q = second;

        int first_num;
        int second_num;
        int counter = 0;

        while (counter < num_parts_cmp) {

                first_num = atoi(p);
                second_num = atoi(q);

                TR0(("Comparing %d and %d\n", first_num, second_num));
		if (first_num > second_num) {
                        result = 1;
                        goto bail;
                } else if (first_num < second_num) {
                        result = -1;
                        goto bail;
                }

                p = strchr(p, '.');
                if (p) {
                        p = p + 1;
                }

                q = strchr(q, '.');
                if (q) {
                        q = q + 1;
                }
                counter++;
        }


bail:
        /* Debugging purposes */
        if (result == 1) {
                TR0(("%s is greater than %s\n", first, second));
        } else if (result == -1) {
                TR0(("%s is less than %s\n", first, second));
        } else {
                TR0(("%s is equal to %s\n", first, second));
        }
        return result;
}




bool
is_healthmon_running_separately()
{
	// New ISO versions have separate healthmond
	bool new_ver_iso = false;
	char buffer[MAXPATHLEN+1];
	char *newline;

        FILE* fd = fopen(PATH_TO_APPLIANCE_VERSION_FILE, "r");
	if (!fd) {
		TR0(("Can't open %s", PATH_TO_APPLIANCE_VERSION_FILE));
		goto bail;
	}

	buffer[0] = 0;	
	if (fgets(buffer, 1024, fd)) {
		buffer[MAXPATHLEN] = 0;
	} else {
		goto bail;
	}

	newline = strchr(buffer, '\n');
	if (newline) {
		*newline = 0;
	}

	// TODO - Place this in some macro
	if (compare_versions(buffer, "1.1") >= 0) {
		new_ver_iso = true;
	}

        fclose(fd);
bail:
	return new_ver_iso;
}

