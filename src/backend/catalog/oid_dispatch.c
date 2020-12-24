/*-------------------------------------------------------------------------
 *
 * oid_dispatch.c
 *		Functions to ensure that QD and QEs use same OIDs for catalog objects.
 *
 *
 * In Greenplum, it's important that most objects, like relations, functions,
 * operators, have the same OIDs in the master and all QE nodes.  Otherwise
 * query plans generated in the master will not work on the QE nodes, because
 * they use the master's OIDs to refer to objects.
 *
 * Whenever a CREATE statement, or any other command that creates new objects,
 * is dispatched, the master also needs to tell the QE servers which OIDs to
 * use for the new objects.  Before GPDB 5.0, that was done by modifying all
 * the structs representing DDL statements, like DefineStmt,
 * CreateOpClassStmt, and so forth, by adding a new OID field to them.
 * However, that was annoying when merging with the upstream, because it
 * required scattered changes to all the structs, and the accompanying
 * routines to copy and serialize them.  Moreover, for more complicated object
 * types, like a table, a single OID was not enough, as CREATE TABLE not only
 * creates the entry in pg_class, it also creates a composite type, an array
 * type for the composite type, and possibly the same for the associated toast
 * table.
 *
 * Starting with GPDB 5.0, we take a different tack.  Whenever a new OID is
 * needed for an object in PostgreSQL, the GetNewOidWithIndex() is used.
 * In GPDB, all the upstream calls to GetNewOidWithIndex() function have been
 * replaced with calls to the GetNewOidFor* functions in this file.  All the
 * GetNewOidFor*() functions are actually just wrappers for the
 * GetNewOrPreassignedOid() function, and only differ in the key arguments
 * for each object type.  GetNewOrPreassignedOid() does the heavy
 * lifting.  It behaves differently in the QD and the QEs:
 *
 * In the QD, GetNewOrPreassignedOid() generates a new OID by calling through
 * to the upstream GetNewOidWithIndex() function.  But it also records the
 * the generated OID in private memory, in the 'dispatch_oids' list, along
 * with the key for that object.  For example, when a new type is created,
 * the GetNewOrPreassignedOid() function generates a new OID, and records it
 * it along with the type's namespace and name in the 'dispatch_oids' list.
 * When the command is dispatched to the QE servers, all the recorded OIDs
 * are included in the dispatched request, and the QE processes in turn stash
 * the list into backend-private memory.  This is the 'preassigned_oids' list.
 *
 * In a QE node, when we reach the same code as in the QD to create a new
 * object, the GetNewOrPreassignedOid() function is called again.  The
 * function looks into the 'preassigned_oids' list to see if we had received
 * an OID for to use for the named object from the master. Under normal
 * circumstances, we should have pre-assigned OIDs for all objects created in
 * QEs, and the GetNewOrPreassignedOid() function will throw an error if we
 * don't.
 *
 * All in all, this provides a generic mechanism for DDL commands, to record
 * OIDs that are assigned for new objects in the master, transfer them to QE
 * nodes when the DDL command is dispatched, and for the QE nodes to use the
 * same, pre-assigned, OIDs for the objects.
 *
 * This same mechanism is used to preserve OIDs when upgrading a GPDB cluster
 * using pg_upgrade. pg_upgrade in PostgreSQL is using a set of global vars to
 * communicate the next OID for an object during upgrade, a strategy GPDB
 * doesn't employ due to the need for multiple OIDs for auxiliary objects.
 * pg_upgrade records the OIDs from the old cluster and inserts them into the
 * same 'preassigned_oids' list to restore them, that we use to assign specific
 * OIDs in a QE node at dispatch. Additionally, to ensure that object creation
 * that isn't bound by preassigned OIDs isn't consuming an OID that will later
 * in the restore process be preassigned, a separate list of all such OIDs is
 * maintained and queried before assigning a new non-preassigned OID.
 *
 * Portions Copyright 2016-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/oid_dispatch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_publication_rel.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_resqueuecapability.h"
#include "catalog/pg_resgroup.h"
#include "catalog/pg_resgroupcapability.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/oid_dispatch.h"
#include "cdb/cdbvars.h"
#include "executor/execdesc.h"
#include "lib/rbtree.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/* #define OID_DISPATCH_DEBUG */

/*
 * These were received from the QD, and should be consumed by the current
 * statement.
 */
static List *preassigned_oids = NIL;

/*
 * These will be sent to the QEs on next CdbDispatchUtilityStatement call.
 */
static List *dispatch_oids = NIL;

static MemoryContext oids_context = NULL;

static bool preserve_oids_on_commit = false;

/*
 * These will be used by the schema restoration process during binary upgrade,
 * so any new object must not use any Oid in this structure or else there will
 * be collisions.
 */
typedef struct
{
	RBTNode		rbnode;
	Oid			oid;
} OidPreassignment;

static RBTree *binary_upgrade_preassigned_oids;

static MemoryContext
get_oids_context(void)
{
	if (!oids_context)
		oids_context = AllocSetContextCreate(TopMemoryContext,
											 "Oid dispatch context",
											 ALLOCSET_SMALL_SIZES);

	return oids_context;
}

/*
 * Some commands, like VACUUM, start transactions of their own. Normally,
 * the list of assigned OIDs is reset at transaction commit, and warnings
 * are printed for any assignments that haven't been dispatched to the
 * segments. Calling PreserveOidAssignmentsOnCommit() changes that, so
 * that the list of assigned OIDs is preserved across commits, until you
 * call ClearOidAssignmentsOnCommit() to reset the flag. Abort always
 * clears the list and resets the flag.
 */
void
PreserveOidAssignmentsOnCommit(void)
{
	preserve_oids_on_commit = true;
}

void
ClearOidAssignmentsOnCommit(void)
{
	preserve_oids_on_commit = false;
}

/* ----------------------------------------------------------------
 * Functions for use in QE.
 * ----------------------------------------------------------------
 */

/*
 * Remember a list of pre-assigned OIDs, to be consumed later in the
 * transaction, when those system objects are created.
 */
void
AddPreassignedOids(List *l)
{
	ListCell *lc;
	MemoryContext old_context;

	if (IsBinaryUpgrade)
		elog(ERROR, "AddPreassignedOids called during binary upgrade");

	/*
	 * In the master, the OID-assignment-list is usually included in the next
	 * command that is dispatched, after an OID was assigned. In almost all
	 * cases, the dispatched command is the same CREATE command for which the
	 * oid was assigned. But I'm not sure if that's true for *all* commands,
	 * and so we don't require it. It is OK if an OID assignment is included
	 * in one dispatched command, but the command that needs the OID is only
	 * dispatched later in the same transaction. Therefore, don't reset the
	 * 'preassigned_oids' list, when it's dispatched.
	 */
	old_context = MemoryContextSwitchTo(get_oids_context());

	foreach(lc, l)
	{
		OidAssignment *p = (OidAssignment *) lfirst(lc);

		p = copyObject(p);
		preassigned_oids = lappend(preassigned_oids, p);

#ifdef OID_DISPATCH_DEBUG
		elog(NOTICE, "received OID assignment: catalog %u, namespace: %u, name: \"%s\": %u",
			 p->catalog, p->namespaceOid, p->objname ? p->objname : "", p->oid);
#endif
	}
	MemoryContextSwitchTo(old_context);

}

/*
 * For pg_upgrade_support functions, created during a binary upgrade, use OIDs
 * from a special reserved block of OIDs. We cannot use "normal" OIDs for these,
 * as they may collide with actual user objects restored later in the upgrade
 * process.
 */
static Oid BinaryUpgradeSchemaReservedOid = FirstBinaryUpgradeReservedObjectId;
static Oid NextBinaryUpgradeReservedOid = FirstBinaryUpgradeReservedObjectId + 1;

static Oid
AssignBinaryUpgradeReservedOid()
{
	Oid		result = NextBinaryUpgradeReservedOid;

	if (result > LastBinaryUpgradeReservedObjectId)
		elog(ERROR, "out of OIDs reserved for binary-upgrade");

	NextBinaryUpgradeReservedOid++;

	return result;
}

/*
 * Helper routine for GetPreassignedOidFor*() functions. Finds an entry in the
 * 'preassigned_oids' list with the given search key.
 *
 * If found, removes the entry from the list, and returns the OID. If not
 * found, returns InvalidOid.
 */
static Oid
GetPreassignedOid(OidAssignment *searchkey)
{
	ListCell   *cur_item;
	ListCell   *prev_item;
	Oid			oid;

	/*
	 * For binary_upgrade schema, and any functions in it, use OIDs
	 * from the reserved block.
	 */
	if (IsBinaryUpgrade)
	{
		if (searchkey->catalog == NamespaceRelationId &&
			strcmp(searchkey->objname, "binary_upgrade") == 0)
			return BinaryUpgradeSchemaReservedOid;
		if (searchkey->catalog == ProcedureRelationId &&
			searchkey->namespaceOid == BinaryUpgradeSchemaReservedOid)
			return AssignBinaryUpgradeReservedOid();
	}

	prev_item = NULL;
	cur_item = list_head(preassigned_oids);

	while (cur_item != NULL)
	{
		OidAssignment *p = (OidAssignment *) lfirst(cur_item);

		if (searchkey->catalog == p->catalog &&
			(searchkey->objname == NULL ||
			 (p->objname != NULL && strcmp(searchkey->objname, p->objname) == 0)) &&
			searchkey->namespaceOid == p->namespaceOid &&
			searchkey->keyOid1 == p->keyOid1 &&
			searchkey->keyOid2 == p->keyOid2)
		{
#ifdef OID_DISPATCH_DEBUG
			elog(NOTICE, "using OID assignment: catalog %u, namespace: %u, name: \"%s\": %u",
				 p->catalog, p->namespaceOid, p->objname ? p->objname : "", p->oid);
#endif

			oid = p->oid;
			preassigned_oids = list_delete_cell(preassigned_oids, cur_item, prev_item);
			pfree(p);
			return oid;
		}
		prev_item = cur_item;
		cur_item = lnext(cur_item);
	}

	return InvalidOid;
}

/* ----------------------------------------------------------------
 * Wrapper functions over upstream GetNewOidWithIndex(), that
 * memorize the OID for dispatch in the QD, and looks up the
 * pre-assigned OID in QE.
 *
 * When adding a function for a new catalog table, look at indexing.h to see what
 * the unique key columns for the table are.
 * ----------------------------------------------------------------
 */
static Oid
GetNewOrPreassignedOid(Relation relation, Oid indexId, AttrNumber oidcolumn,
					   OidAssignment *searchkey)
{
	Oid			oid;

	searchkey->catalog = RelationGetRelid(relation);

	if (Gp_role == GP_ROLE_EXECUTE || IsBinaryUpgrade)
	{
		oid = GetPreassignedOid(searchkey);

		/*
		 * During normal operation, all OIDs are preassigned unless the object
		 * type is exempt (in which case we should never reach here). During
		 * upgrades we do however allow objects to be created with new OIDs
		 * since objects may be created in new cluster which didn't exist in
		 * the old cluster.
		 */
		if (oid == InvalidOid)
		{
			if (IsBinaryUpgrade)
				oid = GetNewOidWithIndex(relation, indexId, oidcolumn);
			else
				elog(ERROR, "no pre-assigned OID for %s tuple \"%s\" (namespace:%u keyOid1:%u keyOid2:%u)",
					 RelationGetRelationName(relation), searchkey->objname ? searchkey->objname : "",
					 searchkey->namespaceOid, searchkey->keyOid1, searchkey->keyOid2);
		}
	}
	else if (Gp_role == GP_ROLE_DISPATCH)
	{
		MemoryContext oldcontext;

		/* Assign a new oid, and memorize it in the list of OIDs to dispatch */
		oid = GetNewOidWithIndex(relation, indexId, oidcolumn);

		oldcontext = MemoryContextSwitchTo(get_oids_context());
		searchkey->oid = oid;
		dispatch_oids = lappend(dispatch_oids, copyObject(searchkey));
		MemoryContextSwitchTo(oldcontext);

#ifdef OID_DISPATCH_DEBUG
		elog(NOTICE, "adding OID assignment: catalog \"%s\", namespace: %u, name: \"%s\": %u",
			 RelationGetRelationName(relation),
			 searchkey->namespaceOid,
			 searchkey->objname ? searchkey->objname : "",
			 searchkey->oid);
#endif
	}
	else
	{
		oid = GetNewOidWithIndex(relation, indexId, oidcolumn);
	}

	return oid;
}

Oid
GetNewOidForAccessMethod(Relation relation, Oid indexId, AttrNumber oidcolumn,
						 char *amname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == AccessMethodRelationId);
	Assert(indexId == AmOidIndexId);
	Assert(oidcolumn == Anum_pg_am_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.catalog = RelationGetRelid(relation);
	key.objname = amname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForAccessMethodOperator(Relation relation, Oid indexId, AttrNumber oidcolumn,
								 Oid amopfamily, Oid amoplefttype, Oid amoprighttype,
								 Oid amopstrategy)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == AccessMethodOperatorRelationId);
	Assert(indexId == AccessMethodOperatorOidIndexId);
	Assert(oidcolumn == Anum_pg_amop_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = amopfamily;
	key.keyOid2 = amoplefttype;
	key.keyOid3 = amoprighttype;
	key.keyOid4 = amopstrategy;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForAccessMethodProcedure(Relation relation, Oid indexId, AttrNumber oidcolumn,
								  Oid amprocfamily, Oid amproclefttype, Oid amprocrighttype,
								  Oid amproc)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == AccessMethodProcedureRelationId);
	Assert(indexId == AccessMethodProcedureOidIndexId);
	Assert(oidcolumn == Anum_pg_amproc_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = amprocfamily;
	key.keyOid2 = amproclefttype;
	key.keyOid3 = amprocrighttype;
	key.keyOid4 = amproc;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForAttrDefault(Relation relation, Oid indexId, AttrNumber oidcolumn,
						Oid adrelid, int16 adnum)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == AttrDefaultRelationId);
	Assert(indexId == AttrDefaultOidIndexId);
	Assert(oidcolumn == Anum_pg_attrdef_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = adrelid;
	key.keyOid2 = (Oid) adnum;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForAuthId(Relation relation, Oid indexId, AttrNumber oidcolumn,
				   char *rolname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == AuthIdRelationId);
	Assert(indexId == AuthIdOidIndexId);
	Assert(oidcolumn == Anum_pg_authid_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = rolname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForCast(Relation relation, Oid indexId, AttrNumber oidcolumn,
				 Oid castsource, Oid casttarget)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == CastRelationId);
	Assert(indexId == CastOidIndexId);
	Assert(oidcolumn == Anum_pg_cast_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = castsource;
	key.keyOid2 = casttarget;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForCollation(Relation relation, Oid indexId, AttrNumber oidcolumn,
					  Oid collnamespace, char *collname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == CollationRelationId);
	Assert(indexId == CollationOidIndexId);
	Assert(oidcolumn == Anum_pg_collation_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.namespaceOid = collnamespace;
	key.objname = collname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForConstraint(Relation relation, Oid indexId, AttrNumber oidcolumn,
					   Oid conrelid, Oid contypid, char *conname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ConstraintRelationId);
	Assert(indexId == ConstraintOidIndexId);
	Assert(oidcolumn == Anum_pg_constraint_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = conname;
	key.keyOid1 = conrelid;
	key.keyOid2 = contypid;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForConversion(Relation relation, Oid indexId, AttrNumber oidcolumn,
					   Oid connamespace, char *conname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ConversionRelationId);
	Assert(indexId == ConversionOidIndexId);
	Assert(oidcolumn == Anum_pg_conversion_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.namespaceOid = connamespace;
	key.objname = conname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}


/*
 * Databases are assigned slightly differently, because the QD
 * needs to do some extra checking on the Oid to check if it's suitable.
 * In the QD, call GetNewOidWithIndex like usual, and when you
 * find an OID that can be used, call RememberAssignedOidForDatabase()
 * to have it dispatched. In the QE, call GetPreassignedOidForDatabase().
 */
Oid
GetPreassignedOidForDatabase(const char *datname)
{
	OidAssignment searchkey;
	Oid			oid;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.type = T_OidAssignment;
	searchkey.catalog = DatabaseRelationId;
	searchkey.objname = (char *) datname;

	if ((oid = GetPreassignedOid(&searchkey)) == InvalidOid)
		elog(ERROR, "no pre-assigned OID for database \"%s\"", datname);
	return oid;
}

void
RememberAssignedOidForDatabase(const char *datname, Oid oid)
{
	MemoryContext oldcontext;
	OidAssignment *key;

	oldcontext = MemoryContextSwitchTo(get_oids_context());

	key = makeNode(OidAssignment);
	key->catalog = DatabaseRelationId;
	key->objname = (char *) pstrdup(datname);
	key->oid = oid;

	dispatch_oids = lappend(dispatch_oids, key);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Return the preassigned OID if it exists, but doesn't allocate or
 * complain if it doesn't.
 */
Oid
GetPreassignedOidForRelation(Oid namespaceOid, const char *relname)
{
	OidAssignment searchkey;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.catalog = RelationRelationId;
	searchkey.namespaceOid = namespaceOid;
	searchkey.objname = (char *) relname;

	return GetPreassignedOid(&searchkey);
}

/*
 * Return the preassigned OID if it exists, but doens't allocate or
 * complain if it doesn't.
 */
Oid
GetPreassignedOidForType(Oid namespaceOid, const char *typname)
{
	OidAssignment searchkey;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.catalog = TypeRelationId;
	searchkey.namespaceOid = namespaceOid;
	searchkey.objname = (char *) typname;

	return GetPreassignedOid(&searchkey);
}

/* Enums values have similar issues as databases */

Oid
GetPreassignedOidForEnum(Oid enumtypid, const char *enumlabel)
{
	OidAssignment searchkey;
	Oid			oid;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.type = T_OidAssignment;
	searchkey.catalog = EnumRelationId;
	searchkey.keyOid1 = enumtypid;
	searchkey.objname = (char *) enumlabel;

	if ((oid = GetPreassignedOid(&searchkey)) == InvalidOid)
		elog(ERROR, "no pre-assigned OID for enum label \"%s\" of %u", enumlabel, enumtypid);
	return oid;
}

void
RememberAssignedOidForEnum(Oid enumtypid, const char *enumlabel, Oid oid)
{
	MemoryContext oldcontext;
	OidAssignment *key;

	oldcontext = MemoryContextSwitchTo(get_oids_context());

	key = makeNode(OidAssignment);
	key->catalog = EnumRelationId;
	key->keyOid1 = enumtypid;
	key->objname = (char *) pstrdup(enumlabel);
	key->oid = oid;

	dispatch_oids = lappend(dispatch_oids, key);

	MemoryContextSwitchTo(oldcontext);
}

Oid
GetNewOidForDefaultAcl(Relation relation, Oid indexId, AttrNumber oidcolumn,
					   Oid defaclrole, Oid defaclnamespace, char defaclobjtype)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == DefaultAclRelationId);
	Assert(indexId == DefaultAclOidIndexId);
	Assert(oidcolumn == Anum_pg_default_acl_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = defaclrole;
	key.namespaceOid = defaclnamespace;
	key.keyOid2 = (Oid) defaclobjtype;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForExtension(Relation relation, Oid indexId, AttrNumber oidcolumn,
					  char *extname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ExtensionRelationId);
	Assert(indexId == ExtensionOidIndexId);
	Assert(oidcolumn == Anum_pg_extension_oid);

	/*
	 * Note that unlike most catalogs with a "namespace" column,
	 * extnamespace is not meant to imply that the extension
	 * belongs to that schema.
	 */
	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = extname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForExtprotocol(Relation relation, Oid indexId, AttrNumber oidcolumn,
						char *ptcname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ExtprotocolRelationId);
	Assert(indexId == ExtprotocolOidIndexId);
	Assert(oidcolumn == Anum_pg_extprotocol_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = ptcname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForForeignDataWrapper(Relation relation, Oid indexId, AttrNumber oidcolumn,
							   char *fdwname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ForeignDataWrapperRelationId);
	Assert(indexId == ForeignDataWrapperOidIndexId);
	Assert(oidcolumn == Anum_pg_foreign_data_wrapper_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = fdwname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForForeignServer(Relation relation, Oid indexId, AttrNumber oidcolumn,
						  char *srvname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ForeignServerRelationId);
	Assert(indexId == ForeignServerOidIndexId);
	Assert(oidcolumn == Anum_pg_foreign_server_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = srvname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);

}

Oid
GetNewOidForLanguage(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *lanname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == LanguageRelationId);
	Assert(indexId == LanguageOidIndexId);
	Assert(oidcolumn == Anum_pg_language_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = lanname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);

}
Oid
GetNewOidForNamespace(Relation relation, Oid indexId, AttrNumber oidcolumn,
					  char *nspname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == NamespaceRelationId);
	Assert(indexId == NamespaceOidIndexId);
	Assert(oidcolumn == Anum_pg_namespace_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = nspname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForOperator(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *oprname, Oid oprleft, Oid oprright, Oid oprnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == OperatorRelationId);
	Assert(indexId == OperatorOidIndexId);
	Assert(oidcolumn == Anum_pg_operator_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = oprname;
	key.keyOid1 = oprleft;
	key.keyOid2 = oprright;
	key.namespaceOid = oprnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForOperatorClass(Relation relation, Oid indexId, AttrNumber oidcolumn,
						  Oid opcmethod, char *opcname, Oid opcnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == OperatorClassRelationId);
	Assert(indexId == OpclassOidIndexId);
	Assert(oidcolumn == Anum_pg_opclass_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = opcmethod;
	key.objname = opcname;
	key.namespaceOid = opcnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForOperatorFamily(Relation relation, Oid indexId, AttrNumber oidcolumn,
						   Oid opfmethod, char *opfname, Oid opfnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == OperatorFamilyRelationId);
	Assert(indexId == OpfamilyOidIndexId);
	Assert(oidcolumn == Anum_pg_opfamily_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = opfmethod;
	key.objname = opfname;
	key.namespaceOid = opfnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForPolicy(Relation relation, Oid indexId, AttrNumber oidcolumn,
				   Oid polrelid, char *polname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == PolicyRelationId);
	Assert(indexId == PolicyOidIndexId);
	Assert(oidcolumn == Anum_pg_policy_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = polrelid;
	key.objname = polname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForProcedure(Relation relation, Oid indexId, AttrNumber oidcolumn,
					  char *proname, oidvector *proargtypes, Oid pronamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ProcedureRelationId);
	Assert(indexId == ProcedureOidIndexId);
	Assert(oidcolumn == Anum_pg_proc_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = proname;
	/*
	 * GPDB_12_MERGE_FIXME: we have nowhere to put 'proargtypes' in the OidAssignment
	 * struct currently. That's harmless, as long as we never try to dispatch the
	 * creation of two overloaded in one go. Isn't it a problem for binary upgrade,
	 * though?
	 */
	key.namespaceOid = pronamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForRelation(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *relname, Oid relnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == RelationRelationId);
	Assert(indexId == ClassOidIndexId);
	Assert(oidcolumn == Anum_pg_class_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = relname;
	key.namespaceOid = relnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForResQueue(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *rsqname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ResQueueRelationId);
	Assert(indexId == ResQueueOidIndexId);
	Assert(oidcolumn == Anum_pg_resqueue_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = rsqname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForRewrite(Relation relation, Oid indexId, AttrNumber oidcolumn,
					Oid ev_class, char *rulename)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == RewriteRelationId);
	Assert(indexId == RewriteOidIndexId);
	Assert(oidcolumn == Anum_pg_rewrite_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = ev_class;
	key.objname = rulename;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForSubscription(Relation relation, Oid indexId, AttrNumber oidcolumn,
						 Oid subdbid, char *subname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == SubscriptionRelationId);
	Assert(indexId == SubscriptionObjectIndexId);
	Assert(oidcolumn == Anum_pg_subscription_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = subdbid;
	key.objname = subname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTableSpace(Relation relation, Oid indexId, AttrNumber oidcolumn,
					   char *spcname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TableSpaceRelationId);
	Assert(indexId == TablespaceOidIndexId);
	Assert(oidcolumn == Anum_pg_tablespace_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = spcname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTransform(Relation relation, Oid indexId, AttrNumber oidcolumn,
					  Oid trftype, Oid trflang)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TransformRelationId);
	Assert(indexId == TransformOidIndexId);
	Assert(oidcolumn == Anum_pg_transform_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = trftype;
	key.keyOid2 = trflang;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTrigger(Relation relation, Oid indexId, AttrNumber oidcolumn,
					Oid tgrelid, char *tgname, Oid tgconstraint, Oid tgfid)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TriggerRelationId);
	Assert(indexId == TriggerOidIndexId);
	Assert(oidcolumn == Anum_pg_trigger_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = tgrelid;
	key.objname = tgname;
	key.keyOid2 = tgconstraint;
	key.keyOid3 = tgfid;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTSParser(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *prsname, Oid prsnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TSParserRelationId);
	Assert(indexId == TSParserOidIndexId);
	Assert(oidcolumn == Anum_pg_ts_parser_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = prsname;
	key.namespaceOid = prsnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTSDictionary(Relation relation, Oid indexId, AttrNumber oidcolumn,
						 char *dictname, Oid dictnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TSDictionaryRelationId);
	Assert(indexId == TSDictionaryOidIndexId);
	Assert(oidcolumn == Anum_pg_ts_dict_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = dictname;
	key.namespaceOid = dictnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTSTemplate(Relation relation, Oid indexId, AttrNumber oidcolumn,
					   char *tmplname, Oid tmplnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TSTemplateRelationId);
	Assert(indexId == TSTemplateOidIndexId);
	Assert(oidcolumn == Anum_pg_ts_template_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.namespaceOid = tmplnamespace;
	key.objname = tmplname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForTSConfig(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *cfgname, Oid cfgnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TSConfigRelationId);
	Assert(indexId == TSConfigOidIndexId);
	Assert(oidcolumn == Anum_pg_ts_config_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = cfgname;
	key.namespaceOid = cfgnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);

}

Oid
GetNewOidForType(Relation relation, Oid indexId, AttrNumber oidcolumn,
				 char *typname, Oid typnamespace)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == TypeRelationId);
	Assert(indexId == TypeOidIndexId);
	Assert(oidcolumn == Anum_pg_type_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = typname;
	key.namespaceOid = typnamespace;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);

}

Oid
GetNewOidForResGroup(Relation relation, Oid indexId, AttrNumber oidcolumn,
					 char *rsgname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == ResGroupRelationId);
	Assert(indexId == ResGroupOidIndexId);
	Assert(oidcolumn == Anum_pg_resgroup_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = rsgname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);

}
Oid
GetNewOidForUserMapping(Relation relation, Oid indexId, AttrNumber oidcolumn,
						Oid umuser, Oid umserver)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == UserMappingRelationId);
	Assert(indexId == UserMappingOidIndexId);
	Assert(oidcolumn == Anum_pg_user_mapping_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = umuser;
	key.keyOid2 = umserver;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForPublication(Relation relation, Oid indexId, AttrNumber oidcolumn,
						char *pubname)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == PublicationRelationId);
	Assert(indexId == PublicationObjectIndexId);
	Assert(oidcolumn == Anum_pg_publication_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.objname = pubname;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

Oid
GetNewOidForPublicationRel(Relation relation, Oid indexId, AttrNumber oidcolumn,
						   Oid prrelid, Oid prpubid)
{
	OidAssignment key;

	Assert(RelationGetRelid(relation) == PublicationRelRelationId);
	Assert(indexId == PublicationRelObjectIndexId);
	Assert(oidcolumn == Anum_pg_publication_rel_oid);

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.keyOid1 = prrelid;
	key.keyOid2 = prpubid;
	return GetNewOrPreassignedOid(relation, indexId, oidcolumn, &key);
}

/*
 * We also use the oid assignment list to remember the index names chosen for
 * partitioned indexes. This is slightly different from the normal use to
 * dispatch OIDs. The key is the parent index OID + child table OID, and
 * the thing we remember/dispatch is the index name chosen (compare with
 * the normal use, where the key is typically an object name, and we
 * remember/dispatch the OID of that object).
 */
char *
GetPreassignedIndexNameForChildIndex(Oid parentIdxOid, Oid childRelId)
{
	ListCell   *cur_item;
	ListCell   *prev_item;
	char	   *result = NULL;

	prev_item = NULL;
	cur_item = list_head(preassigned_oids);

	while (cur_item != NULL)
	{
		OidAssignment *p = (OidAssignment *) lfirst(cur_item);

		if (p->catalog == INDEX_NAME_ASSIGNMENT &&
			p->keyOid1 == parentIdxOid &&
			p->keyOid2 == childRelId)
		{
#ifdef OID_DISPATCH_DEBUG
			elog(NOTICE, "using index name assignment: parentIdxOid: %u childRelId: %u: %s",
				 p->keyOid1, p->keyOid2, p->objname);
#endif

			result = pstrdup(p->objname);
			preassigned_oids = list_delete_cell(preassigned_oids, cur_item, prev_item);
			pfree(p);
			return result;
		}
		prev_item = cur_item;
		cur_item = lnext(cur_item);
	}

	if (result == NULL)
		elog(ERROR, "no pre-assigned index name for parent index %u, child %u",
			 parentIdxOid, childRelId);
	return result;
}

void
RememberPreassignedIndexNameForChildIndex(Oid parentIdxOid, Oid childRelId, const char *idxname)
{
	MemoryContext oldcontext;
	OidAssignment *key;

	oldcontext = MemoryContextSwitchTo(get_oids_context());

	key = makeNode(OidAssignment);
	key->catalog = INDEX_NAME_ASSIGNMENT;
	key->keyOid1 = parentIdxOid;
	key->keyOid2 = childRelId;
	key->objname = (char *) pstrdup(idxname);

	dispatch_oids = lappend(dispatch_oids, key);

	MemoryContextSwitchTo(oldcontext);
}

/* ----------------------------------------------------------------
 * Functions for use in binary-upgrade mode
 * ----------------------------------------------------------------
 */

/*
 * Support functions for the Red-Black Tree which is used to keep the Oid
 * preassignments from the schema restore process during binary upgrade.
 */
static int
rbtree_cmp(const RBTNode *a, const RBTNode *b, void *arg)
{
	const OidPreassignment *prea = (const OidPreassignment *) a;
	const OidPreassignment *preb = (const OidPreassignment *) b;

	return prea->oid - preb->oid;
}

static RBTNode *
rbtree_alloc(void *arg)
{
	return (RBTNode *) palloc(sizeof(OidPreassignment));
}

static void
rbtree_free(RBTNode *node, void *arg)
{
	pfree(node);
}

/*
 * The RB Tree combiner function will be called when a new node has the same
 * key as an existing node (when rbtree_alloc() returns zero). For this
 * particular usecase the only value we have is the key, so make it a no-op.
 */
static void
rbtree_combine(RBTNode *existing pg_attribute_unused(), const RBTNode *new pg_attribute_unused(), void *arg)
{
	return;
}

/*
 * Remember an Oid which will be used in schema restoration during binary
 * upgrade, such that we can prohibit any new object to consume Oids which
 * will lead to collision.
 */
void
MarkOidPreassignedFromBinaryUpgrade(Oid oid)
{
	MemoryContext		oldcontext;
	OidPreassignment	node;
	bool				isnew;

	if (!IsBinaryUpgrade)
		elog(ERROR, "MarkOidPreassignedFromBinaryUpgrade called, but not in binary upgrade mode");

	if (oid == InvalidOid)
		return;

	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	if (!binary_upgrade_preassigned_oids)
	{
		binary_upgrade_preassigned_oids = rbt_create(sizeof(OidPreassignment),
													 rbtree_cmp,
													 rbtree_combine,
													 rbtree_alloc,
													 rbtree_free,
													 NULL);
	}

	node.oid = oid;
	rbt_insert(binary_upgrade_preassigned_oids, (RBTNode *) &node, &isnew);

	MemoryContextSwitchTo(oldcontext);
}


/*
 * Remember an OID which is set from loading a database dump performed
 * using the binary-upgrade flag.
 */
void
AddPreassignedOidFromBinaryUpgrade(Oid oid, Oid catalog, char *objname,
								   Oid namespaceOid, Oid keyOid1, Oid keyOid2)
{
	OidAssignment assignment;
	MemoryContext oldcontext;

	if (!IsBinaryUpgrade)
		elog(ERROR, "AddPreassignedOidFromBinaryUpgrade called, but not in binary upgrade mode");

	if (catalog == InvalidOid)
		elog(ERROR, "AddPreassignedOidFromBinaryUpgrade called with Invalid catalog relation Oid");

	if (Gp_role != GP_ROLE_UTILITY)
	{
		/* Perhaps we should error out and shut down here? */
		return;
	}

	memset(&assignment, 0, sizeof(OidAssignment));
	assignment.type = T_OidAssignment;

	/*
	 * This is essentially mimicking CreateKeyFromCatalogTuple except we set
	 * the members directly from the binary_upgrade function
	 */
	assignment.oid = oid;
	assignment.catalog = catalog;
	if (objname != NULL)
		assignment.objname = objname;
	if (namespaceOid != InvalidOid)
		assignment.namespaceOid = namespaceOid;
	if (keyOid1)
		assignment.keyOid1 = keyOid1;
	if (keyOid2)
		assignment.keyOid2 = keyOid2;

	/*
	 * Note that in binary-upgrade mode, the OID pre-assign calls are not done in
	 * the same transactions as the DDL commands that consume the OIDs. Hence they
	 * need to survive end-of-xact.
	 */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	preassigned_oids = lappend(preassigned_oids, copyObject(&assignment));

	MemoryContextSwitchTo(oldcontext);

#ifdef OID_DISPATCH_DEBUG
	elog(WARNING, "adding OID assignment: catalog \"%u\", namespace: %u, name: \"%s\": %u",
		 assignment.catalog,
		 assignment.namespaceOid,
		 assignment.objname ? assignment.objname : "",
		 assignment.oid);
#endif
}

/* ----------------------------------------------------------------
 * Functions for use in the master node.
 * ----------------------------------------------------------------
 */

/*
 * Get list of OIDs assigned in this transaction, since the last call.
 */
List *
GetAssignedOidsForDispatch(void)
{
	List	   *l;

	l = dispatch_oids;
	dispatch_oids = NIL;
	return l;
}

/*
 * Called at end-of-transaction. There is normally nothing to do,
 * but we perform some sanity checks.
 */
void
AtEOXact_DispatchOids(bool isCommit)
{
	if (preserve_oids_on_commit)
	{
		if (isCommit)
			return;
		preserve_oids_on_commit = false;
	}

	/*
	 * Reset the list of to-be-dispatched OIDs. (in QD)
	 *
	 * All the OID assignments should've been dispatched before end of
	 * transaction. Unless we're aborting.
	 */
	if (isCommit && dispatch_oids)
	{
		while (dispatch_oids)
		{
			OidAssignment *p = (OidAssignment *) linitial(dispatch_oids);

			elog(WARNING, "OID assignment not dispatched: catalog %u, namespace: %u, name: \"%s\"",
				 p->catalog, p->namespaceOid, p->objname ? p->objname : "");
			dispatch_oids = list_delete_first(dispatch_oids);
		}
		elog(ERROR, "oids were assigned, but not dispatched to QEs");
	}
	else
	{
		/* The list will be free'd with the memory context. */
		dispatch_oids = NIL;
	}

	/*
	 * Reset the list of pre-assigned OIDs (in QE).
	 *
	 * Normally, at commit, all the pre-assigned OIDs should be consumed
	 * already. But there are some corner cases where they're not. For
	 * example, when running a CREATE TABLE AS SELECT query, the command
	 * might be dispatched to multiple slices, but only the QE writer
	 * processes create the table. In the other processes, the OIDs will
	 * go unused.
	 *
	 * In binary-upgrade mode, however, the OID pre-assign calls are not
	 * done in the same transactions as the DDL commands that consume
	 * the OIDs. Hence they need to survive end-of-xact.
	 */
	if (!IsBinaryUpgrade)
	{
#ifdef OID_DISPATCH_DEBUG
		while (preassigned_oids)
		{
			OidAssignment *p = (OidAssignment *) linitial(preassigned_oids);

			elog(NOTICE, "unused pre-assigned OID: catalog %u, namespace: %u, name: \"%s\"",
				 p->catalog, p->namespaceOid, p->objname);
			preassigned_oids = list_delete_first(preassigned_oids);
		}
#endif
		preassigned_oids = NIL;

		MemoryContextReset(get_oids_context());
	}
}


/*
 * Is the given OID reserved for some other object?
 *
 * This is mainly of concern during binary upgrade, where we preassign
 * all the OIDs at the beginning of a restore. During normal operation,
 * there should be no clashes anyway.
 */
bool
IsOidAcceptable(Oid oid)
{
	ListCell *lc;
	OidPreassignment pre;

	foreach(lc, preassigned_oids)
	{
		OidAssignment *p = (OidAssignment *) lfirst(lc);

		if (p->oid == oid)
			return false;
	}

	if (binary_upgrade_preassigned_oids == NULL)
		return true;

	pre.oid = oid;
	return (rbt_find(binary_upgrade_preassigned_oids, (RBTNode *) &pre) == NULL);
}
