/*
 *	pg_upgrade_support.c
 *
 *	server-side functions to set backend global variables
 *	to control oid and relfilenode assignment, and do other special
 *	hacks needed for pg_upgrade.
 *
 *	Copyright (c) 2010-2011, PostgreSQL Global Development Group
 *	contrib/pg_upgrade_support/pg_upgrade_support.c
 */

#include "postgres.h"

#include "fmgr.h"
#include "catalog/namespace.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbvars.h"
#include "commands/extension.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"

/* THIS IS USED ONLY FOR PG >= 9.0 */

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern PGDLLIMPORT Oid binary_upgrade_next_toast_pg_type_oid;

extern PGDLLIMPORT Oid binary_upgrade_next_toast_pg_class_oid;

#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

Datum		set_next_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_array_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_toast_pg_type_oid(PG_FUNCTION_ARGS);

Datum		set_next_heap_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_index_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_toast_pg_class_oid(PG_FUNCTION_ARGS);

Datum		set_next_pg_enum_oid(PG_FUNCTION_ARGS);
Datum		set_next_pg_authid_oid(PG_FUNCTION_ARGS);

Datum		create_empty_extension(PG_FUNCTION_ARGS);

Datum		set_next_pg_namespace_oid(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(set_next_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_array_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_toast_pg_type_oid);

PG_FUNCTION_INFO_V1(set_next_heap_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_index_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_toast_pg_class_oid);

PG_FUNCTION_INFO_V1(set_next_pg_enum_oid);
PG_FUNCTION_INFO_V1(set_next_pg_authid_oid);

PG_FUNCTION_INFO_V1(create_empty_extension);

PG_FUNCTION_INFO_V1(set_next_pg_namespace_oid);

Datum
set_next_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);
	Oid			typnamespaceoid = PG_GETARG_OID(1);
	char	   *typname = GET_STR(PG_GETARG_TEXT_P(2));

	AddPreassignedOidFromBinaryUpgrade(typoid, TypeRelationId, typname,
						typnamespaceoid, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_array_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);
	Oid			typnamespaceoid = PG_GETARG_OID(1);
	char	   *typname = GET_STR(PG_GETARG_TEXT_P(2));

	AddPreassignedOidFromBinaryUpgrade(typoid, TypeRelationId, typname,
						typnamespaceoid, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_toast_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);
	Oid			typnamespaceoid = PG_GETARG_OID(1);
	char	   *typname = GET_STR(PG_GETARG_TEXT_P(2));

	binary_upgrade_next_toast_pg_type_oid++;

	AddPreassignedOidFromBinaryUpgrade(typoid, TypeRelationId, typname,
						typnamespaceoid, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_heap_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);
	Oid			relnamespace = PG_GETARG_OID(1);
	char	   *relname = GET_STR(PG_GETARG_TEXT_P(2));

	AddPreassignedOidFromBinaryUpgrade(reloid, RelationRelationId, relname,
									   relnamespace, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_toast_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);
	Oid			relnamespace = PG_GETARG_OID(1);
	char	   *relname = GET_STR(PG_GETARG_TEXT_P(2));

	binary_upgrade_next_toast_pg_class_oid++;

	AddPreassignedOidFromBinaryUpgrade(reloid, RelationRelationId, relname,
									   relnamespace, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_index_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);
	Oid			relnamespace = PG_GETARG_OID(1);
	char	   *relname = GET_STR(PG_GETARG_TEXT_P(2));

	AddPreassignedOidFromBinaryUpgrade(reloid, RelationRelationId, relname,
									   relnamespace, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_pg_enum_oid(PG_FUNCTION_ARGS)
{
	Oid			enumoid = PG_GETARG_OID(0);
	Oid			typeoid = PG_GETARG_OID(1);
	char	   *enumlabel = GET_STR(PG_GETARG_TEXT_P(2));

	AddPreassignedOidFromBinaryUpgrade(enumoid, EnumRelationId, enumlabel,
									   InvalidOid, typeoid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
set_next_pg_authid_oid(PG_FUNCTION_ARGS)
{
	Oid			roleid = PG_GETARG_OID(0);
	char	   *rolename = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(roleid, AuthIdRelationId, rolename,
										   InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
create_empty_extension(PG_FUNCTION_ARGS)
{
	text	   *extName = PG_GETARG_TEXT_PP(0);
	text	   *schemaName = PG_GETARG_TEXT_PP(1);
	bool		relocatable = PG_GETARG_BOOL(2);
	text	   *extVersion = PG_GETARG_TEXT_PP(3);
	Datum		extConfig;
	Datum		extCondition;
	List	   *requiredExtensions;

	if (PG_ARGISNULL(4))
		extConfig = PointerGetDatum(NULL);
	else
		extConfig = PG_GETARG_DATUM(4);

	if (PG_ARGISNULL(5))
		extCondition = PointerGetDatum(NULL);
	else
		extCondition = PG_GETARG_DATUM(5);

	requiredExtensions = NIL;
	if (!PG_ARGISNULL(6))
	{
		ArrayType  *textArray = PG_GETARG_ARRAYTYPE_P(6);
		Datum	   *textDatums;
		int			ndatums;
		int			i;

		deconstruct_array(textArray,
						  TEXTOID, -1, false, 'i',
						  &textDatums, NULL, &ndatums);
		for (i = 0; i < ndatums; i++)
		{
			text	   *txtname = DatumGetTextPP(textDatums[i]);
			char	   *extName = text_to_cstring(txtname);
			Oid			extOid = get_extension_oid(extName, false);

			requiredExtensions = lappend_oid(requiredExtensions, extOid);
		}
	}

	InsertExtensionTuple(text_to_cstring(extName),
						 GetUserId(),
					   get_namespace_oid(text_to_cstring(schemaName), false),
						 relocatable,
						 text_to_cstring(extVersion),
						 extConfig,
						 extCondition,
						 requiredExtensions);

	PG_RETURN_VOID();
}

Datum
set_next_pg_namespace_oid(PG_FUNCTION_ARGS)
{
	Oid			nspid = PG_GETARG_OID(0);
	char	   *nspname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(nspid, NamespaceRelationId, nspname,
										   InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}
