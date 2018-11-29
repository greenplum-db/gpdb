/* mock implementation for pxfprotocol.h */

#ifndef UNIT_TESTING
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pxfprotocol_export);
PG_FUNCTION_INFO_V1(pxfprotocol_import);
PG_FUNCTION_INFO_V1(pxfprotocol_validate_urls);

Datum pxfprotocol_export(PG_FUNCTION_ARGS);
Datum pxfprotocol_import(PG_FUNCTION_ARGS);
Datum pxfprotocol_validate_urls(PG_FUNCTION_ARGS);

static gphadoop_context* create_context(PG_FUNCTION_ARGS, bool is_import);
static void cleanup_context(gphadoop_context* context);
static void check_caller(PG_FUNCTION_ARGS, const char* func_name);

Datum
pxfprotocol_validate_urls(FunctionCallInfo fcinfo)
{
	check_expected(fcinfo);
	return (Datum) mock();
}

Datum
pxfprotocol_export(FunctionCallInfo fcinfo)
{
	check_expected(fcinfo);
	return (Datum) mock();
}

Datum
pxfprotocol_import(FunctionCallInfo fcinfo)
{
	check_expected(fcinfo);
	return (Datum) mock();
}

static gphadoop_context*
create_context(FunctionCallInfo fcinfo, bool is_import)
{
	return (gphadoop_context*) mock();
}

static void
cleanup_context(gphadoop_context* context)
{
	mock();
}

static void
check_caller(FunctionCallInfo fcinfo, const char* func_name)
{
	mock();
}
