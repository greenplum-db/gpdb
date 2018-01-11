/*-------------------------------------------------------------------------
 *
 * extprotocolcmds.c
 *
 *	  Routines for external protocol-manipulation commands
 *
 * Portions Copyright (c) 2011, Greenplum/EMC.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/commands/extprotocolcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/extprotocolcmds.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/fmgroids.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"

/*
 *	DefineExtprotocol
 */
void
DefineExtProtocol(List *name, List *parameters, bool trusted)
{
	char	   *protName;
	List	   *readfuncName = NIL;
	List	   *writefuncName = NIL;
	List	   *validatorfuncName = NIL;
	ListCell   *pl;
	Oid			protOid;

	protName = strVal(linitial(name));

	foreach(pl, parameters)
	{
		DefElem    *defel = (DefElem *) lfirst(pl);

		if (pg_strcasecmp(defel->defname, "readfunc") == 0)
			readfuncName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "writefunc") == 0)
			writefuncName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "validatorfunc") == 0)
			validatorfuncName = defGetQualifiedName(defel);
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("protocol attribute \"%s\" not recognized",
							defel->defname)));
	}

	/*
	 * make sure we have our required definitions
	 */
	if (readfuncName == NULL && writefuncName == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("protocol must be specify at least a readfunc or a writefunc")));


	/*
	 * Most of the argument-checking is done inside of ExtProtocolCreate
	 */
	protOid = ExtProtocolCreate(protName,			/* protocol name */
								readfuncName,		/* read function name */
								writefuncName,		/* write function name */
								validatorfuncName, 	/* validator function name */
								trusted);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		DefineStmt * stmt = makeNode(DefineStmt);
		stmt->kind = OBJECT_EXTPROTOCOL;
		stmt->oldstyle = false;  
		stmt->defnames = name;
		stmt->args = NIL;
		stmt->definition = parameters;
		stmt->trusted = trusted;
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}
}


/*
 * RemoveExtProtocols
 *		Implements DROP EXTERNAL PROTOCOL
 */
void
RemoveExtProtocols(DropStmt *drop)
{
	ObjectAddresses *objects;
	ListCell		*cell;

	/*
	 * First we identify all the objects, then we delete them in a single
	 * performMultipleDeletions() call.  This is to avoid unwanted
	 * DROP RESTRICT errors if one of the objects depends on another.
	 */
	objects = new_object_addresses();

	foreach(cell, drop->objects)
	{
		List		*names = (List *) lfirst(cell);
		char	   *protocolName;
		Oid			protocolOid;
		ObjectAddress object;

		if (list_length(names) != 1)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("protocol name may not be qualified")));
		protocolName = strVal(linitial(names));

		protocolOid = LookupExtProtocolOid(protocolName, drop->missing_ok);

		if (!OidIsValid(protocolOid))
		{
			if (!drop->missing_ok)
			{
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("protocol \"%s\" does not exist",
								protocolName)));
			}
			else
			{
				if (Gp_role != GP_ROLE_EXECUTE)
					ereport(NOTICE,
							(errcode(ERRCODE_UNDEFINED_OBJECT),
							 errmsg("protocol \"%s\" does not exist, skipping",
									protocolName)));
			}
			continue;
		}

		/* Permission check: must own protocol */
		if (!pg_extprotocol_ownercheck(protocolOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_EXTPROTOCOL, protocolName);

		object.classId = ExtprotocolRelationId;
		object.objectId = protocolOid;
		object.objectSubId = 0;

		add_exact_object_address(&object, objects);
	}

	performMultipleDeletions(objects, drop->behavior);

	free_object_addresses(objects);
}

/*
 * Drop PROTOCOL by OID. This is the guts of deletion.
 * This is called to clean up dependencies.
 */
void
RemoveExtProtocolById(Oid protOid)
{
	ExtProtocolDeleteByOid(protOid);
}

/*
 * Change external protocol owner
 */
void
AlterExtProtocolOwner(const char *name, Oid newOwnerId)
{
	HeapTuple	tup;
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	Oid			ptcId;
	Oid			ownerId;
	AclResult	aclresult;
	bool		isNull;
	bool		isTrusted;
	Datum		ownerDatum;
	Datum		trustedDatum;

	/*
	 * Check the pg_extprotocol relation to be certain the protocol
	 * is there. 
	 */
	rel = heap_open(ExtprotocolRelationId, RowExclusiveLock);
	ScanKeyInit(&scankey,
				Anum_pg_extprotocol_ptcname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(name));
	scan = systable_beginscan(rel, ExtprotocolPtcnameIndexId, true,
							  SnapshotNow, 1, &scankey);
	tup = systable_getnext(scan);
	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("protocol \"%s\" does not exist",
						name)));

	ptcId = HeapTupleGetOid(tup);

	ownerDatum = heap_getattr(tup,
							  Anum_pg_extprotocol_ptcowner,
							  RelationGetDescr(rel),
							  &isNull);
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("internal error: protocol \"%s\"  has no owner defined",
						 name)));

	ownerId = DatumGetObjectId(ownerDatum);

	trustedDatum = heap_getattr(tup,
								Anum_pg_extprotocol_ptctrusted,
								RelationGetDescr(rel),
								&isNull);
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("internal error: protocol \"%s\" has no trust attribute defined",
						 name)));

	isTrusted = DatumGetBool(trustedDatum);
	
	if (ownerId != newOwnerId)
	{
		Acl		   *newAcl;
		Datum		values[Natts_pg_extprotocol];
		bool		nulls[Natts_pg_extprotocol];
		bool		replaces[Natts_pg_extprotocol];
		HeapTuple	newtuple;
		Datum		aclDatum;

		/* Superusers can always do it */
		if (!superuser())
		{
			/* Must be owner */
			if (!pg_extprotocol_ownercheck(ptcId, GetUserId()))
				aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_EXTPROTOCOL,
							   name);

			/* Must be able to become new owner */
			check_is_member_of_role(GetUserId(), newOwnerId);

			/* New owner must have USAGE privilege on protocol */
			aclresult = pg_extprotocol_aclcheck(ptcId, newOwnerId, ACL_USAGE);

			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_EXTPROTOCOL, name);
		}

		/* MPP-14592: untrusted? don't allow ALTER OWNER to non-super user */
		if(!isTrusted && !superuser_arg(newOwnerId))
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("untrusted protocol \"%s\" can't be owned by non superuser", 
							 name)));

		
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_extprotocol_ptcowner - 1] = true;
		values[Anum_pg_extprotocol_ptcowner - 1] = ObjectIdGetDatum(newOwnerId);

		aclDatum = heap_getattr(tup,
								Anum_pg_extprotocol_ptcacl,
								RelationGetDescr(rel),
								&isNull);
		
		if (!isNull)
		{
			newAcl = aclnewowner(DatumGetAclP(aclDatum),
								 ownerId, newOwnerId);
			replaces[Anum_pg_extprotocol_ptcacl - 1] = true;
			values[Anum_pg_extprotocol_ptcacl - 1] = PointerGetDatum(newAcl);
		}

		newtuple = heap_modify_tuple(tup,
									 RelationGetDescr(rel),
									 values, nulls, replaces);
		simple_heap_update(rel, &newtuple->t_self, newtuple);
		CatalogUpdateIndexes(rel, newtuple);

		heap_freetuple(newtuple);
		
		/* Update owner dependency reference */
		changeDependencyOnOwner(ExtprotocolRelationId, ptcId, newOwnerId);
	}

	systable_endscan(scan);
	heap_close(rel, NoLock);
}

/*
 * Change external protocol owner
 */
void
RenameExtProtocol(const char *oldname, const char *newname)
{
	HeapTuple	tup;
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	Oid			ptcId;
	bool		isNull;
	
	/*
	 * Check the pg_extprotocol relation to be certain the protocol 
	 * is there. 
	 */
	rel = heap_open(ExtprotocolRelationId, RowExclusiveLock);
	ScanKeyInit(&scankey,
				Anum_pg_extprotocol_ptcname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(oldname));
	scan = systable_beginscan(rel, ExtprotocolPtcnameIndexId, true,
							  SnapshotNow, 1, &scankey);
	tup = systable_getnext(scan);
	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("protocol \"%s\" does not exist",
						oldname)));

	ptcId = HeapTupleGetOid(tup);

	(void) heap_getattr(tup,
						Anum_pg_extprotocol_ptcowner,
						RelationGetDescr(rel),
						&isNull);
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("internal error: protocol \"%s\"  has no owner defined",
						 oldname)));	

	if (strcmp(oldname, newname) != 0)
	{
		Datum		values[Natts_pg_extprotocol];
		bool		nulls[Natts_pg_extprotocol];
		bool		replaces[Natts_pg_extprotocol];
		HeapTuple	newtuple;

		/* Superusers can always do it */
		if (!superuser())
		{
			/* Must be owner */
			if (!pg_extprotocol_ownercheck(ptcId, GetUserId()))
				aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_EXTPROTOCOL,
							   oldname);
		}

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_extprotocol_ptcname - 1] = true;
		values[Anum_pg_extprotocol_ptcname - 1] =
			DirectFunctionCall1(namein, CStringGetDatum((char *) newname));
		
		newtuple = heap_modify_tuple(tup,
									 RelationGetDescr(rel),
									 values, nulls, replaces);
		simple_heap_update(rel, &tup->t_self, newtuple);

		/* Update indexes */
		CatalogUpdateIndexes(rel, newtuple);

		heap_freetuple(newtuple);		
	}

	systable_endscan(scan);

	heap_close(rel, NoLock);
}
