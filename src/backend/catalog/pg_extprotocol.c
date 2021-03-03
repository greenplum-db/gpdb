/*-------------------------------------------------------------------------
 *
 * pg_extprotocol.c
 *	  routines to support manipulation of the pg_extprotocol relation
 *
 * Portions Copyright (c) 2011, Greenplum/EMC
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *		src/backend/catalog/pg_extprotocol.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/oid_dispatch.h"
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
 * ExtProtocolCreate
 */
Oid
ExtProtocolCreate(const char *protocolName,
				  List *readfuncName,
				  List *writefuncName,
				  List *validatorfuncName,
				  bool trusted)
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
	Oid			ownerId = GetUserId();
	ScanKeyData skey;
	SysScanDesc scan;
	Oid			protOid;

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

	rel = table_open(ExtprotocolRelationId, RowExclusiveLock);

	/* make sure there is no existing protocol of same name */
	ScanKeyInit(&skey,
				Anum_pg_extprotocol_ptcname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(protocolName));
	scan = systable_beginscan(rel, ExtprotocolPtcnameIndexId, true,
							  NULL, 1, &skey);
	tup = systable_getnext(scan);
	if (HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("protocol \"%s\" already exists",
						protocolName)));
	systable_endscan(scan);

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

	protOid = GetNewOidForExtprotocol(rel, ExtprotocolOidIndexId,
									  Anum_pg_extprotocol_oid, NameStr(prtname));
	values[Anum_pg_extprotocol_oid - 1] = protOid;
	
	tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);

	/* insert a new tuple */
	CatalogTupleInsert(rel, tup);

	table_close(rel, RowExclusiveLock);

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
	/* dependency on extension */
	recordDependencyOnCurrentExtension(&myself, false);

	return protOid;
}

/*
 * ValidateProtocolFunction -- common code for finding readfn, writefn or validatorfn
 */
static Oid
ValidateProtocolFunction(List *fnName, ExtPtcFuncType fntype)
{
	Oid			fnOid;
	bool		retset;
	Oid		   *true_oid_array;
	Oid			actual_rettype;
	Oid			desired_rettype;
	FuncDetailCode fdresult;
	AclResult	aclresult;
	Oid			inputTypes[1] = {InvalidOid}; /* dummy */
	int			nargs = 0; /* true for all 3 function types at the moment */
	int			nvargs;
	Oid			vatype;

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
	fdresult = func_get_detail(fnName, NIL, NIL,
							   nargs, inputTypes, false, false,
							   &fnOid, &actual_rettype, &retset,
							   &nvargs, &vatype, &true_oid_array, NULL);

	/* only valid case is a normal function not returning a set */
	if (fdresult != FUNCDETAIL_NORMAL || !OidIsValid(fnOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function %s does not exist",
						func_signature_string(fnName, nargs, NIL, inputTypes))));

	if (OidIsValid(vatype))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("Invalid protocol function"),
				 errdetail("Protocol functions cannot be variadic.")));

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
						func_signature_string(fnName, nargs, NIL, inputTypes),
						(fntype == EXTPTC_FUNC_VALIDATOR ? "void" : "an integer"))));

	if (func_volatile(fnOid) == PROVOLATILE_IMMUTABLE)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("%s protocol function %s is declared IMMUTABLE",
						func_type_to_name(fntype),
						func_signature_string(fnName, nargs, NIL, inputTypes)),
				 errhint("PROTOCOL functions must be declared STABLE or VOLATILE")));


	/* Check protocol creator has permission to call the function */
	aclresult = pg_proc_aclcheck(fnOid, GetUserId(), ACL_EXECUTE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(fnOid));

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
	Relation	rel;
	Oid			funcOid = InvalidOid;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tup;
	Form_pg_extprotocol extprot;

	rel = table_open(ExtprotocolRelationId, AccessShareLock);

	/*
	 * Check the pg_extprotocol relation to be certain the protocol
	 * is there.
	 */
	ScanKeyInit(&skey,
				Anum_pg_extprotocol_ptcname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(prot_name));
	scan = systable_beginscan(rel, ExtprotocolPtcnameIndexId, true,
							  NULL, 1, &skey);
	tup = systable_getnext(scan);

	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("protocol \"%s\" does not exist",
						prot_name)));

	extprot = (Form_pg_extprotocol) GETSTRUCT(tup);

	switch (prot_type)
	{
		case EXTPTC_FUNC_READER:
			funcOid = extprot->ptcreadfn;
			break;
		case EXTPTC_FUNC_WRITER:
			funcOid = extprot->ptcwritefn;
			break;
		case EXTPTC_FUNC_VALIDATOR:
			funcOid = extprot->ptcvalidatorfn;
			break;
		default:
			elog(ERROR, "internal error in pg_extprotocol:func_type_to_attnum");
			break;
	}
	systable_endscan(scan);
	table_close(rel, AccessShareLock);

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
get_extprotocol_oid(const char *prot_name, bool missing_ok)
{
	Oid			protOid = InvalidOid;
	Relation	rel;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tup;

	rel = table_open(ExtprotocolRelationId, AccessShareLock);

	/*
	 * Check the pg_extprotocol relation to be certain the protocol
	 * is there.
	 */
	ScanKeyInit(&skey,
				Anum_pg_extprotocol_ptcname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(prot_name));
	scan = systable_beginscan(rel, ExtprotocolPtcnameIndexId, true,
							  NULL, 1, &skey);
	tup = systable_getnext(scan);

	if (HeapTupleIsValid(tup))
	{
		Form_pg_extprotocol extprot = (Form_pg_extprotocol) GETSTRUCT(tup);

		protOid = extprot->oid;
	}
	else if (!missing_ok)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("protocol \"%s\" does not exist",
						prot_name)));

	systable_endscan(scan);
	table_close(rel, AccessShareLock);

	return protOid;

}

char *
ExtProtocolGetNameByOid(Oid protOid)
{
	char		*ptcnamestr;
	Relation	rel;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tup;
	Form_pg_extprotocol extprot;

	rel = table_open(ExtprotocolRelationId, AccessShareLock);

	/*
	 * Search pg_extprotocol.
	 */
	ScanKeyInit(&skey,
				Anum_pg_extprotocol_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(protOid));
	scan = systable_beginscan(rel, ExtprotocolOidIndexId, true,
							  NULL, 1, &skey);
	tup = systable_getnext(scan);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "protocol %u could not be found", protOid);

	extprot = (Form_pg_extprotocol) GETSTRUCT(tup);

	ptcnamestr = pstrdup(extprot->ptcname.data);

	systable_endscan(scan);
	table_close(rel, AccessShareLock);

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
