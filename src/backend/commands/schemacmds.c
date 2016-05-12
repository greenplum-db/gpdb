/*------------------------------------------------------------------------- 
 *
 * schemacmds.c
 *	  schema creation/manipulation commands
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/schemacmds.c,v 1.43 2007/02/01 19:10:26 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_namespace.h"
#include "commands/dbcommands.h"
#include "commands/schemacmds.h"
#include "miscadmin.h"
#include "parser/analyze.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbvars.h"

static void AlterSchemaOwner_internal(cqContext  *pcqCtx, 
									  HeapTuple tup, Relation rel, Oid newOwnerId);
static void RemoveSchema_internal(const char *namespaceName, DropBehavior behavior,
								  bool missing_ok, bool is_internal);


/*
 * CREATE SCHEMA
 */
void
CreateSchemaCommand(CreateSchemaStmt *stmt, const char *queryString)
{
	const char *schemaName = stmt->schemaname;
	const char *authId = stmt->authid;
	const bool  istemp = stmt->istemp;
	Oid			namespaceId = 0;
	List	   *parsetree_list;
	ListCell   *parsetree_item;
	Oid			owner_uid;
	Oid			saved_uid;
	int			save_sec_context;
	AclResult	aclresult;
	bool		shouldDispatch = (Gp_role == GP_ROLE_DISPATCH && 
								  !IsBootstrapProcessingMode());

	GetUserIdAndSecContext(&saved_uid, &save_sec_context);

	/*
	 * Who is supposed to own the new schema?
	 */
	if (authId)
		owner_uid = get_roleid_checked(authId);
	else
		owner_uid = saved_uid;

	/* 
	 * If we are creating a temporary schema then we can skip a 
	 * bunch of checks that we would otherwise make.
	 */
	if (istemp)
	{
		/*
		 * CDB: Delete old temp schema.
		 *
		 * Remove any vestigages of old temporary schema, if any.  This can
		 * happen when an old session crashes and doesn't run normal session
		 * shutdown.  
		 *
		 * In postgres they try to reuse existing schemas in this case, 
		 * however that does not work well for us since the schemas may exist 
		 * on a segment by segment basis and we want to keep them syncronized
		 * on oid.  The best way of dealing with this is to just delete the
		 * old schemas.
		 */
		RemoveSchema_internal(schemaName, DROP_CASCADE, true, true);
	}
	else
	{
		/*
		 * To create a schema, must have schema-create privilege on the current
		 * database and must be able to become the target role (this does not
		 * imply that the target role itself must have create-schema privilege).
		 * The latter provision guards against "giveaway" attacks. Note that a
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
	}

	/*
	 * If the requested authorization is different from the current user,
	 * temporarily set the current user so that the object(s) will be created
	 * with the correct ownership.
	 *
	 * (The setting will be restored at the end of this routine, or in case
	 * of error, transaction abort will clean things up.)
	 */
	if (saved_uid != owner_uid)
		SetUserIdAndSecContext(owner_uid,
							   save_sec_context | SECURITY_LOCAL_USERID_CHANGE);

	/* Create the schema's namespace */
	if (shouldDispatch || Gp_role != GP_ROLE_EXECUTE)
	{
		namespaceId = NamespaceCreate(schemaName, owner_uid, 0);

		if (shouldDispatch)
		{
            elog(DEBUG5, "shouldDispatch = true, namespaceOid = %d", namespaceId);

            Assert(stmt->schemaOid == 0);
            stmt->schemaOid = namespaceId;

            /*
             * Dispatch the command to all primary and mirror segment dbs.
             * Starts a global transaction and reconfigures cluster if needed.
             * Waits for QEs to finish.  Exits via ereport(ERROR,...) if error.
             */
            CdbDispatchUtilityStatement((Node *)stmt, "CreateSchemaCommand");
		}

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH && !istemp)
			MetaTrackAddObject(NamespaceRelationId,
							   namespaceId,
							   saved_uid,
							   "CREATE", "SCHEMA"
					);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		namespaceId = NamespaceCreate(schemaName, owner_uid, stmt->schemaOid);
	}

	/* Advance cmd counter to make the namespace visible */
	CommandCounterIncrement();

	/* If this is the temporary namespace we must mark it specially */
	if (istemp)
		SetTempNamespace(namespaceId);

	/*
	 * Temporarily make the new namespace be the front of the search path, as
	 * well as the default creation target namespace.  This will be undone at
	 * the end of this routine, or upon error.
	 */
	PushSpecialNamespace(namespaceId);

	/*
	 * Examine the list of commands embedded in the CREATE SCHEMA command, and
	 * reorganize them into a sequentially executable order with no forward
	 * references.	Note that the result is still a list of raw parsetrees in
	 * need of parse analysis --- we cannot, in general, run analyze.c on one
	 * statement until we have actually executed the prior ones.
	 */
	parsetree_list = analyzeCreateSchemaStmt(stmt);

	/*
	 * Analyze and execute each command contained in the CREATE SCHEMA
	 */
	foreach(parsetree_item, parsetree_list)
	{
		Node	   *parsetree = (Node *) lfirst(parsetree_item);
		List	   *querytree_list;
		ListCell   *querytree_item;

		querytree_list = parse_analyze(parsetree, NULL, NULL, 0);

		foreach(querytree_item, querytree_list)
		{
			Query	   *querytree = (Query *) lfirst(querytree_item);

			/* schemas should contain only utility stmts */
			Assert(querytree->commandType == CMD_UTILITY);
			/* do this step */
			ProcessUtility(querytree->utilityStmt, 
						   queryString,
						   NULL, 
						   false, /* not top level */
						   None_Receiver, 
						   NULL);
			/* make sure later steps can see the object created here */
			CommandCounterIncrement();
		}
	}

	/* Reset search path to normal state */
	PopSpecialNamespace(namespaceId);

	/* Reset current user and security context */
	SetUserIdAndSecContext(saved_uid, save_sec_context);
}


/*
 *	RemoveSchema
 *		Removes a schema.
 */
void
RemoveSchema(List *names, DropBehavior behavior, bool missing_ok)
{
	char	   *namespaceName;

	if (list_length(names) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("schema name cannot be qualified")));
	namespaceName = strVal(linitial(names));

	RemoveSchema_internal(namespaceName, behavior, missing_ok, false);
}

static void
RemoveSchema_internal(const char *namespaceName, DropBehavior behavior,
					  bool missing_ok, bool is_internal)
{
	Oid			namespaceId;
	ObjectAddress object;

	namespaceId = caql_getoid(
			NULL,
			cql("SELECT oid FROM pg_namespace "
				" WHERE nspname = :1 ",
				CStringGetDatum(namespaceName)));
	if (!OidIsValid(namespaceId))
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_SCHEMA),
					 errmsg("schema \"%s\" does not exist", namespaceName)));
		}
		if (!is_internal && Gp_role != GP_ROLE_EXECUTE)
		{
			ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_SCHEMA),
					 errmsg("schema \"%s\" does not exist, skipping",
							namespaceName)));
		}

		return;
	}

	/* Permission check */
	if (!is_internal && !pg_namespace_ownercheck(namespaceId, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_NAMESPACE,
					   namespaceName);

	/* Additional check to protect reserved schema names, exclude temp schema */
	if (!is_internal && !allowSystemTableModsDDL &&
		IsReservedName(namespaceName) &&
        (strlen(namespaceName)>=7 && strncmp(namespaceName, "pg_temp", 7)!=0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("cannot drop schema %s because it is required by the database system",
						namespaceName)));
	}

	/*
	 * Do the deletion.  Objects contained in the schema are removed by means
	 * of their dependency links to the schema.
	 */
	object.classId = NamespaceRelationId;
	object.objectId = namespaceId;
	object.objectSubId = 0;

	performDeletion(&object, behavior);

	/* MPP-6929: metadata tracking */
	if (!is_internal && Gp_role == GP_ROLE_DISPATCH)
		MetaTrackDropObject(NamespaceRelationId, namespaceId);
}


/*
 * Guts of schema deletion.
 */
void
RemoveSchemaById(Oid schemaOid)
{
	if (0 ==
		caql_getcount(
				NULL,
				cql("DELETE FROM pg_namespace "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(schemaOid)))) /* should not happen */
	{
		elog(ERROR, "cache lookup failed for namespace %u", schemaOid);
	}
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
	cqContext	cqc2;
	cqContext	cqc;
	cqContext  *pcqCtx;

	rel = heap_open(NamespaceRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), rel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_namespace "
				" WHERE nspname = :1 "
				" FOR UPDATE ",
				CStringGetDatum((char *) oldname)));

	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_SCHEMA),
				 errmsg("schema \"%s\" does not exist", oldname)));

	/* make sure the new name doesn't exist */
	if (caql_getcount(
				caql_addrel(cqclr(&cqc2), rel),
				cql("SELECT * FROM pg_namespace "
					" WHERE nspname = :1 ",
					CStringGetDatum((char *) newname))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_SCHEMA),
				 errmsg("schema \"%s\" already exists", newname)));
	}

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
	caql_update_current(pcqCtx, tup); /* implicit update of index as well */
	
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
	cqContext	cqc;
	cqContext  *pcqCtx;

	rel = heap_open(NamespaceRelationId, RowExclusiveLock);

	pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), rel),
				cql("SELECT * FROM pg_namespace "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(oid)));

	tup = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for schema %u", oid);

	AlterSchemaOwner_internal(pcqCtx, tup, rel, newOwnerId);

	caql_endscan(pcqCtx);

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
	cqContext	cqc;
	cqContext  *pcqCtx;

	rel = heap_open(NamespaceRelationId, RowExclusiveLock);

	pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), rel),
				cql("SELECT * FROM pg_namespace "
					" WHERE nspname = :1 "
					" FOR UPDATE ",
					CStringGetDatum((char *) name)));

	tup = caql_getnext(pcqCtx);

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

	AlterSchemaOwner_internal(pcqCtx, tup, rel, newOwnerId);

	caql_endscan(pcqCtx);

	heap_close(rel, RowExclusiveLock);
}

static void
AlterSchemaOwner_internal(cqContext  *pcqCtx, 
						  HeapTuple tup, Relation rel, Oid newOwnerId)
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
		aclDatum = caql_getattr(pcqCtx,
								Anum_pg_namespace_nspacl,
								&isNull);
		if (!isNull)
		{
			newAcl = aclnewowner(DatumGetAclP(aclDatum),
								 nspForm->nspowner, newOwnerId);
			repl_repl[Anum_pg_namespace_nspacl - 1] = true;
			repl_val[Anum_pg_namespace_nspacl - 1] = PointerGetDatum(newAcl);
		}

		newtuple = caql_modify_current(pcqCtx, repl_val, repl_null, repl_repl);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

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
