
#include "postgres.h"
#include "miscadmin.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "commands/queue.h"
#include "executor/spi.h"
#include "postmaster/fts.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

/* tests for caql coverage in regproc.c */
extern Datum caql_bootstrap_regproc(PG_FUNCTION_ARGS);

/* tests for caql coverage in lsyscache.c */
extern Datum check_get_atttypmod(PG_FUNCTION_ARGS);
extern Datum check_get_opname(PG_FUNCTION_ARGS);
extern Datum check_get_typ_typrelid(PG_FUNCTION_ARGS);
extern Datum check_get_base_element_type(PG_FUNCTION_ARGS);

/* test for caql coverage in spi.c */
extern Datum check_SPI_gettype(PG_FUNCTION_ARGS);

/* tests for caql coverage in cdbutil.c */
extern Datum check_master_standby_dbid(PG_FUNCTION_ARGS);
extern Datum check_my_mirror_dbid(PG_FUNCTION_ARGS);
extern Datum check_dbid_get_dbinfo(PG_FUNCTION_ARGS);
extern Datum check_contentid_get_dbid(PG_FUNCTION_ARGS);

/* tests for caql coverage in fts.c */
extern Datum check_FtsFindSuperuser(PG_FUNCTION_ARGS);

/* tests for caql coverage in segadmin.c */
extern Datum check_gp_activate_standby(PG_FUNCTION_ARGS);

/* tests for caql coverage in queue.c */
extern Datum check_GetResqueueName(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(caql_bootstrap_regproc);
Datum
caql_bootstrap_regproc(PG_FUNCTION_ARGS)
{
	char	   *cstr_regprocin = "boolin";
	char	   *cstr_regoperin = "#>="; /* we should pick up a unique name */
	char	   *cstr_regclassin = "pg_class";
	char	   *cstr_regtypein = "bool";
	Datum		result;
	StringInfoData	buf;

	initStringInfo(&buf);

	SetProcessingMode(BootstrapProcessing);
	/* regproc */
	result = DirectFunctionCall1(regprocin, CStringGetDatum(cstr_regprocin));
	appendStringInfo(&buf, "regprocin(%s) = %d\n",
			cstr_regprocin, DatumGetObjectId(result));

	/* regoper */
	result = DirectFunctionCall1(regoperin, CStringGetDatum(cstr_regoperin));
	appendStringInfo(&buf, "regoperin(%s) = %d\n",
			cstr_regoperin, DatumGetObjectId(result));

	/* regclass */
	result = DirectFunctionCall1(regclassin, CStringGetDatum(cstr_regclassin));
	appendStringInfo(&buf, "regclassin(%s) = %d\n",
			cstr_regclassin, DatumGetObjectId(result));

	/* regtype */
	result = DirectFunctionCall1(regtypein, CStringGetDatum(cstr_regtypein));
	appendStringInfo(&buf, "regtypein(%s) = %d\n",
			cstr_regtypein, DatumGetObjectId(result));
	SetProcessingMode(NormalProcessing);

	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

/*
 * Testing caql coverage for lsyscache.c-
 */
PG_FUNCTION_INFO_V1(check_get_atttypmod);
Datum
check_get_atttypmod(PG_FUNCTION_ARGS)
{
	Oid relid = PG_GETARG_OID(0);
	AttrNumber attnum = PG_GETARG_INT16(1);
	int32 result;

	result = get_atttypmod(relid, attnum);

	PG_RETURN_INT32(result);
}

PG_FUNCTION_INFO_V1(check_get_opname);
Datum
check_get_opname(PG_FUNCTION_ARGS)
{
	Oid opno = PG_GETARG_OID(0);
	char *result;
	StringInfoData buf; 

	initStringInfo(&buf); 
	result = get_opname(opno);
	if (result)
		appendStringInfo(&buf, "%s", result);

	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

PG_FUNCTION_INFO_V1(check_get_typ_typrelid);
Datum
check_get_typ_typrelid(PG_FUNCTION_ARGS)
{
	Oid typid = PG_GETARG_OID(0);
	Oid result;

	result = get_typ_typrelid(typid);

	PG_RETURN_OID(result);
}

PG_FUNCTION_INFO_V1(check_get_base_element_type);
Datum
check_get_base_element_type(PG_FUNCTION_ARGS)
{
	Oid typid = PG_GETARG_OID(0);
	Oid result;

	result = get_base_element_type(typid);

	PG_RETURN_OID(result);
}

PG_FUNCTION_INFO_V1(check_SPI_gettype);
Datum
check_SPI_gettype(PG_FUNCTION_ARGS)
{
	int		fnumber = PG_GETARG_INT32(0);
	Relation	rel = relation_open(RelationRelationId, AccessShareLock);
	char		*name = SPI_gettype(RelationGetDescr(rel), fnumber);
	relation_close(rel, AccessShareLock);

	PG_RETURN_TEXT_P(cstring_to_text(name));
}

PG_FUNCTION_INFO_V1(check_master_standby_dbid);
Datum
check_master_standby_dbid(PG_FUNCTION_ARGS)
{
	int result;
	result = master_standby_dbid();

	PG_RETURN_OID(result);
}

PG_FUNCTION_INFO_V1(check_my_mirror_dbid);
Datum
check_my_mirror_dbid(PG_FUNCTION_ARGS)
{
	int result;
	result = my_mirror_dbid();

	PG_RETURN_OID(result);
}

PG_FUNCTION_INFO_V1(check_dbid_get_dbinfo);
Datum
check_dbid_get_dbinfo(PG_FUNCTION_ARGS)
{
	int16 result;
	int dbid = PG_GETARG_INT16(0);
	CdbComponentDatabaseInfo *i = NULL;

	i = dbid_get_dbinfo(dbid);

	result = i->dbid;

	PG_RETURN_INT16(result);
}

PG_FUNCTION_INFO_V1(check_contentid_get_dbid);
Datum
check_contentid_get_dbid(PG_FUNCTION_ARGS)
{
	int16 contentid = PG_GETARG_INT16(0);
	char role = PG_GETARG_CHAR(1);
	bool getPreferredRoleNotCurrentRole = PG_GETARG_BOOL(2);
	int16 result;

	result = contentid_get_dbid(contentid, role, getPreferredRoleNotCurrentRole);

	PG_RETURN_INT16(result);
}

PG_FUNCTION_INFO_V1(check_FtsFindSuperuser);
Datum
check_FtsFindSuperuser(PG_FUNCTION_ARGS)
{
	bool try_bootstrap = PG_GETARG_OID(0);
	char *result;
	StringInfoData buf; 

	initStringInfo(&buf); 
	result = FtsFindSuperuser(try_bootstrap);
	if (result)
		appendStringInfo(&buf, "%s", result);

	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

PG_FUNCTION_INFO_V1(check_gp_activate_standby);
Datum
check_gp_activate_standby(PG_FUNCTION_ARGS)
{
	/* Pretend as if I'm a standby.  The dbid is given as an argument. */
	GpIdentity.dbid = PG_GETARG_INT16(0);

	/* There is no DirectFunctionCall0 */
	DirectFunctionCall1(gp_activate_standby, Int32GetDatum(0));

	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(check_GetResqueueName);
Datum
check_GetResqueueName(PG_FUNCTION_ARGS)
{
	Oid		resqueueOid = PG_GETARG_OID(0);

	PG_RETURN_TEXT_P(cstring_to_text(GetResqueueName(resqueueOid)));
}
