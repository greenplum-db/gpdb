#include "pxfutils.h"
#include "utils/formatting.h"
#include "utils/syscache.h"

/*
 * Full name of the HEADER KEY expected by the PXF service
 * Converts input string to upper case and prepends "X-GP-" string
 *
 */
char *
normalize_key_name(const char *key)
{
	if (!key || strlen(key) == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.")));
	}

	StringInfoData formatter;

	initStringInfo(&formatter);
	char	   *upperCasedKey = str_toupper(pstrdup(key), strlen(key));

	appendStringInfo(&formatter, "X-GP-%s", upperCasedKey);
	pfree(upperCasedKey);

	return formatter.data;
}

/*
 * TypeOidGetTypename
 * Get the name of the type, given the OID
 */
char *
TypeOidGetTypename(Oid typid)
{

	Assert(OidIsValid(typid));

	HeapTuple	typtup;
	Form_pg_type typform;

	typtup = SearchSysCache(TYPEOID,
							ObjectIdGetDatum(typid),
							0, 0, 0);
	if (!HeapTupleIsValid(typtup))
		elog(ERROR, "cache lookup failed for type %u", typid);

	typform = (Form_pg_type) GETSTRUCT(typtup);

	char	   *typname = NameStr(typform->typname);

	StringInfoData tname;

	initStringInfo(&tname);
	appendStringInfo(&tname, "%s", typname);

	ReleaseSysCache(typtup);

	return tname.data;
}

/* Concatenate multiple literal strings using stringinfo */
char *
concat(int num_args,...)
{
	va_list		ap;
	StringInfoData str;

	initStringInfo(&str);

	va_start(ap, num_args);

	for (int i = 0; i < num_args; i++)
	{
		appendStringInfoString(&str, va_arg(ap, char *));
	}
	va_end(ap);

	return str.data;
}

/* Get authority (host:port) for the PXF server URL */
char *
get_authority(void)
{
	return psprintf("%s:%d", PxfDefaultHost, PxfDefaultPort);
}
