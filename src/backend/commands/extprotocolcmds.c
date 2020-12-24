/*-------------------------------------------------------------------------
 *
 * extprotocolcmds.c
 *
 *	  Routines for external protocol-manipulation commands
 *
 * Portions Copyright (c) 2011, Greenplum/EMC.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to create an external protocol")));

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
 * Drop PROTOCOL by OID. This is the guts of deletion.
 * This is called to clean up dependencies.
 */
void
RemoveExtProtocolById(Oid protOid)
{
	Relation	rel;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tup;
	bool		found = false;

	/*
	 * Search pg_extprotocol.
	 */
	rel = table_open(ExtprotocolRelationId, RowExclusiveLock);

	ScanKeyInit(&skey,
				Anum_pg_extprotocol_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(protOid));
	scan = systable_beginscan(rel, ExtprotocolOidIndexId, true,
							  NULL, 1, &skey);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		CatalogTupleDelete(rel, &tup->t_self);
		found = true;
	}
	systable_endscan(scan);

	if (!found)
		elog(ERROR, "protocol %u could not be found", protOid);

	table_close(rel, NoLock);
}
