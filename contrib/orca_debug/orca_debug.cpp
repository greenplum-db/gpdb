//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Greenplum, Inc.
//
//	@doc:
//		API for invoking optimizer using GPDB udfs
//---------------------------------------------------------------------------

#include "postgres.h"

#include <sys/stat.h>

#include "gpopt/utils/CCatalogUtils.h"
#include "gpopt/utils/COptTasks.h"
#include "gpopt/mdcache/CMDCache.h"
#include "utils/guc.h"
#include "utils/snapmgr.h"

#include "gpos/_api.h"
#include "gpos/io/CFileReader.h"
#include "gpos/io/CFileWriter.h"
#include "gpopt/gpdbwrappers.h"

extern "C" {

PG_MODULE_MAGIC;

#undef PG_DETOAST_DATUM
#define PG_DETOAST_DATUM(datum) \
	gpdb::PvlenDetoastDatum((struct varlena *) gpdb::PvPointerFromDatum(datum))

#undef PG_RETURN_POINTER
#define PG_RETURN_POINTER(x) return gpdb::DDatumFromPointer(x)

#undef PG_RETURN_UINT32
#define PG_RETURN_UINT32(x)  return gpdb::DDatumFromUint32(x)

#undef PG_RETURN_INT32
#define PG_RETURN_INT32(x)  return gpdb::DDatumFromInt32(x)

// by value

Datum DumpPlan(PG_FUNCTION_ARGS);
Datum RestorePlan(PG_FUNCTION_ARGS);
Datum DumpPlanToFile(PG_FUNCTION_ARGS);
Datum RestorePlanFromFile(PG_FUNCTION_ARGS);
Datum RestorePlanDXL(PG_FUNCTION_ARGS);
Datum RestorePlanFromDXLFile(PG_FUNCTION_ARGS);
Datum DumpMDObjDXL(PG_FUNCTION_ARGS);
Datum DumpCatalogDXL(PG_FUNCTION_ARGS);
Datum DumpRelStatsDXL(PG_FUNCTION_ARGS);
Datum DumpMDCastDXL(PG_FUNCTION_ARGS);
Datum DumpMDScCmpDXL(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(DumpPlan);
PG_FUNCTION_INFO_V1(RestorePlan);
PG_FUNCTION_INFO_V1(DumpPlanToFile);
PG_FUNCTION_INFO_V1(RestorePlanFromFile);
PG_FUNCTION_INFO_V1(RestorePlanDXL);
PG_FUNCTION_INFO_V1(RestorePlanFromDXLFile);
PG_FUNCTION_INFO_V1(DumpMDObjDXL);
PG_FUNCTION_INFO_V1(DumpCatalogDXL);
PG_FUNCTION_INFO_V1(DumpRelStatsDXL);
PG_FUNCTION_INFO_V1(DumpMDCastDXL);
PG_FUNCTION_INFO_V1(DumpMDScCmpDXL);

Datum DumpQuery(PG_FUNCTION_ARGS);
Datum RestoreQuery(PG_FUNCTION_ARGS);
Datum DumpQueryToFile(PG_FUNCTION_ARGS);
Datum RestoreQueryFromFile(PG_FUNCTION_ARGS);
Datum DumpQueryDXL(PG_FUNCTION_ARGS);
Datum DumpQueryToDXLFile(PG_FUNCTION_ARGS);
Datum DumpQueryFromFileToDXLFile(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(DumpQuery);
PG_FUNCTION_INFO_V1(RestoreQuery);
PG_FUNCTION_INFO_V1(DumpQueryToFile);
PG_FUNCTION_INFO_V1(RestoreQueryFromFile);
PG_FUNCTION_INFO_V1(DumpQueryDXL);
PG_FUNCTION_INFO_V1(DumpQueryToDXLFile);
PG_FUNCTION_INFO_V1(DumpQueryFromFileToDXLFile);

Datum Optimize(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(Optimize);

Datum EvalExprFromDXLFile(PG_FUNCTION_ARGS);
Datum OptimizeMinidumpFromFile(PG_FUNCTION_ARGS);
Datum ExecuteMinidumpFromFile(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(EvalExprFromDXLFile);
PG_FUNCTION_INFO_V1(OptimizeMinidumpFromFile);
PG_FUNCTION_INFO_V1(ExecuteMinidumpFromFile);
} // end extern C


static PlannedStmt *planQuery(char *szSqlText);
static Query *parseSQL(char *szSqlText);
static char *getQueryBinary(char *szSqlText, size_t *piLength);
static char *getPlannedStmtBinary(char *szSqlText, size_t *piLength);
static int extractFrozenPlanAndExecute(char *pcSerializedPS);
static int extractFrozenQueryPlanAndExecute(char *pcQuery);
static int executeXMLPlan(char *szXml);
static int translateQueryToFile(char *szSqlText, char *szFilename);

//---------------------------------------------------------------------------
//	@function:
//		parseSQL
//
//	@doc:
//		Parse a query given as SQL text.
// 		Input:
// 			szSqlText - SQL query
// 		Output:
// 			query object
//
//---------------------------------------------------------------------------

static Query *parseSQL(char *szSqlText)
{
	Assert(szSqlText);
	Assert(szSqlText);

	List *plQueryTree = pg_parse_and_rewrite(szSqlText, NULL, 0);

	if (1 != gpdb::UlListLength(plQueryTree))
	{
		elog(ERROR, "problem parsing query %s", szSqlText);
	}

	Query *pquery = (Query *) lfirst(gpdb::PlcListHead(plQueryTree));

	return pquery;
}

//---------------------------------------------------------------------------
//	@function:
//		PlannedStmt
//
//	@doc:
//		Plan a query given as SQL text.
// 		Input:
// 			szSqlText - SQL query
// 		Output:
// 			planned stmt
//
//---------------------------------------------------------------------------

static PlannedStmt *planQuery(char *szSqlText)
{
	Assert(szSqlText);
	Assert(szSqlText);

	List *plQueryTree = pg_parse_and_rewrite(szSqlText, NULL, 0);

	if (1 != gpdb::UlListLength(plQueryTree))
	{
		elog(ERROR, "problem parsing query %s", szSqlText);
	}

	Query *pqueryTree = (Query *) lfirst(gpdb::PlcListHead(plQueryTree));

	PlannedStmt *pplstmt = pg_plan_query(pqueryTree, NULL);

	if (!pplstmt)
	{
		elog(ERROR, "problem planned query %s. query tree is %s", szSqlText, gpdb::SzNodeToString(pqueryTree));
	}

	return pplstmt;
}

//---------------------------------------------------------------------------
//	@function:
//		getQueryBinary
//
//	@doc:
//		This method takes a SQL text, calls the parser and gets a GPDB query object.
// 		Lastly serializes the plan with auxillary structures.
// 		Inputs:
// 			szSqlText - SQL text
// 		Output:
// 			piLength - length of serialized GPDB query object
// 		Return:
// 			serialized GPDB query buffer.
//
//---------------------------------------------------------------------------

static char *getQueryBinary
	(
	char *szSqlText,
	size_t *piLength
	)
{
	Query *pquery = parseSQL(szSqlText);

	int iQueryStringLen = -1;
	char *pcQuery = nodeToBinaryStringFast((Node *) pquery, &iQueryStringLen);

	Assert(pcQuery);

	// Write all the serialized structures into a single bytea
	size_t iSerializedStructure = sizeof(ULONG);
	iSerializedStructure += iQueryStringLen;

	*piLength = iSerializedStructure;

	char *pcBuf = (char *) gpdb::GPDBAlloc(iSerializedStructure);
	char *pcRetBuf = pcBuf;

	memcpy(pcBuf, &iQueryStringLen, sizeof(ULONG));
	pcBuf+=sizeof(ULONG);

	memcpy(pcBuf, pcQuery, iQueryStringLen);
	pcBuf+=iQueryStringLen;

	return pcRetBuf;
}

//---------------------------------------------------------------------------
//	@function:
//		getPlannedStmtBinary
//
//	@doc:
//		This method takes a SQL text, calls the planner and creates a plan
//		and then serializes the plan with auxillary structures.
// 		Inputs:
// 			szSqlText - SQL text
// 		Output:
// 			piLength - length of serialized plan
// 		Return:
// 			serialized plan buffer.
//
//---------------------------------------------------------------------------

static char *getPlannedStmtBinary
	(
	char *szSqlText,
	size_t *piLength
	)
{

	PlannedStmt *pplstmt = planQuery(szSqlText);

	int iPlannedStmtStringLen = -1;
	char *szPlannedStmtString = nodeToBinaryStringFast((Node *) pplstmt, &iPlannedStmtStringLen);

	Assert(szPlannedStmtString);

	// Write all the serialized structures into a single bytea
	size_t iSerializedStructure = sizeof(ULONG);
	iSerializedStructure += iPlannedStmtStringLen;

	*piLength = iSerializedStructure;

	char *pcBuf = (char *) gpdb::GPDBAlloc(iSerializedStructure);
	char *pcRetBuf = pcBuf;

	memcpy(pcBuf, &iPlannedStmtStringLen, sizeof(ULONG));
	pcBuf+=sizeof(ULONG);

	memcpy(pcBuf, szPlannedStmtString, iPlannedStmtStringLen);
	pcBuf+=iPlannedStmtStringLen;

	return pcRetBuf;
}

static int translateQueryToFile
	(
	char *szSqlText,
	char *szFilename
	)
{
	Query *pquery = parseSQL(szSqlText);

	Assert(pquery);

	char *szXmlString = COptTasks::SzDXL(pquery);
	int iLen = (int) gpos::clib::UlStrLen(szXmlString);

	CFileWriter fw;
	fw.Open(szFilename, S_IRUSR | S_IWUSR);
	fw.Write(reinterpret_cast<const BYTE*>(szXmlString), iLen + 1);
	fw.Close();

	return iLen;
}


//---------------------------------------------------------------------------
//	@function:
//		DumpQuery
//
//	@doc:
//		Parse a query and dump out query object as a bytea.
// 		Input: sql query text
// 		Output: serialized version of query as bytea
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpQuery(PG_FUNCTION_ARGS)
{
	char *szSqlText = text_to_cstring(PG_GETARG_TEXT_P(0));

	Query *pquery = parseSQL(szSqlText);
	elog(NOTICE, "(DumpQuery - Original) \n %s", pretty_format_node_dump(const_cast<char*>(gpdb::SzNodeToString(pquery))));

	Query *pqueryNormalized = preprocess_query_optimizer(pquery, NULL);
	elog(NOTICE, "(DumpQuery - Normalized) \n %s", pretty_format_node_dump(const_cast<char*>(gpdb::SzNodeToString(pqueryNormalized))));

	text *ptResult = cstring_to_text("Query dumped");

	PG_RETURN_TEXT_P(ptResult);
}
}

//---------------------------------------------------------------------------
//	@function:
//		RestoreQuery
//
//	@doc:
//		Restores a query, along with meta-data from a bytea, plans it, and executes it.
// 		Input: bytea corresponding to serialized plan
// 		Output: number of rows corresponding to execution of plan.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
RestoreQuery(PG_FUNCTION_ARGS)
{
	bytea   *pbyteaData = PG_GETARG_BYTEA_P(0);

	char   *pcSerializedData = VARDATA(pbyteaData);

	int iProcessed = extractFrozenQueryPlanAndExecute(pcSerializedData);

	elog(NOTICE, "(RestorePlan) PROCESSED %d", iProcessed);
	StringInfoData str;
	initStringInfo(&str);

	appendStringInfo(&str, "Query processed %d rows", iProcessed);
	text *ptResult = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(ptResult);
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpQueryToFile
//
//	@doc:
//		Parse SQL query, extract meta-data and dump it to a file.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpQueryToFile(PG_FUNCTION_ARGS)
{
	char *szSql = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(1));

	size_t iQueryStringLen = -1;
	char *pcQuery = getQueryBinary(szSql, &iQueryStringLen);

	CFileWriter fw;
	fw.Open(szFilename, S_IRUSR | S_IWUSR);
	fw.Write(reinterpret_cast<const BYTE*>(&iQueryStringLen), sizeof(iQueryStringLen));
	fw.Write(reinterpret_cast<const BYTE*>(pcQuery), iQueryStringLen);
	fw.Close();

	PG_RETURN_UINT32( (ULONG) iQueryStringLen);
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpQueryDXL
//
//	@doc:
//		Parse a query and dump out query as xml text.
// 		Input: sql query text
// 		Output: plan in dxl
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpQueryDXL(PG_FUNCTION_ARGS)
{
	char *szSqlText = text_to_cstring(PG_GETARG_TEXT_P(0));

	Query *pquery = parseSQL(szSqlText);

	Assert(pquery);

	char *szXmlString = COptTasks::SzDXL(pquery);
	if (NULL == szXmlString)
	{
		elog(ERROR, "Error translating query to DXL");
	}

	PG_RETURN_TEXT_P(cstring_to_text(szXmlString));
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpQueryToDXLFile
//
//	@doc:
//		Parse a query and dump out query as xml text.
// 		Input: sql query text
// 		Output: serialized dxl representation written to file.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpQueryToDXLFile(PG_FUNCTION_ARGS)
{
	char *szSqlText = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(1));

	int iLen = translateQueryToFile(szSqlText, szFilename);

	PG_RETURN_INT32(iLen);
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpQueryFromFileToDXLFile
//
//	@doc:
//		Parse a query and dump out query as xml text.
// 		Input: name of the file containing query
// 		Output: serialized dxl representation written to file.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpQueryFromFileToDXLFile(PG_FUNCTION_ARGS)
{
	char *szSqlFilename = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(1));

	CFileReader fr;
	fr.Open(szSqlFilename);
	ULLONG ullSize = fr.UllSize();

	char *pcBuf = (char*) gpdb::GPDBAlloc(ullSize + 1);
	fr.UlpRead((BYTE*)pcBuf, ullSize);
	pcBuf[ullSize] = '\0';
	fr.Close();

	int iLen = translateQueryToFile(pcBuf, szFilename);

	gpdb::GPDBFree(pcBuf);

	PG_RETURN_INT32(iLen);
}
}

//---------------------------------------------------------------------------
//	@function:
//		EvalExprFromDXLFile
//
//	@doc:
//		Reads a serialized XML representation of a DXL constant scalar expression
//		from the file path given by its argument. The expression is then evaluated
//		by the executor and the result is transformed back to DXL and serialized
//		as a string.
//		It doesn't check that the expression is actually constant.
//
//---------------------------------------------------------------------------
extern "C" {
Datum
EvalExprFromDXLFile(PG_FUNCTION_ARGS)
{
	char *szFileName = text_to_cstring(PG_GETARG_TEXT_P(0));
	CFileReader fr;
	fr.Open(szFileName);
	ULLONG ullSize = fr.UllSize();
	char *pcBuf = (char*) gpdb::GPDBAlloc(ullSize + 1);
	fr.UlpRead((BYTE*)pcBuf, ullSize);
	fr.Close();
	pcBuf[ullSize] = '\0';

	char *szResultDXL = COptTasks::SzEvalExprFromXML(pcBuf);
	gpdb::GPDBFree(pcBuf);

	if (NULL != szResultDXL)
	{
		text *ptResult = cstring_to_text(szResultDXL);
		gpdb::GPDBFree(szResultDXL);
		PG_RETURN_TEXT_P(ptResult);
	}
	else
	{
		// Return a dummy value so the tests can continue
		PG_RETURN_NULL();
	}
}
}


//---------------------------------------------------------------------------
//	@function:
//		OptimizeMinidumpFromFile
//
//	@doc:
//		Loads a minidump from the given file path, optimizes it and returns
//		the string serialized representation of the result as DXL
//
//---------------------------------------------------------------------------

extern "C" {
Datum
OptimizeMinidumpFromFile(PG_FUNCTION_ARGS)
{
	char *szFileName = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *szResultDXL = COptTasks::SzOptimizeMinidumpFromFile(szFileName);
	if (NULL != szResultDXL)
	{
		text *ptResult = cstring_to_text(szResultDXL);
		gpdb::GPDBFree(szResultDXL);
		PG_RETURN_TEXT_P(ptResult);
	}
	else
	{
		elog(NOTICE, "Execution of UDF 'OptimizeMinidumpFromFile' failed. Consult the LOG for more information.");

		// return a dummy value
		PG_RETURN_NULL();
	}
}
}


//---------------------------------------------------------------------------
//	@function:
//		ExecuteMinidumpFromFile
//
//	@doc:
//		Loads a minidump from the given file path, executes it and returns
//		the number of rows in the result.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
ExecuteMinidumpFromFile(PG_FUNCTION_ARGS)
{
	char *szFileName = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *szResultDXL = COptTasks::SzOptimizeMinidumpFromFile(szFileName);
	if (NULL == szResultDXL)
	{
		elog(NOTICE, "Execution of UDF 'ExecuteMinidumpFromFile' failed. Consult the LOG for more information.");

		// return a dummy value
		PG_RETURN_NULL();
	}
	int iProcessed = executeXMLPlan(szResultDXL);
	gpdb::GPDBFree(szResultDXL);
	StringInfoData str;
	initStringInfo(&str);
	appendStringInfo(&str, "processed %d rows", iProcessed);
	text *ptResult = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(ptResult);
}
}


//---------------------------------------------------------------------------
//	@function:
//		RestorePlanDXL
//
//	@doc:
//		Take an xml representation of a plan and execute it. Restores a plan,
//		along with meta-data from a bytea, executes it and returns number of rows.
// 		Input: bytea corresponding to serialized plan
// 		Output: number of rows corresponding to execution of plan.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
RestorePlanDXL(PG_FUNCTION_ARGS)
{
	char *szXmlString = text_to_cstring(PG_GETARG_TEXT_P(0));

	int iProcessed = executeXMLPlan(szXmlString);

	StringInfoData str;
	initStringInfo(&str);
	appendStringInfo(&str, "processed %d rows", iProcessed);

	text *ptResult = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(ptResult);
}
}

//---------------------------------------------------------------------------
//	@function:
//		RestorePlanFromDXLFile
//
//	@doc:
//		Restores a plan specified in DXL format in an XML file, executes it
//		and returns number of rows.
// 		Input: bytea corresponding to XML file name
// 		Output: number of rows corresponding to execution of plan.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
RestorePlanFromDXLFile(PG_FUNCTION_ARGS)
{
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(0));

	CFileReader fr;
	fr.Open(szFilename);
	ULLONG ullSize = fr.UllSize();

	char *pcBuf = (char*) gpdb::GPDBAlloc(ullSize + 1);
	fr.UlpRead((BYTE*)pcBuf, ullSize);
	pcBuf[ullSize] = '\0';

	fr.Close();

	int	iProcessed = executeXMLPlan(pcBuf);

	elog(NOTICE, "Processed %d rows.", iProcessed);
	gpdb::GPDBFree(pcBuf);

	StringInfoData str;
	initStringInfo(&str);

	appendStringInfo(&str, "Query processed %d rows", iProcessed);
	text *ptResult = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(ptResult);
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpMDObjDXL
//
//	@doc:
//		Dump relcache info about a catalog object
// 		Input: type oid
// 		Output: cache type object into dxl
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpMDObjDXL(PG_FUNCTION_ARGS)
{
	Oid oid = gpdb::OidFromDatum(PG_GETARG_DATUM(0));

	char *szDXL = COptTasks::SzMDObjs(ListMake1Oid(oid));

	if (NULL == szDXL)
	{
		elog(ERROR, "Error dumping MD object");
	}

	PG_RETURN_TEXT_P(cstring_to_text(szDXL));
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpRelStatsDXL
//
//	@doc:
//		Dump statistics info about a relation
// 		Input: relation oid
// 		Output: relstats object in DXL form
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpRelStatsDXL(PG_FUNCTION_ARGS)
{
	Oid oid = gpdb::OidFromDatum(PG_GETARG_DATUM(0));

	char *szDXL = COptTasks::SzRelStats(ListMake1Oid(oid));

	PG_RETURN_TEXT_P(cstring_to_text(szDXL));
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpMDCastDXL
//
//	@doc:
//		Dump cast function between two types
// 		Input: source and destination type oids
// 		Output: scalar cast function in DXL format
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpMDCastDXL(PG_FUNCTION_ARGS)
{
	Oid oidSrc = gpdb::OidFromDatum(PG_GETARG_DATUM(0));
	Oid oidDest = gpdb::OidFromDatum(PG_GETARG_DATUM(1));

	char *szDXL = COptTasks::SzMDCast(ListMake2Oid(oidSrc, oidDest));

	PG_RETURN_TEXT_P(cstring_to_text(szDXL));
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpMDScCmpDXL
//
//	@doc:
//		Dump scalar comparison operator between two types
// 		Input: left and right type oids and comparison type
// 		Output: scalar comparison object in DXL format
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpMDScCmpDXL(PG_FUNCTION_ARGS)
{
	Oid oidLeft = gpdb::OidFromDatum(PG_GETARG_DATUM(0));
	Oid oidRight = gpdb::OidFromDatum(PG_GETARG_DATUM(1));
	char *szCmpType = text_to_cstring(PG_GETARG_TEXT_P(2));
	
	char *szDXL = COptTasks::SzMDScCmp(ListMake2Oid(oidLeft, oidRight), szCmpType);

	PG_RETURN_TEXT_P(cstring_to_text(szDXL));
}
}

//---------------------------------------------------------------------------
//	@function:
//		DumpCatalogDXL
//
//	@doc:
//		Dump entire catalog into a DXL file (entire means the objects
//		supported by MD cache). Returns number of bytes written.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpCatalogDXL(PG_FUNCTION_ARGS)
{
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(0));
	List *plAllOids = CCatalogUtils::PlAllOids();

	COptTasks::DumpMDObjs(plAllOids, szFilename);

	PG_RETURN_INT32(0);
}
}

//---------------------------------------------------------------------------
//	@function:
//		extractFrozenQueryPlanAndExecute
//
//	@doc:
//		This method takes a frozen query, thaws it, plans it and executes the
//		generated plan.
//
//---------------------------------------------------------------------------

static int extractFrozenQueryPlanAndExecute(char *pcQuery)
{
	Assert(pcQuery);

	ULONG ulQueryLen = -1;
	memcpy(&ulQueryLen, pcQuery, sizeof(ULONG));
	pcQuery+=sizeof(ULONG);

	Query *pquery = (Query *) readNodeFromBinaryString(pcQuery, ulQueryLen);

	pcQuery+=ulQueryLen;

	PlannedStmt *pplstmt = pg_plan_query(pquery, NULL);

	if (!pplstmt)
	{
		elog(ERROR, "Problem with planned statement of query tree %s", gpdb::SzNodeToString(pquery));
	}

	// The following steps are required to be able to execute the query.

	DestReceiver *pdest = CreateDestReceiver(DestNone);
	QueryDesc    *pqueryDesc = CreateQueryDesc(pplstmt, PStrDup("Internal Query") /*plan->query */,
			GetActiveSnapshot(),
			InvalidSnapshot,
			pdest,
			NULL /*paramLI*/,
			false);

	// Do not record gpperfmon information about internal queries
	pqueryDesc->gpmon_pkt = NULL;

	elog(NOTICE, "Executing thawed plan...");

	ExecutorStart(pqueryDesc, 0);

	ExecutorRun(pqueryDesc, ForwardScanDirection, 0);

	ExecutorEnd(pqueryDesc);

	int iProcessed = (int) pqueryDesc->es_processed;

	FreeQueryDesc(pqueryDesc);

	return iProcessed;
}

//---------------------------------------------------------------------------
//	@function:
//		extractFrozenPlanAndExecute
//
//	@doc:
//		This method takes a frozen plan, thaws it and executes the plan.
//
//---------------------------------------------------------------------------

static int extractFrozenPlanAndExecute(char *pcSerializedPS)
{
	Assert(pcSerializedPS);

	ULONG ulPlannedStmtLen = -1;
	memcpy(&ulPlannedStmtLen, pcSerializedPS, sizeof(ULONG));
	pcSerializedPS+=sizeof(ULONG);

	PlannedStmt *pplstmt = (PlannedStmt *) readNodeFromBinaryString(pcSerializedPS, ulPlannedStmtLen);

	pcSerializedPS+=ulPlannedStmtLen;

	//The following steps are required to be able to execute the query.

	DestReceiver *pdest = CreateDestReceiver(DestNone);
	QueryDesc    *pqueryDesc = CreateQueryDesc(pplstmt, PStrDup("Internal Query") /*plan->query */,
			GetActiveSnapshot(),
			InvalidSnapshot,
			pdest,
			NULL /*paramLI*/,
			false);

	// Do not record gpperfmon information about internal queries
	pqueryDesc->gpmon_pkt = NULL;

	elog(NOTICE, "Executing thawed plan...");

	ExecutorStart(pqueryDesc, 0);

	ExecutorRun(pqueryDesc, ForwardScanDirection, 0);

	ExecutorEnd(pqueryDesc);

	int iProcessed = (int) pqueryDesc->es_processed;

	FreeQueryDesc(pqueryDesc);

	return iProcessed;
}

static int executeXMLPlan(char *szXml)
{
	PlannedStmt *pplstmt = COptTasks::PplstmtFromXML(szXml);

	// The following steps are required to be able to execute the query.

	DestReceiver *pdest = CreateDestReceiver(DestNone);
	QueryDesc    *pqueryDesc = CreateQueryDesc(pplstmt, PStrDup("Internal Query") /*plan->query */,
			GetActiveSnapshot(),
			InvalidSnapshot,
			pdest,
			NULL /*paramLI*/,
			false);

	// Do not record gpperfmon information about internal queries
	pqueryDesc->gpmon_pkt = NULL;

	elog(NOTICE, "Executing thawed plan...");

	ExecutorStart(pqueryDesc, 0);

	ExecutorRun(pqueryDesc, ForwardScanDirection, 0);

	ExecutorEnd(pqueryDesc);

	int iProcessed = (int) pqueryDesc->es_processed;

	FreeQueryDesc(pqueryDesc);

	return iProcessed;
}

//---------------------------------------------------------------------------
//	@function:
//		RestoreQueryFromFile
//
//	@doc:
//		Restores a query, along with meta-data from a bytea, plans the query
//		executes it and returns number of rows.
// 		Input: bytea corresponding to serialized query along with auxillary information
// 		Output: number of rows corresponding to execution of plan.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
RestoreQueryFromFile(PG_FUNCTION_ARGS)
{
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(0));

	CFileReader fr;
	fr.Open(szFilename);
	ULLONG ullSize = fr.UllSize();
	elog(NOTICE, "(RestoreFromFile) Filesize is " UINT64_FORMAT, (uint64) ullSize);

	char *pcBuf = (char*) gpdb::GPDBAlloc(ullSize);
	fr.UlpRead((BYTE*)pcBuf, ullSize);
	fr.Close();

	int iBinaryLen;
	memcpy(&iBinaryLen, pcBuf, sizeof(int));
	Assert(iBinaryLen == ullSize - sizeof(int));

	elog(NOTICE, "(RestoreFromFile) BinaryLen is %d", iBinaryLen);
	char *pcBinary = pcBuf + sizeof(int);

	int iProcessed = extractFrozenQueryPlanAndExecute(pcBinary);

	elog(NOTICE, "(RestorePlan) PROCESSED %d", iProcessed);
	StringInfoData str;
	initStringInfo(&str);

	appendStringInfo(&str, "Query processed %d rows", iProcessed);
	text *ptResult = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(ptResult);
}
}


//---------------------------------------------------------------------------
//	@function:
//		DumpPlanToFile
//
//	@doc:
//		Plan SQL query, extract meta-data and dump it to a file.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DumpPlanToFile(PG_FUNCTION_ARGS)
{
	char *szSql = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(1));

	size_t iBinaryLen = -1;
	char *pcBinary = getPlannedStmtBinary(szSql, &iBinaryLen);

	CFileWriter fw;
	fw.Open(szFilename, S_IRUSR | S_IWUSR);
	fw.Write(reinterpret_cast<const BYTE*>(&iBinaryLen), sizeof(iBinaryLen));
	fw.Write(reinterpret_cast<const BYTE*>(pcBinary), iBinaryLen);
	fw.Close();

	PG_RETURN_UINT32((ULONG) iBinaryLen);
}
}

//---------------------------------------------------------------------------
//	@function:
//		RestorePlanFromFile
//
//	@doc:
//		Restores a plan along with meta data from a file, executes it and
//		returns number of rows.
// 		Input: bytea corresponding to serialized plan
// 		Output: number of rows corresponding to execution of plan.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
RestorePlanFromFile(PG_FUNCTION_ARGS)
{
	char *szFilename = text_to_cstring(PG_GETARG_TEXT_P(0));

	CFileReader fr;
	fr.Open(szFilename);
	ULLONG ullSize = fr.UllSize();

	char *pcBuf = (char*) gpdb::GPDBAlloc(ullSize);
	fr.UlpRead((BYTE*)pcBuf, ullSize);
	fr.Close();

	int iBinaryLen;
	memcpy(&iBinaryLen, pcBuf, sizeof(int));
	Assert(iBinaryLen == ullSize - sizeof(int));

	char *pcBinary = pcBuf + sizeof(int);

	int	iProcessed = extractFrozenPlanAndExecute(pcBinary);

	elog(NOTICE, "Processed %d rows.", iProcessed);
	gpdb::GPDBFree(pcBuf);

	StringInfoData str;
	initStringInfo(&str);

	appendStringInfo(&str, "Query processed %d rows", iProcessed);
	text *ptResult = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(ptResult);
}
}

//---------------------------------------------------------------------------
//	@function:
//		Optimize
//
//	@doc:
//		Optimize the query using the new optimizer
// 		Input: sql query text
// 		Output: number of rows corresponding to execution
// 			of plan generated by the new optimizer.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
Optimize(PG_FUNCTION_ARGS)
{
	char *szSQLText = text_to_cstring(PG_GETARG_TEXT_P(0));

	Query *pquery = parseSQL(szSQLText);
	Query *pqueryNormalized = preprocess_query_optimizer(pquery, NULL);

	Assert(pqueryNormalized);

	char *szOutput = COptTasks::SzOptimize(pqueryNormalized);

	if (NULL == szOutput)
	{
		elog(ERROR, "Error optimizing query");
	}

	PG_RETURN_TEXT_P(cstring_to_text(szOutput));
}
}
