/*-------------------------------------------------------------------------
 *
 * schemacmds.c
 *	  schema creation/manipulation commands
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/schemacmds.c,v 1.53 2009/06/11 14:48:56 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_namespace.h"
#include "commands/dbcommands.h"
#include "commands/schemacmds.h"
#include "miscadmin.h"
#include "parser/parse_utilcmd.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbvars.h"

static void AlterSchemaOwner_internal(HeapTuple tup, Relation rel, Oid newOwnerId);


/*
 * CREATE SCHEMA
 */
void
CreateSchemaCommand(CreateSchemaStmt *stmt, const char *queryString)
{
	const char *schemaName = stmt->schemaname;
	const char *authId = stmt->authid;
	Oid			namespaceId;
	OverrideSearchPath *overridePath;
	List	   *parsetree_list;
	ListCell   *parsetree_item;
	Oid			owner_uid;
	Oid			saved_uid;
	int			save_sec_context;
	AclResult	aclresult;
	bool		shouldDispatch = (Gp_role == GP_ROLE_DISPATCH && 
								  !IsBootstrapProcessingMode());

	/*
	 * GPDB: Creation of temporary namespaces is a special case. This statement
	 * is dispatched by the dispatcher node the first time a temporary table is
	 * created. It bypasses all the normal checks and logic of schema creation,
	 * and is routed to the internal routine for creating temporary namespaces,
	 * instead.
	 */
	if (stmt->istemp)
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);

		Assert(stmt->schemaname == InvalidOid);
		Assert(stmt->authid == NULL);
		Assert(stmt->schemaElts == NIL);

		InitTempTableNamespace();
		return;
	}

	GetUserIdAndSecContext(&saved_uid, &save_sec_context);

	/*
	 * Who is supposed to own the new schema?
	 */
	if (authId)
		owner_uid = get_roleid_checked(authId);
	else
		owner_uid = saved_uid;

	/*
	 * To create a schema, must have schema-create privilege on the current
	 * database and must be able to become the target role (this does not
	 * imply that the target role itself must have create-schema privilege).
	 * The latter provision guards against "giveaway" attacks.	Note that a
	 * superuser will always have both of these privileges a fortiori.
	 */
	aclresult = pg_database_aclcheck(MyDatabaseId, saved_uid, ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_DATABASE,
					   get_database_name(MyDatabaseId));

	check_is_member_of_role(saved_uid, owner_uid);

	/* Additional check to protect reserved schema names */
	if (!allowSystemTableModsDDL && IsReservedName(schemaName))
	{
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("unacceptable schema name \"%s\"", schemaName),
				 errdetail("The prefix \"%s\" is reserved for system schemas.",
						   GetReservedPrefix(schemaName))));
	}

	/*
	 * If the requested authorization is different from the current user,
	 * temporarily set the current user so that the object(s) will be created
	 * with the correct ownership.
	 *
	 * (The setting will be restored at the end of this routine, or in case of
	 * error, transaction abort will clean things up.)
	 */
	if (saved_uid != owner_uid)
		SetUserIdAndSecContext(owner_uid,
							   save_sec_context | SECURITY_LOCAL_USERID_CHANGE);

	/* Create the schema's namespace */
	if (shouldDispatch || Gp_role != GP_ROLE_EXECUTE)
	{
		namespaceId = NamespaceCreate(schemaName, owner_uid);

		if (shouldDispatch)
		{
			elog(DEBUG5, "shouldDispatch = true, namespaceOid = %d", namespaceId);

			/*
			 * Dispatch the command to all primary and mirror segment dbs.
			 * Starts a global transaction and reconfigures cluster if needed.
			 * Waits for QEs to finish.  Exits via ereport(ERROR,...) if error.
			 */
			CdbDispatchUtilityStatement((Node *) stmt,
										DF_CANCEL_ON_ERROR |
										DF_WITH_SNAPSHOT |
										DF_NEED_TWO_PHASE,
										GetAssignedOidsForDispatch(),
										NULL);
		}

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
			MetaTrackAddObject(NamespaceRelationId,
							   namespaceId,
							   saved_uid,
							   "CREATE", "SCHEMA"
					);
	}
	else
	{
		namespaceId = NamespaceCreate(schemaName, owner_uid);
	}

	/* Advance cmd counter to make the namespace visible */
	CommandCounterIncrement();

	/*
	 * Temporarily make the new namespace be the front of the search path, as
	 * well as the default creation target namespace.  This will be undone at
	 * the end of this routine, or upon error.
	 */
	overridePath = GetOverrideSearchPath(CurrentMemoryContext);
	overridePath->schemas = lcons_oid(namespaceId, overridePath->schemas);
	/* XXX should we clear overridePath->useTemp? */
	PushOverrideSearchPath(overridePath);

	/*
	 * Examine the list of commands embedded in the CREATE SCHEMA command, and
	 * reorganize them into a sequentially executable order with no forward
	 * references.	Note that the result is still a list of raw parsetrees ---
	 * we cannot, in general, run parse analysis on one statement until we
	 * have actually executed the prior ones.
	 */
	parsetree_list = transformCreateSchemaStmt(stmt);

	/*
	 * Execute each command contained in the CREATE SCHEMA.  Since the grammar
	 * allows only utility commands in CREATE SCHEMA, there is no need to pass
	 * them through parse_analyze() or the rewriter; we can just hand them
	 * straight to ProcessUtility.
	 */
	foreach(parsetree_item, parsetree_list)
	{
		Node	   *stmt = (Node *) lfirst(parsetree_item);

		/* do this step */
		ProcessUtility(stmt,
					   queryString,
					   NULL,
					   false,	/* not top level */
					   None_Receiver,
					   NULL);
		/* make sure later steps can see the object created here */
		CommandCounterIncrement();
	}

	/* Reset search path to normal state */
	PopOverrideSearchPath();

	/* Reset current user and security context */
	SetUserIdAndSecContext(saved_uid, save_sec_context);
}


/*
 *	RemoveSchemas
 *		Implements DROP SCHEMA.
 */
void
RemoveSchemas(DropStmt *drop)
{
	ObjectAddresses *objects;
	ListCell   *cell;
	List	   *namespaceIdList = NIL;

	/*
	 * First we identify all the schemas, then we delete them in a single
	 * performMultipleDeletions() call.  This is to avoid unwanted DROP
	 * RESTRICT errors if one of the schemas depends on another.
	 */
	objects = new_object_addresses();

	foreach(cell, drop->objects)
	{
		List	   *names = (List *) lfirst(cell);
		char	   *namespaceName;
		Oid			namespaceId;
		ObjectAddress object;

		if (list_length(names) != 1)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("schema name cannot be qualified")));
		namespaceName = strVal(linitial(names));

		namespaceId = GetSysCacheOid(NAMESPACENAME,
									 CStringGetDatum(namespaceName),
									 0, 0, 0);

		if (!OidIsValid(namespaceId))
		{
			if (!drop->missing_ok)
			{
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_SCHEMA),
						 errmsg("schema \"%s\" does not exist",
								namespaceName)));
			}
			else
			{
				if (Gp_role != GP_ROLE_EXECUTE)
					ereport(NOTICE,
							(errmsg("schema \"%s\" does not exist, skipping",
								namespaceName)));
			}
			continue;
		}

		namespaceIdList = lappend_oid(namespaceIdList, namespaceId);

		/* Permission check */
		if (!pg_namespace_ownercheck(namespaceId, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_NAMESPACE,
						   namespaceName);

		/*
		 * Additional check to protect reserved schema names.
		 *
		 * But allow dropping temp schemas. This makes it much easier to get rid
		 * of leaked temp schemas. I wish it wasn't necessary, but we do tend to
		 * leak them on crashes, so let's make life a bit easier for admins.
		 * gpcheckcat will also try to automatically drop any leaked temp schemas.
		 */
		if (!allowSystemTableModsDDL &&	IsReservedName(namespaceName) &&
			strncmp(namespaceName, "pg_temp_", 8) != 0 &&
			strncmp(namespaceName, "pg_toast_temp_", 14) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_RESERVED_NAME),
					 errmsg("cannot drop schema %s because it is required by the database system",
							namespaceName)));

		object.classId = NamespaceRelationId;
		object.objectId = namespaceId;
		object.objectSubId = 0;

		add_exact_object_address(&object, objects);
	}

	/*
	 * Do the deletions.  Objects contained in the schema(s) are removed by
	 * means of their dependency links to the schema.
	 */
	performMultipleDeletions(objects, drop->behavior);

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		foreach(cell, namespaceIdList)
		{
			Oid namespaceId = lfirst_oid(cell);

			MetaTrackDropObject(NamespaceRelationId, namespaceId);
		}
	}

	free_object_addresses(objects);
}


/*
 * Guts of schema deletion.
 */
void
RemoveSchemaById(Oid schemaOid)
{
	Relation	relation;
	HeapTuple	tup;

	relation = heap_open(NamespaceRelationId, RowExclusiveLock);

	tup = SearchSysCache(NAMESPACEOID,
						 ObjectIdGetDatum(schemaOid),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup)) /* should not happen */
		elog(ERROR, "cache lookup failed for namespace %u", schemaOid);

	simple_heap_delete(relation, &tup->t_self);

	ReleaseSysCache(tup);

	heap_close(relation, RowExclusiveLock);
}

/*
 * Rename schema
 */
void
RenameSchema(const char *oldname, const char *newname)
{
	HeapTuple	tup;
	Oid			nsoid;
	Relation	rel;
	AclResult	aclresult;

	rel = heap_open(NamespaceRelationId, RowExclusiveLock);

	tup = SearchSysCacheCopy(NAMESPACENAME,
							 CStringGetDatum(oldname),
							 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_SCHEMA),
				 errmsg("schema \"%s\" does not exist", oldname)));

	/* make sure the new name doesn't exist */
	if (HeapTupleIsValid(
						 SearchSysCache(NAMESPACENAME,
										CStringGetDatum(newname),
										0, 0, 0)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_SCHEMA),
				 errmsg("schema \"%s\" already exists", newname)));

	/* must be owner */
	nsoid = HeapTupleGetOid(tup);
	if (!pg_namespace_ownercheck(nsoid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_NAMESPACE,
					   oldname);

	/* must have CREATE privilege on database */
	aclresult = pg_database_aclcheck(MyDatabaseId, GetUserId(), ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_DATABASE,
					   get_database_name(MyDatabaseId));

	if (!allowSystemTableModsDDL && IsReservedName(oldname))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to ALTER SCHEMA \"%s\"", oldname),
				 errdetail("Schema %s is reserved for system use.", oldname)));
	}

	if (!allowSystemTableModsDDL && IsReservedName(newname))
	{
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("unacceptable schema name \"%s\"", newname),
				 errdetail("The prefix \"%s\" is reserved for system schemas.",
						   GetReservedPrefix(newname))));
	}


	/* rename */
	namestrcpy(&(((Form_pg_namespace) GETSTRUCT(tup))->nspname), newname);
	simple_heap_update(rel, &tup->t_self, tup);
	CatalogUpdateIndexes(rel, tup);

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
		MetaTrackUpdObject(NamespaceRelationId,
						   nsoid,
						   GetUserId(),
						   "ALTER", "RENAME"
				);

	heap_close(rel, NoLock);
	heap_freetuple(tup);

}

void
AlterSchemaOwner_oid(Oid oid, Oid newOwnerId)
{
	HeapTuple	tup;
	Relation	rel;

	rel = heap_open(NamespaceRelationId, RowExclusiveLock);

	tup = SearchSysCache(NAMESPACEOID,
						 ObjectIdGetDatum(oid),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for schema %u", oid);

	AlterSchemaOwner_internal(tup, rel, newOwnerId);

	ReleaseSysCache(tup);

	heap_close(rel, RowExclusiveLock);
}


/*
 * Change schema owner
 */
void
AlterSchemaOwner(const char *name, Oid newOwnerId)
{
	HeapTuple	tup;
	Relation	rel;

	rel = heap_open(NamespaceRelationId, RowExclusiveLock);

	tup = SearchSysCache(NAMESPACENAME,
						 CStringGetDatum(name),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_SCHEMA),
				 errmsg("schema \"%s\" does not exist", name)));

	if (!allowSystemTableModsDDL && IsReservedName(name))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to ALTER SCHEMA \"%s\"", name),
				 errdetail("Schema %s is reserved for system use.", name)));
	}

	AlterSchemaOwner_internal(tup, rel, newOwnerId);

	ReleaseSysCache(tup);

	heap_close(rel, RowExclusiveLock);
}

static void
AlterSchemaOwner_internal(HeapTuple tup, Relation rel, Oid newOwnerId)
{
	Form_pg_namespace nspForm;

	Assert(RelationGetRelid(rel) == NamespaceRelationId);

	nspForm = (Form_pg_namespace) GETSTRUCT(tup);

	/*
	 * If the new owner is the same as the existing owner, consider the
	 * command to have succeeded.  This is for dump restoration purposes.
	 */
	if (nspForm->nspowner != newOwnerId)
	{
		Datum		repl_val[Natts_pg_namespace];
		bool		repl_null[Natts_pg_namespace];
		bool		repl_repl[Natts_pg_namespace];
		Acl		   *newAcl;
		Datum		aclDatum;
		bool		isNull;
		HeapTuple	newtuple;
		AclResult	aclresult;
		Oid			nsoid;

		/* Otherwise, must be owner of the existing object */
		nsoid = HeapTupleGetOid(tup);
		if (!pg_namespace_ownercheck(nsoid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_NAMESPACE,
						   NameStr(nspForm->nspname));

		/* Must be able to become new owner */
		check_is_member_of_role(GetUserId(), newOwnerId);

		/*
		 * must have create-schema rights
		 *
		 * NOTE: This is different from other alter-owner checks in that the
		 * current user is checked for create privileges instead of the
		 * destination owner.  This is consistent with the CREATE case for
		 * schemas.  Because superusers will always have this right, we need
		 * no special case for them.
		 */
		aclresult = pg_database_aclcheck(MyDatabaseId, GetUserId(),
										 ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_DATABASE,
						   get_database_name(MyDatabaseId));

		memset(repl_null, false, sizeof(repl_null));
		memset(repl_repl, false, sizeof(repl_repl));

		repl_repl[Anum_pg_namespace_nspowner - 1] = true;
		repl_val[Anum_pg_namespace_nspowner - 1] = ObjectIdGetDatum(newOwnerId);

		/*
		 * Determine the modified ACL for the new owner.  This is only
		 * necessary when the ACL is non-null.
		 */
		aclDatum = SysCacheGetAttr(NAMESPACENAME, tup,
								   Anum_pg_namespace_nspacl,
								   &isNull);
		if (!isNull)
		{
			newAcl = aclnewowner(DatumGetAclP(aclDatum),
								 nspForm->nspowner, newOwnerId);
			repl_repl[Anum_pg_namespace_nspacl - 1] = true;
			repl_val[Anum_pg_namespace_nspacl - 1] = PointerGetDatum(newAcl);
		}

		newtuple = heap_modify_tuple(tup, RelationGetDescr(rel), repl_val, repl_null, repl_repl);

		simple_heap_update(rel, &newtuple->t_self, newtuple);
		CatalogUpdateIndexes(rel, newtuple);

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
			MetaTrackUpdObject(NamespaceRelationId,
							   nsoid,
							   GetUserId(),
							   "ALTER", "OWNER"
					);

		heap_freetuple(newtuple);

		/* Update owner dependency reference */
		changeDependencyOnOwner(NamespaceRelationId, HeapTupleGetOid(tup),
								newOwnerId);
	}

}
