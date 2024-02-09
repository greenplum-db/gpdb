/*
 *	pg_upgrade_support.c
 *
 *	server-side functions to set backend global variables
 *	to control oid and relfilenode assignment, and do other special
 *	hacks needed for pg_upgrade.
 *
 *	Copyright (c) 2010-2014, PostgreSQL Global Development Group
 *	contrib/pg_upgrade_support/pg_upgrade_support.c
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "access/transam.h"
#include "catalog/binary_upgrade.h"
#include "catalog/namespace.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_class.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbvars.h"
#include "commands/extension.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "rewrite/rewriteHandler.h"
#include "utils/array.h"
#include "utils/builtins.h"

/* THIS IS USED ONLY FOR PG >= 9.0 */

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern PGDLLIMPORT Oid binary_upgrade_next_toast_pg_type_oid;

extern PGDLLIMPORT Oid binary_upgrade_next_toast_pg_class_oid;

#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

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

PG_FUNCTION_INFO_V1(set_preassigned_oids);
PG_FUNCTION_INFO_V1(set_next_preassigned_tablespace_oid);

PG_FUNCTION_INFO_V1(view_has_anyarray_casts);
PG_FUNCTION_INFO_V1(view_has_unknown_casts);
PG_FUNCTION_INFO_V1(view_has_removed_operators);

static bool check_node_anyarray_walker(Node *node, void *context);
static bool check_node_unknown_walker(Node *node, void *context);
static bool check_node_removed_operators_walker(Node *node, void *context);

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

Datum
set_preassigned_oids(PG_FUNCTION_ARGS)
{
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(0);
	Datum	   *oids;
	int			nelems;
	int			i;

	deconstruct_array(array, OIDOID, sizeof(Oid), true, 'i',
					  &oids, NULL, &nelems);

	for (i = 0; i < nelems; i++)
	{
		Datum		oid = DatumGetObjectId(oids[i]);

		MarkOidPreassignedFromBinaryUpgrade(oid);
	}

	PG_RETURN_VOID();
}

Datum
set_next_preassigned_tablespace_oid(PG_FUNCTION_ARGS)
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

/*
 * Check for anyarray casts which may have corrupted the given view's definition
 * The corruption can result from the GPDB special handling for ANYARRAY types
 * in parse_coerce.c: coerce_type()
 */

Datum
view_has_anyarray_casts(PG_FUNCTION_ARGS)
{
	Oid			view_oid = PG_GETARG_OID(0);
	Relation 	rel = try_relation_open(view_oid, AccessShareLock, false);
	Query		*viewquery;
	bool		found;

	if (rel == NULL)
		elog(ERROR, "Could not open relation file for relation oid %u", view_oid);

	if(rel->rd_rel->relkind == RELKIND_VIEW)
	{
		viewquery = get_view_query(rel);
		found = query_tree_walker(viewquery, check_node_anyarray_walker, NULL, 0);
	}
	else
		found = false;

	relation_close(rel, AccessShareLock);

	PG_RETURN_BOOL(found);
}

static bool
check_node_anyarray_walker(Node *node, void *context)
{
	Assert(context == NULL);

	if (node == NULL)
		return false;

	/*
	 * Look only at Consts since the GPDB special handling hack for ANYARRAY
	 * types is only applied to Consts. See parse_coerce.c: coerce_type()
	 */
	if (IsA(node, Const))
	{
		Const *constant = (Const *) node;
		/*
		 * Check to see if the constant has an anyarray cast. If the constant's
		 * value is NULL, disregard. This is because NULL::anyarray is a valid
		 * expression and is encountered in the pg_stats catalog view.
		 */
		return constant->consttype == ANYARRAYOID && !constant->constisnull;
	}
	else if (IsA(node, Query))
	{
		/* recurse into subselects and ctes */
		Query *query = (Query *) node;
		return query_tree_walker(query, check_node_anyarray_walker, context, 0);
	}

	return expression_tree_walker(node, check_node_anyarray_walker,
								  context);
}

Datum
view_has_unknown_casts(PG_FUNCTION_ARGS)
{
	Oid			view_oid = PG_GETARG_OID(0);
	Relation 	rel = try_relation_open(view_oid, AccessShareLock, false);
	Query		*viewquery;
	bool		found;

	if (rel == NULL)
		elog(ERROR, "Could not open relation file for relation oid %u", view_oid);

	if(rel->rd_rel->relkind == RELKIND_VIEW)
	{
		viewquery = get_view_query(rel);
		found = query_tree_walker(viewquery, check_node_unknown_walker, NULL, 0);
	}
	else
		found = false;

	relation_close(rel, AccessShareLock);

	PG_RETURN_BOOL(found);
}

static bool
check_node_unknown_walker(Node *node, void *context)
{
	Assert(context == NULL);

	if (node == NULL)
		return false;

	/*
	 * Look only at FuncExpr since the GPDB special handling hack for unknown
	 * types is only applied to FuncExpr. See parse_coerce.c: coerce_type()
	 */
	if (IsA(node, FuncExpr))
	{
		FuncExpr *fe = (FuncExpr *) node;
		/*
		 * Check to see if the FuncExpr has an unknown::cstring explicit cast.
		 *
		 * If it has no such cast yet, check its arguments.
		 */
		if ((fe->funcresulttype != CSTRINGOID) || !fe->args || (list_length(fe->args) != 1) || (fe->funcformat == COERCE_IMPLICIT_CAST))
			return expression_tree_walker(node, check_node_unknown_walker, context);

		Node *head = lfirst(((List *)fe->args)->head);

		if (IsA(head, Var) && ((Var *)head)->vartype == UNKNOWNOID)
			return true;
		else
			return expression_tree_walker(node, check_node_unknown_walker, context);
	}
	else if (IsA(node, Query))
	{
		/* recurse into subselects and ctes */
		Query *query = (Query *) node;
		return query_tree_walker(query, check_node_unknown_walker, context, 0);
	}

	return expression_tree_walker(node, check_node_unknown_walker, context);
}

Datum
view_has_removed_operators(PG_FUNCTION_ARGS)
{
	Oid		  view_oid = PG_GETARG_OID(0);
	Relation  rel = try_relation_open(view_oid, AccessShareLock, false);
	Query	 *viewquery;
	bool	  found;

	if (rel == NULL)
		elog(ERROR, "Could not open relation file for relation oid %u", view_oid);

	if(rel->rd_rel->relkind == RELKIND_VIEW)
	{
		viewquery = get_view_query(rel);
		found = query_tree_walker(viewquery, check_node_removed_operators_walker, NULL, 0);
	}
	else
		found = false;

	relation_close(rel, AccessShareLock);

	PG_RETURN_BOOL(found);
}

static bool
check_node_removed_operators_walker(Node *node, void *context)
{
	Assert(context == NULL);

	if (node == NULL)
		return false;

	if (IsA(node, OpExpr))
	{
		Oid op_oid = ((OpExpr *)node)->opno;
		if (op_oid == 386) // int2vectoreq
			return true;

		return false;
	}
	else if (IsA(node, Query))
	{
		/* recurse into subselects and ctes */
		Query *query = (Query *) node;
		return query_tree_walker(query, check_node_removed_operators_walker, context, 0);
	}

	return expression_tree_walker(node, check_node_removed_operators_walker, context);
}

