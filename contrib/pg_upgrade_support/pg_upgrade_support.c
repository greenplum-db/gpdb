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
PG_FUNCTION_INFO_V1(view_has_removed_functions);
PG_FUNCTION_INFO_V1(view_has_removed_types);

static bool check_node_anyarray_walker(Node *node, void *context);
static bool check_node_unknown_walker(Node *node, void *context);
static bool check_node_removed_operators_walker(Node *node, void *context);
static bool check_node_removed_functions_walker(Node *node, void *context);
static bool check_node_removed_types_walker(Node *node, void *context);

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

Datum
view_has_removed_functions(PG_FUNCTION_ARGS)
{
	Oid		  view_oid = PG_GETARG_OID(0);
	Relation  rel = try_relation_open(view_oid, AccessShareLock, false);
	Query	 *viewquery;
	bool	  found;

	if (!RelationIsValid(rel))
		elog(ERROR, "Could not open relation file for relation oid %u", view_oid);

	if(rel->rd_rel->relkind == RELKIND_VIEW)
	{
		viewquery = get_view_query(rel);
		found = query_tree_walker(viewquery, check_node_removed_functions_walker, NULL, 0);
	}
	else
		found = false;

	relation_close(rel, AccessShareLock);

	PG_RETURN_BOOL(found);
}

static bool
check_node_removed_functions_walker(Node *node, void *context)
{
	Assert(context == NULL);

	if (node == NULL)
		return false;

	if (IsA(node, FuncExpr))
	{
		Oid func_oid = ((FuncExpr *)node)->funcid;
		if (func_oid == 12512 || // gp_toolkit.__gp_get_ao_entry_from_cache
			func_oid == 12511 || // gp_toolkit.__gp_remove_ao_entry_from_cache
			func_oid == 12498 || // gp_toolkit.pg_resgroup_check_move_query
			func_oid ==  7188 || // pg_catalog.bmbeginscan
			func_oid ==  7193 || // pg_catalog.bmbuild
			func_oid ==  7011 || // pg_catalog.bmbuildempty
			func_oid ==  7194 || // pg_catalog.bmbulkdelete
			func_oid ==  7196 || // pg_catalog.bmcostestimate
			func_oid ==  7190 || // pg_catalog.bmendscan
			func_oid ==  7051 || // pg_catalog.bmgetbitmap
			func_oid ==  7050 || // pg_catalog.bmgettuple
			func_oid ==  7187 || // pg_catalog.bminsert
			func_oid ==  7191 || // pg_catalog.bmmarkpos
			func_oid ==  7197 || // pg_catalog.bmoptions
			func_oid ==  7189 || // pg_catalog.bmrescan
			func_oid ==  7192 || // pg_catalog.bmrestrpos
			func_oid ==  7195 || // pg_catalog.bmvacuumcleanup
			func_oid ==   333 || // pg_catalog.btbeginscan
			func_oid ==   338 || // pg_catalog.btbuild
			func_oid ==   328 || // pg_catalog.btbuildempty
			func_oid ==   332 || // pg_catalog.btbulkdelete
			func_oid ==  6276 || // pg_catalog.btcanreturn
			func_oid ==  1268 || // pg_catalog.btcostestimate
			func_oid ==   335 || // pg_catalog.btendscan
			func_oid ==   636 || // pg_catalog.btgetbitmap
			func_oid ==   330 || // pg_catalog.btgettuple
			func_oid ==   331 || // pg_catalog.btinsert
			func_oid ==   336 || // pg_catalog.btmarkpos
			func_oid ==  6785 || // pg_catalog.btoptions
			func_oid ==   334 || // pg_catalog.btrescan
			func_oid ==   337 || // pg_catalog.btrestrpos
			func_oid ==   972 || // pg_catalog.btvacuumcleanup
			func_oid ==  2733 || // pg_catalog.ginbeginscan
			func_oid ==  2738 || // pg_catalog.ginbuild
			func_oid ==   325 || // pg_catalog.ginbuildempty
			func_oid ==  2739 || // pg_catalog.ginbulkdelete
			func_oid ==  6741 || // pg_catalog.gincostestimate
			func_oid ==  2735 || // pg_catalog.ginendscan
			func_oid ==  2731 || // pg_catalog.gingetbitmap
			func_oid ==  2732 || // pg_catalog.gininsert
			func_oid ==  2736 || // pg_catalog.ginmarkpos
			func_oid ==  2788 || // pg_catalog.ginoptions
			func_oid ==  2734 || // pg_catalog.ginrescan
			func_oid ==  2737 || // pg_catalog.ginrestrpos
			func_oid ==  6740 || // pg_catalog.ginvacuumcleanup
			func_oid ==   777 || // pg_catalog.gistbeginscan
			func_oid ==  2579 || // pg_catalog.gist_box_compress
			func_oid ==  2580 || // pg_catalog.gist_box_decompress
			func_oid ==   782 || // pg_catalog.gistbuild
			func_oid ==   326 || // pg_catalog.gistbuildempty
			func_oid ==   776 || // pg_catalog.gistbulkdelete
			func_oid ==   772 || // pg_catalog.gistcostestimate
			func_oid ==   779 || // pg_catalog.gistendscan
			func_oid ==   638 || // pg_catalog.gistgetbitmap
			func_oid ==   774 || // pg_catalog.gistgettuple
			func_oid ==   775 || // pg_catalog.gistinsert
			func_oid ==   780 || // pg_catalog.gistmarkpos
			func_oid ==  6787 || // pg_catalog.gistoptions
			func_oid ==   778 || // pg_catalog.gistrescan
			func_oid ==   781 || // pg_catalog.gistrestrpos
			func_oid ==  2561 || // pg_catalog.gistvacuumcleanup
			func_oid ==  5044 || // pg_catalog.gp_elog
			func_oid ==  5045 || // pg_catalog.gp_elog
			func_oid ==  9999 || // pg_catalog.gp_fault_inject
			func_oid == 12531 || // pg_catalog.gp_quicklz_compress
			func_oid == 12529 || // pg_catalog.gp_quicklz_constructor
			func_oid == 12532 || // pg_catalog.gp_quicklz_decompress
			func_oid == 12530 || // pg_catalog.gp_quicklz_destructor
			func_oid == 12533 || // pg_catalog.gp_quicklz_validator
			func_oid ==  7173 || // pg_catalog.gp_update_ao_master_stats
			func_oid ==  3696 || // pg_catalog.gtsquery_decompress
			func_oid ==   443 || // pg_catalog.hashbeginscan
			func_oid ==   448 || // pg_catalog.hashbuild
			func_oid ==   327 || // pg_catalog.hashbuildempty
			func_oid ==   442 || // pg_catalog.hashbulkdelete
			func_oid ==   438 || // pg_catalog.hashcostestimate
			func_oid ==   445 || // pg_catalog.hashendscan
			func_oid ==   637 || // pg_catalog.hashgetbitmap
			func_oid ==   440 || // pg_catalog.hashgettuple
			func_oid ==   441 || // pg_catalog.hashinsert
			func_oid ==   398 || // pg_catalog.hashint2vector
			func_oid ==   446 || // pg_catalog.hashmarkpos
			func_oid ==  6786 || // pg_catalog.hashoptions
			func_oid ==   444 || // pg_catalog.hashrescan
			func_oid ==   447 || // pg_catalog.hashrestrpos
			func_oid ==   425 || // pg_catalog.hashvacuumcleanup
			func_oid ==  3556 || // pg_catalog.inet_gist_decompress
			func_oid ==   315 || // pg_catalog.int2vectoreq
			func_oid ==  7597 || // pg_catalog.numeric2point
			func_oid ==  3157 || // pg_catalog.numeric_transform
			func_oid ==  2852 || // pg_catalog.pg_current_xlog_insert_location
			func_oid ==  2849 || // pg_catalog.pg_current_xlog_location
			func_oid ==  5024 || // pg_catalog.pg_get_partition_def
			func_oid ==  5034 || // pg_catalog.pg_get_partition_def
			func_oid ==  5025 || // pg_catalog.pg_get_partition_def
			func_oid ==  5028 || // pg_catalog.pg_get_partition_rule_def
			func_oid ==  5027 || // pg_catalog.pg_get_partition_rule_def
			func_oid ==  5037 || // pg_catalog.pg_get_partition_template_def
			func_oid ==  3073 || // pg_catalog.pg_is_xlog_replay_paused
			func_oid ==  3820 || // pg_catalog.pg_last_xlog_receive_location
			func_oid ==  3821 || // pg_catalog.pg_last_xlog_replay_location
			func_oid ==  2853 || // pg_catalog.pg_stat_get_backend_waiting
			func_oid ==  7298 || // pg_catalog.pg_stat_get_backend_waiting_reason
			func_oid ==  2848 || // pg_catalog.pg_switch_xlog
			func_oid ==  2851 || // pg_catalog.pg_xlogfile_name
			func_oid ==  2850 || // pg_catalog.pg_xlogfile_name_offset
			func_oid ==  3165 || // pg_catalog.pg_xlog_location_diff
			func_oid ==  3071 || // pg_catalog.pg_xlog_replay_pause
			func_oid ==  3072 || // pg_catalog.pg_xlog_replay_resume
			func_oid ==  3877 || // pg_catalog.range_gist_compress
			func_oid ==  3878 || // pg_catalog.range_gist_decompress
			func_oid ==  4004 || // pg_catalog.spgbeginscan
			func_oid ==  4009 || // pg_catalog.spgbuild
			func_oid ==  4010 || // pg_catalog.spgbuildempty
			func_oid ==  4011 || // pg_catalog.spgbulkdelete
			func_oid ==  4032 || // pg_catalog.spgcanreturn
			func_oid ==  4013 || // pg_catalog.spgcostestimate
			func_oid ==  4006 || // pg_catalog.spgendscan
			func_oid ==  4002 || // pg_catalog.spggetbitmap
			func_oid ==  4001 || // pg_catalog.spggettuple
			func_oid ==  4003 || // pg_catalog.spginsert
			func_oid ==  4007 || // pg_catalog.spgmarkpos
			func_oid ==  4014 || // pg_catalog.spgoptions
			func_oid ==  4005 || // pg_catalog.spgrescan
			func_oid ==  4008 || // pg_catalog.spgrestrpos
			func_oid ==  4012 || // pg_catalog.spgvacuumcleanup
			func_oid ==  3917 || // pg_catalog.timestamp_transform
			func_oid ==  3944 || // pg_catalog.time_transform
			func_oid ==  3158 || // pg_catalog.varbit_transform
			func_oid ==  3097)   // pg_catalog.varchar_transform
			return true;

		return false;
	}
	else if (IsA(node, Query))
	{
		/* recurse into subselects and ctes */
		Query *query = (Query *) node;
		return query_tree_walker(query, check_node_removed_functions_walker, context, 0);
	}

	return expression_tree_walker(node, check_node_removed_functions_walker, context);
}


Datum
view_has_removed_types(PG_FUNCTION_ARGS)
{
	Oid		  view_oid = PG_GETARG_OID(0);
	Relation  rel = try_relation_open(view_oid, AccessShareLock, false);
	Query	 *viewquery;
	bool	  found;

	if (!RelationIsValid(rel))
		elog(ERROR, "Could not open relation file for relation oid %u", view_oid);

	if(rel->rd_rel->relkind == RELKIND_VIEW)
	{
		viewquery = get_view_query(rel);
		found = query_tree_walker(viewquery, check_node_removed_types_walker, NULL, 0);
	}
	else
		found = false;

	relation_close(rel, AccessShareLock);

	PG_RETURN_BOOL(found);
}

static bool
check_node_removed_types_walker(Node *node, void *context)
{
	Assert(context == NULL);

	if (node == NULL)
		return false;

	if (IsA(node, Var) || IsA(node, Const))
	{
		Oid type_oid;
		if IsA(node, Var)
			type_oid = ((Var *)node)->vartype;
		else
			type_oid = ((Const *)node)->consttype;

		if (type_oid == 12475 || // gp_toolkit.gp_size_of_partition_and_indexes_disk
			type_oid == 12366 || // gp_toolkit.__gp_user_data_tables
			type_oid ==  1023 || // pg_catalog._abstime
			type_oid ==   702 || // pg_catalog.abstime
			type_oid == 11612 || // pg_catalog.pg_partition
			type_oid == 11787 || // pg_catalog.pg_partition_columns
			type_oid == 11617 || // pg_catalog.pg_partition_encoding
			type_oid == 11613 || // pg_catalog.pg_partition_rule
			type_oid == 11783 || // pg_catalog.pg_partitions
			type_oid == 11790 || // pg_catalog.pg_partition_templates
			type_oid == 11797 || // pg_catalog.pg_stat_partition_operations
			type_oid ==  1024 || // pg_catalog._reltime
			type_oid ==   703 || // pg_catalog.reltime
			type_oid ==   210 || // pg_catalog.smgr
			type_oid ==  1025 || // pg_catalog._tinterval
			type_oid ==   704)   // pg_catalog.tinterval
			return true;

		return false;
	}
	else if (IsA(node, Query))
	{
		/* recurse into subselects and ctes */
		Query *query = (Query *) node;
		return query_tree_walker(query, check_node_removed_types_walker, context, 0);
	}

	return expression_tree_walker(node, check_node_removed_types_walker, context);
}
