/*-------------------------------------------------------------------------
 *
 * pg_extprotocol.c
 *	  routines to support manipulation of the pg_extprotocol relation
 *
 * Portions Copyright (c) 2011, Greenplum/EMC
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static Oid ValidateProtocolFunction(List *fnName, ExtPtcFuncType fntype);
static char *func_type_to_name(ExtPtcFuncType ftype);

/*
 * ExtProtocolCreateWithOid
 */
Oid
ExtProtocolCreateWithOid(const char		*protocolName,
					     List			*readfuncName,
					     List			*writefuncName,
					     List			*validatorfuncName,					   
					     Oid			 protOid,
					     bool			 trusted)
{
	Relation	rel;
	HeapTuple	tup;
	bool		nulls[Natts_pg_extprotocol];
	Datum		values[Natts_pg_extprotocol];
	Oid			readfn = InvalidOid;
	Oid			writefn = InvalidOid;
	Oid			validatorfn = InvalidOid;
	NameData	prtname;
	int			i;
	ObjectAddress myself,
				referenced;
	Oid 		ownerId = GetUserId();
	cqContext	cqc;
	cqContext	cqc2;
	cqContext  *pcqCtx;

	/* sanity checks (caller should have caught these) */
	if (!protocolName)
		elog(ERROR, "no protocol name supplied");

	if (!readfuncName && !writefuncName)
		elog(ERROR, "protocol must have at least one of readfunc or writefunc");

	/*
	 * Until we add system protocols to pg_extprotocol, make sure no
	 * protocols with the same name are created.
	 */
	if (strcasecmp(protocolName, "file") == 0 ||
		strcasecmp(protocolName, "http") == 0 ||
		strcasecmp(protocolName, "gpfdist") == 0 ||
		strcasecmp(protocolName, "gpfdists") == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("protocol \"%s\" already exists",
						 protocolName),
				 errhint("pick a different protocol name")));
	}

	rel = heap_open(ExtprotocolRelationId, RowExclusiveLock);

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), rel),
			cql("INSERT INTO pg_extprotocol",
				NULL));

	/* make sure there is no existing protocol of same name */
	if (caql_getcount(
				caql_addrel(cqclr(&cqc2), rel),
				cql("SELECT COUNT(*) FROM pg_extprotocol "
					" WHERE ptcname = :1 ",
					CStringGetDatum((char *) protocolName))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("protocol \"%s\" already exists", 
						protocolName)));
	}

	/*
	 * function checks: if supplied, check existence and correct signature in the catalog
	 */
	
	if (readfuncName)
		readfn = ValidateProtocolFunction(readfuncName, EXTPTC_FUNC_READER);

	if (writefuncName)
		writefn = ValidateProtocolFunction(writefuncName, EXTPTC_FUNC_WRITER);				

	if (validatorfuncName)
		validatorfn = ValidateProtocolFunction(validatorfuncName, EXTPTC_FUNC_VALIDATOR);

	/*
	 * Everything looks okay.  Try to create the pg_extprotocol entry for the
	 * protocol.  (This could fail if there's already a conflicting entry.)
	 */

	/* initialize nulls and values */
	for (i = 0; i < Natts_pg_extprotocol; i++)
	{
		nulls[i] = false;
		values[i] = (Datum) 0;
	}
	namestrcpy(&prtname, protocolName);
	values[Anum_pg_extprotocol_ptcname - 1] = NameGetDatum(&prtname);
	values[Anum_pg_extprotocol_ptcreadfn - 1] = ObjectIdGetDatum(readfn);
	values[Anum_pg_extprotocol_ptcwritefn - 1] = ObjectIdGetDatum(writefn);
	values[Anum_pg_extprotocol_ptcvalidatorfn - 1] = ObjectIdGetDatum(validatorfn);
	values[Anum_pg_extprotocol_ptcowner - 1] = ObjectIdGetDatum(ownerId);
	values[Anum_pg_extprotocol_ptctrusted - 1] = BoolGetDatum(trusted);
	nulls[Anum_pg_extprotocol_ptcacl - 1] = true;

	tup = caql_form_tuple(pcqCtx, values, nulls);

	if (protOid != (Oid) 0)
		HeapTupleSetOid(tup, protOid);

	/* insert a new tuple */
	protOid = caql_insert(pcqCtx, tup); /* implicit update of index as well */

	caql_endscan(pcqCtx);
	heap_close(rel, RowExclusiveLock);

	/*
	 * Create dependencies for the protocol
	 */
	myself.classId = ExtprotocolRelationId;
	myself.objectId = protOid;
	myself.objectSubId = 0;

	/* Depends on read function, if any */
	if (OidIsValid(readfn))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = readfn;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* Depends on write function, if any */
	if (OidIsValid(writefn))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = writefn;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* dependency on owner */
	recordDependencyOnOwner(ExtprotocolRelationId, protOid, GetUserId());

	return protOid;
}

void
ExtProtocolDeleteByOid(Oid	protOid)
{		
	Relation	rel;
	cqContext	cqc;

	/*
	 * Search pg_extprotocol.  We use a heapscan here even though there is an
	 * index on oid, on the theory that pg_extprotocol will usually have just a
	 * few entries and so an indexed lookup is a waste of effort.
	 */
	rel = heap_open(ExtprotocolRelationId, RowExclusiveLock);

	if (0 == caql_getcount(
				caql_addrel(cqclr(&cqc), rel),
				cql("DELETE FROM pg_extprotocol "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(protOid))))
	{
		elog(ERROR, "protocol %u could not be found", protOid);
	}
	heap_close(rel, NoLock);
}

/*
 * ValidateProtocolFunction -- common code for finding readfn, writefn or validatorfn
 */
static Oid
ValidateProtocolFunction(List *fnName, ExtPtcFuncType fntype)
{
	Oid			fnOid;
	bool		retset;
	bool        retstrict;
	bool        retordered;
	Oid		   *true_oid_array;
	Oid 	    actual_rettype;
	Oid			desired_rettype;
	FuncDetailCode fdresult;
	AclResult	aclresult;
	Oid 		inputTypes[1] = {InvalidOid}; /* dummy */
	int			nargs = 0; /* true for all 3 function types at the moment */
	int			nvargs;
	
	if (fntype == EXTPTC_FUNC_VALIDATOR)
		desired_rettype = VOIDOID;
	else
		desired_rettype = INT4OID;

	/*
	 * func_get_detail looks up the function in the catalogs, does
	 * disambiguation for polymorphic functions, handles inheritance, and
	 * returns the funcid and type and set or singleton status of the
	 * function's return value.  it also returns the true argument types to
	 * the function.
	 */
	fdresult = func_get_detail(fnName, NIL, nargs, inputTypes, false, false,
							   &fnOid, &actual_rettype, &retset, &retstrict,
							   &retordered, &nvargs, &true_oid_array, NULL);

	/* only valid case is a normal function not returning a set */
	if (fdresult != FUNCDETAIL_NORMAL || !OidIsValid(fnOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function %s does not exist",
						func_signature_string(fnName, nargs, inputTypes))));
	
	if (retset)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("Invalid protocol function"),
				 errdetail("Protocol functions cannot return sets.")));		

	if (actual_rettype != desired_rettype)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("%s protocol function %s must return %s",
						func_type_to_name(fntype),
						func_signature_string(fnName, nargs, inputTypes),
						(fntype == EXTPTC_FUNC_VALIDATOR ? "void" : "an integer"))));
	
	if (func_volatile(fnOid) == PROVOLATILE_IMMUTABLE)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("%s protocol function %s is declared IMMUTABLE",
						func_type_to_name(fntype),
						func_signature_string(fnName, nargs, inputTypes)),
				 errhint("PROTOCOL functions must be declared STABLE or VOLATILE")));

	
	/* Check protocol creator has permission to call the function */
	aclresult = pg_proc_aclcheck(fnOid, GetUserId(), ACL_EXECUTE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_PROC, get_func_name(fnOid));

	return fnOid;
}


/*
 * Finds an external protocol by passed in protocol name.
 * Errors if no such protocol exist, or if no function to
 * execute this protocol exists (for read or write separately).
 * 
 * Returns the protocol function to use.
 */
Oid
LookupExtProtocolFunction(const char *prot_name, 
						  ExtPtcFuncType prot_type,
						  bool error)
{
	bool		isNull;
	Oid			funcOid = InvalidOid;
	int			fetchCount = 0;
	
	/*
	 * Check the pg_extprotocol relation to be certain the protocol
	 * is there.
	 */
	
	switch (prot_type)
	{
		case EXTPTC_FUNC_READER:
			funcOid = caql_getoid_plus(
					NULL,
					&fetchCount,
					&isNull,
				cql("SELECT ptcreadfn FROM pg_extprotocol "
					" WHERE ptcname = :1 ",
					CStringGetDatum(prot_name)));
			break;
		case EXTPTC_FUNC_WRITER:
			funcOid = caql_getoid_plus(
					NULL,
					&fetchCount,
					&isNull,
				cql("SELECT ptcwritefn FROM pg_extprotocol "
					" WHERE ptcname = :1 ",
					CStringGetDatum(prot_name)));
			break;
		case EXTPTC_FUNC_VALIDATOR:
			funcOid = caql_getoid_plus(
					NULL,
					&fetchCount,
					&isNull,
				cql("SELECT ptcvalidatorfn FROM pg_extprotocol "
					" WHERE ptcname = :1 ",
					CStringGetDatum(prot_name)));
			break;
		default:
			elog(ERROR, "internal error in pg_extprotocol:func_type_to_attnum");
			break;
	}

	if (!fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("protocol \"%s\" does not exist",
						prot_name)));

	/* cat attr is defined as NOT NULL */
	Assert(!isNull);

	if (!OidIsValid(funcOid) && error)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("protocol '%s' has no %s function defined",
						 prot_name, func_type_to_name(prot_type))));

	return funcOid;

}

/*
 * Same as LookupExtProtocolFunction but returns the actual
 * protocol Oid.
 */
Oid
LookupExtProtocolOid(const char *prot_name, bool missing_ok)
{
	int			fetchCount;
	Oid			protOid = InvalidOid;
	
	/*
	 * Check the pg_extprotocol relation to be certain the protocol
	 * is there.
	 */
	protOid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oid FROM pg_extprotocol "
				" WHERE ptcname = :1 ",
				CStringGetDatum(prot_name)));

	if (0 == fetchCount)
	{
		if(!missing_ok)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("protocol \"%s\" does not exist",
							prot_name)));
	}
	
	return protOid;

}

char *
ExtProtocolGetNameByOid(Oid	protOid)
{		
	char		*ptcnamestr;
	bool		isNull;
	int			fetchCount;
	
	/*
	 * Search pg_extprotocol.  We use a heapscan here even though there is an
	 * index on oid, on the theory that pg_extprotocol will usually have just a
	 * few entries and so an indexed lookup is a waste of effort.
	 */

	ptcnamestr = caql_getcstring_plus(
			NULL,
			&fetchCount,
			&isNull,
			cql("SELECT ptcname FROM pg_extprotocol "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(protOid)));

	/* We assume that there can be at most one matching tuple */
	if (!fetchCount)
	{
		elog(ERROR, "protocol %u could not be found", protOid);
	}

	if(isNull)
		ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_OBJECT),
			 errmsg("internal error: protocol '%u' has no name defined",
					 protOid)));

	return ptcnamestr;
}

static char *func_type_to_name(ExtPtcFuncType ftype)
{
	switch (ftype)
	{
		case EXTPTC_FUNC_READER:
			return "read";
		case EXTPTC_FUNC_WRITER:
			return "write";
		case EXTPTC_FUNC_VALIDATOR:
			return "validator";
		default:
			elog(ERROR, "internal error in pg_extprotocol:func_type_to_name");
			return "undefined";
	}
}

