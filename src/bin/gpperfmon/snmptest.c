#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#define GPMMON_SNMP_COMMUNITY_NAME "public"
#define GPMMON_SNMP_PROTOCOL_VERSION SNMP_VERSION_2c
#define GPMMON_SNMP_OID_STRING_BUF_SIZE 512

int main(int argc, char** argv)
{
	const char* hostname = "localhost";
	const char* oidname = "1.3.6.1.4.1.674.10892.1.600.60.1.8.1.1";

	init_snmp("gpmmon");
	snmp_out_toggle_options("n"); // make output of oids numeric only

	struct snmp_pdu *pdu = NULL;
	struct snmp_pdu *response = NULL;

	oid anOID[MAX_OID_LEN] = {};
	size_t anOID_len = MAX_OID_LEN;
   
	pdu = snmp_pdu_create(SNMP_MSG_GET);
	read_objid(oidname, anOID, &anOID_len);
	snmp_add_null_var(pdu, anOID, anOID_len);

	struct snmp_session session, *ss = NULL;
	int status;

	snmp_sess_init( &session );
	session.peername = (char*)hostname;
	session.version = GPMMON_SNMP_PROTOCOL_VERSION;
	session.community = (unsigned char*)GPMMON_SNMP_COMMUNITY_NAME;
	session.community_len = strlen((char*)session.community);
	ss = snmp_open(&session);

	if (!ss)
	{
		fprintf(stdout, "error opening session\n");
		exit(0);
	}

	status = snmp_synch_response(ss, pdu, &response);

	if (status == STAT_TIMEOUT)
	{
		fprintf(stdout, "timeout\n");
	}
	else if (status == STAT_SUCCESS)
	{
		print_variable(response->variables->name, response->variables->name_length, response->variables);
	}
	else
	{
		fprintf(stdout, "unkown error\n");
	}

   	snmp_close(ss);

	if (response)
    	snmp_free_pdu(response);

	return 0;
}
