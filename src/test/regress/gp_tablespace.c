#include "postgres.h"

#include "funcapi.h"
#include "tablefuncapi.h"
#include "miscadmin.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include <unistd.h>
#include <regex.h>

PG_FUNCTION_INFO_V1(gp_tablespace_temptablespaceOid);
Datum
gp_tablespace_temptablespaceOid(PG_FUNCTION_ARGS)
{
	PG_RETURN_OID(GetNextTempTableSpace());
}

PG_FUNCTION_INFO_V1(gp_tablespace_tmppath);
Datum
gp_tablespace_tmppath(PG_FUNCTION_ARGS)
{
	char tempdirpath[4096];
	Oid tblspcOid = PG_GETARG_OID(0);
	if (!OidIsValid(tblspcOid))
	{
		tblspcOid = GetNextTempTableSpace();
		if (!OidIsValid(tblspcOid))
			tblspcOid = MyDatabaseTableSpace ? MyDatabaseTableSpace : DEFAULTTABLESPACE_OID;
	}
	if (tblspcOid == DEFAULTTABLESPACE_OID ||
		tblspcOid == GLOBALTABLESPACE_OID)
	{
		snprintf(tempdirpath, sizeof(tempdirpath), "base/%s",
				PG_TEMP_FILES_DIR);
	}
	else
	{
		snprintf(tempdirpath, sizeof(tempdirpath), "pg_tblspc/%u/%s/%s",
				tblspcOid, GP_TABLESPACE_VERSION_DIRECTORY, PG_TEMP_FILES_DIR);
	}
	PG_RETURN_TEXT_P(CStringGetTextDatum(tempdirpath));
}
