//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Greenplum, Inc.
//
//	@filename:
//		funcs.cpp
//
//	@doc:
//		API for invoking optimizer using GPDB udfs
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include <sys/stat.h>

#include "gpopt/utils/CCatalogUtils.h"
#include "gpopt/utils/COptTasks.h"
#include "utils/guc.h"

#include "gpos/_api.h"
#include "gpopt/gpdbwrappers.h"

#include "gpos/version.h"
#include "gpopt/version.h"
#include "xercesc/util/XercesVersion.hpp"

//---------------------------------------------------------------------------
//	@function:
//		DisableXform
//
//	@doc:
//		Takes transformation name as input, and disables this transformation.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
DisableXform(PG_FUNCTION_ARGS)
{
	char *szXform = text_to_cstring(PG_GETARG_TEXT_P(0));
	bool fResult = COptTasks::FSetXform(szXform, true /*fDisable*/);

	StringInfoData str;
	initStringInfo(&str);

	if (fResult)
	{
		appendStringInfo(&str, "%s is disabled", szXform);
	}
	else
	{
		appendStringInfo(&str, "%s is not recognized", szXform);
	}
	text *result = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(result);
}
}

//---------------------------------------------------------------------------
//	@function:
//		EnableXform
//
//	@doc:
//		Takes transformation name as input, and enables this transformation.
//
//---------------------------------------------------------------------------

extern "C" {
Datum
EnableXform(PG_FUNCTION_ARGS)
{
	char *szXform = text_to_cstring(PG_GETARG_TEXT_P(0));
	bool fResult = COptTasks::FSetXform(szXform, false /*fDisable*/);

	StringInfoData str;
	initStringInfo(&str);

	if (fResult)
	{
		appendStringInfo(&str, "%s is enabled", szXform);
	}
	else
	{
		appendStringInfo(&str, "%s is not recognized", szXform);
	}
	text *result = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(result);
}
}


//---------------------------------------------------------------------------
//	@function:
//		LibraryVersion
//
//	@doc:
//		Returns the optimizer and gpos library versions as a message
//
//---------------------------------------------------------------------------
extern "C" {
Datum
LibraryVersion()
{
	StringInfoData str;
	initStringInfo(&str);
	appendStringInfo(&str, "GPOPT version: %d.%d", GPORCA_VERSION_MAJOR, GPORCA_VERSION_MINOR);
	appendStringInfo(&str, ", GPOS version: %d.%d", GPOS_VERSION_MAJOR, GPOS_VERSION_MINOR);
	appendStringInfo(&str, ", Xerces version: %s", XERCES_FULLVERSIONDOT);
	text *result = cstring_to_text(str.data);

	PG_RETURN_TEXT_P(result);
}
}

extern "C" {
StringInfo
OptVersion()
{
	StringInfo str = gpdb::SiMakeStringInfo();
	appendStringInfo(str, "%d.%d", GPORCA_VERSION_MAJOR, GPORCA_VERSION_MINOR);

	return str;
}
}
