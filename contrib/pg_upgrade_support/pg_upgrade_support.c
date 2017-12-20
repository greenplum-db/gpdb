/*
 *	pg_upgrade_sysoids.c
 *
 *	server-side functions to set backend global variables
 *	to control oid and relfilenode assignment
 *
 *	Copyright (c) 2010, PostgreSQL Global Development Group
 *	$PostgreSQL: pgsql/contrib/pg_upgrade_support/pg_upgrade_support.c,v 1.5 2010/07/06 19:18:55 momjian Exp $
 */

#include "postgres.h"

#include "fmgr.h"
#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/pg_class.h"
#include "catalog/pg_type.h"
#include "commands/extension.h"
#include "miscadmin.h"
#include "utils/builtins.h"

#include "catalog/oid_dispatch.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_user_mapping.h"
#include "cdb/cdbvars.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum		add_pg_enum_label(PG_FUNCTION_ARGS);

Datum		create_empty_extension(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(add_pg_enum_label);

PG_FUNCTION_INFO_V1(create_empty_extension);

Datum
add_pg_enum_label(PG_FUNCTION_ARGS)
{
	Oid			enumoid = PG_GETARG_OID(0);
	Oid			typoid = PG_GETARG_OID(1);
	Name		label = PG_GETARG_NAME(2);

	EnumValuesCreate(typoid, list_make1(makeString(NameStr(*label))),
					 enumoid);

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


#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

/*
 * The preassign_<object>_oid() functions could all be implemented with a
 * single preassign_oid() function taking the object type as an argument. While
 * this would drastically reduce the amount of code in this file, it would make
 * reading the dumpfile harder, and thats why these are separate function.
 * Explicitly calling out the name in the function prototype and not taking any
 * superflous arguments makes it easier to visually inspect and verify the
 * dump. Should this be revisited, annotating the call with a comment in the
 * file could be one way forward but it's unclear whether it's worth
 * addressing.
 */

Datum		preassign_type_oid(PG_FUNCTION_ARGS);
Datum		preassign_arraytype_oid(PG_FUNCTION_ARGS);
Datum		preassign_extprotocol_oid(PG_FUNCTION_ARGS);
Datum		preassign_filespace_oid(PG_FUNCTION_ARGS);
Datum		preassign_tablespace_oid(PG_FUNCTION_ARGS);
Datum		preassign_opclass_oid(PG_FUNCTION_ARGS);
Datum		preassign_conversion_oid(PG_FUNCTION_ARGS);
Datum		preassign_resqueue_oid(PG_FUNCTION_ARGS);
Datum		preassign_resqueuecb_oid(PG_FUNCTION_ARGS);
Datum		preassign_cast_oid(PG_FUNCTION_ARGS);
Datum		preassign_opfam_oid(PG_FUNCTION_ARGS);
Datum		preassign_authid_oid(PG_FUNCTION_ARGS);
Datum		preassign_database_oid(PG_FUNCTION_ARGS);
Datum		preassign_language_oid(PG_FUNCTION_ARGS);
Datum		preassign_relation_oid(PG_FUNCTION_ARGS);
Datum		preassign_procedure_oid(PG_FUNCTION_ARGS);
Datum		preassign_namespace_oid(PG_FUNCTION_ARGS);
Datum		preassign_attrdef_oid(PG_FUNCTION_ARGS);
Datum		preassign_constraint_oid(PG_FUNCTION_ARGS);
Datum		preassign_rule_oid(PG_FUNCTION_ARGS);
Datum		preassign_operator_oid(PG_FUNCTION_ARGS);
Datum		preassign_tsparser_oid(PG_FUNCTION_ARGS);
Datum		preassign_tsdict_oid(PG_FUNCTION_ARGS);
Datum		preassign_tstemplate_oid(PG_FUNCTION_ARGS);
Datum		preassign_tsconfig_oid(PG_FUNCTION_ARGS);
Datum		preassign_extension_oid(PG_FUNCTION_ARGS);
Datum		preassign_enum_oid(PG_FUNCTION_ARGS);
Datum		preassign_amop_oid(PG_FUNCTION_ARGS);
Datum		preassign_fdw_oid(PG_FUNCTION_ARGS);
Datum		preassign_fserver_oid(PG_FUNCTION_ARGS);
Datum		preassign_user_mapping_oid(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(preassign_type_oid);
PG_FUNCTION_INFO_V1(preassign_arraytype_oid);
PG_FUNCTION_INFO_V1(preassign_extprotocol_oid);
PG_FUNCTION_INFO_V1(preassign_filespace_oid);
PG_FUNCTION_INFO_V1(preassign_tablespace_oid);
PG_FUNCTION_INFO_V1(preassign_opclass_oid);
PG_FUNCTION_INFO_V1(preassign_conversion_oid);
PG_FUNCTION_INFO_V1(preassign_resqueue_oid);
PG_FUNCTION_INFO_V1(preassign_resqueuecb_oid);
PG_FUNCTION_INFO_V1(preassign_cast_oid);
PG_FUNCTION_INFO_V1(preassign_opfam_oid);
PG_FUNCTION_INFO_V1(preassign_authid_oid);
PG_FUNCTION_INFO_V1(preassign_database_oid);
PG_FUNCTION_INFO_V1(preassign_language_oid);
PG_FUNCTION_INFO_V1(preassign_relation_oid);
PG_FUNCTION_INFO_V1(preassign_procedure_oid);
PG_FUNCTION_INFO_V1(preassign_namespace_oid);
PG_FUNCTION_INFO_V1(preassign_attrdef_oid);
PG_FUNCTION_INFO_V1(preassign_constraint_oid);
PG_FUNCTION_INFO_V1(preassign_rule_oid);
PG_FUNCTION_INFO_V1(preassign_operator_oid);
PG_FUNCTION_INFO_V1(preassign_tsparser_oid);
PG_FUNCTION_INFO_V1(preassign_tsdict_oid);
PG_FUNCTION_INFO_V1(preassign_tstemplate_oid);
PG_FUNCTION_INFO_V1(preassign_tsconfig_oid);
PG_FUNCTION_INFO_V1(preassign_extension_oid);
PG_FUNCTION_INFO_V1(preassign_enum_oid);
PG_FUNCTION_INFO_V1(preassign_amop_oid);
PG_FUNCTION_INFO_V1(preassign_fdw_oid);
PG_FUNCTION_INFO_V1(preassign_fserver_oid);
PG_FUNCTION_INFO_V1(preassign_user_mapping_oid);

Datum
preassign_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			typnamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(typoid, TypeRelationId,
						objname, typnamespace, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_arraytype_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			typnamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
		PG_RETURN_VOID();

	if (typoid == InvalidOid && GpIdentity.dbid != MASTER_DBID)
		PG_RETURN_VOID();

	AddPreassignedOidFromBinaryUpgrade(typoid, TypeRelationId, objname,
								typnamespace, InvalidOid, InvalidOid);

	PG_RETURN_VOID();
}

Datum
preassign_extprotocol_oid(PG_FUNCTION_ARGS)
{
	Oid			extprotoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(extprotoid, ExtprotocolRelationId, objname,
									InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_tablespace_oid(PG_FUNCTION_ARGS)
{
	Oid			tsoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(tsoid, TableSpaceRelationId, objname,
									InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_opclass_oid(PG_FUNCTION_ARGS)
{
	Oid			opcoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			opcnamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(opcoid, OperatorClassRelationId,
									objname, opcnamespace, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_opfam_oid(PG_FUNCTION_ARGS)
{
	Oid			opfoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			opfnamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(opfoid, OperatorFamilyRelationId,
									objname, opfnamespace, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_conversion_oid(PG_FUNCTION_ARGS)
{
	Oid			conoid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			connamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(conoid, ConversionRelationId,
									objname, connamespace, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_resqueue_oid(PG_FUNCTION_ARGS)
{
	Oid			resqueueid = PG_GETARG_OID(0);
	char	   *objname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(resqueueid, ResQueueRelationId,
									objname, InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_resqueuecb_oid(PG_FUNCTION_ARGS)
{
	Oid			resqueuecapabilityid = PG_GETARG_OID(0);
	Oid			resqueueid = PG_GETARG_OID(1);
	Oid			restypeid = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(resqueuecapabilityid, ResQueueCapabilityRelationId,
									NULL, InvalidOid, resqueueid, restypeid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_cast_oid(PG_FUNCTION_ARGS)
{
	Oid			castoid = PG_GETARG_OID(0);
	Oid			castsource = PG_GETARG_OID(1);
	Oid			casttarget = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(castoid, CastRelationId, NULL, InvalidOid,
										   castsource, casttarget);
	}

	PG_RETURN_VOID();
}

Datum
preassign_authid_oid(PG_FUNCTION_ARGS)
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
preassign_database_oid(PG_FUNCTION_ARGS)
{
	Oid			dboid = PG_GETARG_OID(0);
	char	   *dbname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(dboid, DatabaseRelationId, dbname,
										   InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_language_oid(PG_FUNCTION_ARGS)
{
	Oid			lanoid = PG_GETARG_OID(0);
	char	   *lanname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(lanoid, LanguageRelationId, lanname,
										   InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_relation_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);
	char	   *relname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			relnamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(reloid, RelationRelationId, relname,
										   relnamespace, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_procedure_oid(PG_FUNCTION_ARGS)
{
	Oid			procoid = PG_GETARG_OID(0);
	char	   *procname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			procnamespace = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(procoid, ProcedureRelationId, procname,
										   procnamespace, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_namespace_oid(PG_FUNCTION_ARGS)
{
	Oid			nspoid = PG_GETARG_OID(0);
	char	   *nspname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(nspoid, NamespaceRelationId, nspname,
										   InvalidOid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_attrdef_oid(PG_FUNCTION_ARGS)
{
	Oid			attdefoid = PG_GETARG_OID(0);
	Oid			adrelid = PG_GETARG_OID(1);
	Oid			adnum = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(attdefoid, AttrDefaultRelationId, NULL,
										   InvalidOid, adrelid, adnum);
	}

	PG_RETURN_VOID();
}

Datum
preassign_constraint_oid(PG_FUNCTION_ARGS)
{
	Oid			constoid = PG_GETARG_OID(0);
	Oid			nsoid = PG_GETARG_OID(1);
	char	   *constname = GET_STR(PG_GETARG_TEXT_P(2));
	Oid			conrelid = PG_GETARG_OID(3);
	Oid			contypid = PG_GETARG_OID(4);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(constoid, ConstraintRelationId, constname,
										   nsoid, conrelid, contypid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_rule_oid(PG_FUNCTION_ARGS)
{
	Oid			ruleoid = PG_GETARG_OID(0);
	Oid			tableoid = PG_GETARG_OID(1);
	char	   *rulename = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(ruleoid, RewriteRelationId, rulename,
										   InvalidOid, tableoid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_operator_oid(PG_FUNCTION_ARGS)
{
	Oid			opoid = PG_GETARG_OID(0);
	Oid			nsoid = PG_GETARG_OID(1);
	char	   *opname = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(opoid, OperatorRelationId, opname,
										   nsoid, InvalidOid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_tsparser_oid(PG_FUNCTION_ARGS)
{
	Oid			parseroid = PG_GETARG_OID(0);
	Oid			nsoid = PG_GETARG_OID(1);
	char	   *parsername = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(parseroid, TSParserRelationId,
										   parsername, nsoid, InvalidOid,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_tsdict_oid(PG_FUNCTION_ARGS)
{
	Oid			dictoid = PG_GETARG_OID(0);
	Oid			nsoid = PG_GETARG_OID(1);
	char	   *dictname = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(dictoid, TSDictionaryRelationId,
										   dictname, nsoid, InvalidOid,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_tstemplate_oid(PG_FUNCTION_ARGS)
{
	Oid			templateoid = PG_GETARG_OID(0);
	Oid			nsoid = PG_GETARG_OID(1);
	char	   *templatename = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(templateoid, TSTemplateRelationId,
										   templatename, nsoid, InvalidOid,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_tsconfig_oid(PG_FUNCTION_ARGS)
{
	Oid			configoid = PG_GETARG_OID(0);
	Oid			nsoid = PG_GETARG_OID(1);
	char	   *configname = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(configoid, TSConfigRelationId,
										   configname, nsoid, InvalidOid,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_extension_oid(PG_FUNCTION_ARGS)
{
	Oid			extensionoid = PG_GETARG_OID(0);
	char	   *extensionname = GET_STR(PG_GETARG_TEXT_P(1));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(extensionoid, ExtensionRelationId,
										   extensionname, InvalidOid, InvalidOid,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_enum_oid(PG_FUNCTION_ARGS)
{
	Oid			enumoid = PG_GETARG_OID(0);
	Oid			typeoid = PG_GETARG_OID(1);
	char	   *enumlabel = GET_STR(PG_GETARG_TEXT_P(2));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(enumoid, EnumRelationId, enumlabel,
										   InvalidOid, typeoid, InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_amop_oid(PG_FUNCTION_ARGS)
{
	Oid			amopoid = PG_GETARG_OID(0);
	Oid			amopmethod = PG_GETARG_OID(1);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(amopoid,
										   AccessMethodOperatorRelationId,
										   NULL,
										   InvalidOid,
										   amopmethod,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_fdw_oid(PG_FUNCTION_ARGS)
{
	Oid			fdwoid = PG_GETARG_OID(0);
	char	   *fdwname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			fdwowneroid = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(fdwoid,
										   ForeignDataWrapperRelationId,
										   fdwname,
										   InvalidOid,
										   fdwowneroid,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_fserver_oid(PG_FUNCTION_ARGS)
{
	Oid			fsrvoid = PG_GETARG_OID(0);
	char	   *fsrvname = GET_STR(PG_GETARG_TEXT_P(1));
	Oid			fsrvfdw = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(fsrvoid,
										   ForeignServerRelationId,
										   fsrvname,
										   InvalidOid,
										   fsrvfdw,
										   InvalidOid);
	}

	PG_RETURN_VOID();
}

Datum
preassign_user_mapping_oid(PG_FUNCTION_ARGS)
{
	Oid			umoid = PG_GETARG_OID(0);
	Oid			umuser = PG_GETARG_OID(1);
	Oid			umserver = PG_GETARG_OID(2);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		AddPreassignedOidFromBinaryUpgrade(umoid, UserMappingRelationId,
										   NULL, InvalidOid, umuser, umserver);
	}

	PG_RETURN_VOID();
}
