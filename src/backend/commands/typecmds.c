/*-------------------------------------------------------------------------
 *
 * typecmds.c
 *	  Routines for SQL commands that manipulate types (and domains).
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/typecmds.c,v 1.100 2007/02/14 01:58:57 tgl Exp $
 *
 * DESCRIPTION
 *	  The "DefineFoo" routines take the parse tree and pick out the
 *	  appropriate arguments/flags, passing the results to the
 *	  corresponding "FooDefine" routines (in src/catalog) that do
 *	  the actual catalog-munging.  These routines also verify permission
 *	  of the user to execute the command.
 *
 * NOTES
 *	  These things must be defined and committed in the following order:
 *		"create function":
 *				input/output, recv/send functions
 *		"create type":
 *				type
 *		"create operator":
 *				operators
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/reloptions.h"
#include "access/xact.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/pg_compression.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_encoding.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/typecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/planmain.h"
#include "optimizer/var.h"
#include "parser/analyze.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp.h"


/* result structure for get_rels_with_domain() */
typedef struct
{
	Relation	rel;			/* opened and locked relation */
	int			natts;			/* number of attributes of interest */
	int		   *atts;			/* attribute numbers */
	/* atts[] is of allocated length RelationGetNumberOfAttributes(rel) */
} RelToCheck;


static Oid	findTypeInputFunction(List *procname, Oid typeOid);
static Oid	findTypeOutputFunction(List *procname, Oid typeOid);
static Oid	findTypeReceiveFunction(List *procname, Oid typeOid);
static Oid	findTypeSendFunction(List *procname, Oid typeOid);
static Oid	findTypeTypmodinFunction(List *procname);
static Oid	findTypeTypmodoutFunction(List *procname);
static Oid	findTypeAnalyzeFunction(List *procname, Oid typeOid);
static List *get_rels_with_domain(Oid domainOid, LOCKMODE lockmode);
static void checkDomainOwner(HeapTuple tup, TypeName *typename);
static char *domainAddConstraint(Oid domainOid, Oid domainNamespace,
					Oid baseTypeOid,
					int typMod, Constraint *constr,
					char *domainName);
static void remove_type_encoding(Oid typid);

/*
 * DefineType
 *		Registers a new type.
 */
void
DefineType(List *names, List *parameters, Oid newOid, Oid shadowOid)
{
	char	   *typeName;
	Oid			typeNamespace;
	AclResult	aclresult;
	int16		internalLength = -1;	/* default: variable-length */
	Oid			elemType = InvalidOid;
	List	   *inputName = NIL;
	List	   *outputName = NIL;
	List	   *receiveName = NIL;
	List	   *sendName = NIL;
	List	   *typmodinName = NIL;
	List	   *typmodoutName = NIL;
	List	   *analyzeName = NIL;
	char	   *defaultValue = NULL;
	bool		byValue = false;
	char		delimiter = DEFAULT_TYPDELIM;
	char		alignment = 'i';	/* default alignment */
	char		storage = 'p';	/* default TOAST storage method */
	char        typtype = TYPTYPE_BASE;
	Oid			inputOid;
	Oid			outputOid;
	Oid			receiveOid = InvalidOid;
	Oid			sendOid = InvalidOid;
	Oid			typmodinOid = InvalidOid;
	Oid			typmodoutOid = InvalidOid;
	Oid			analyzeOid = InvalidOid;
	char	   *shadow_type;
	ListCell   *pl;
	Oid			typoid;
	Oid			resulttype;
	Datum		typoptions = 0;
	List	   *encoding = NIL;

	/* Convert list of names to a name and namespace */
	typeNamespace = QualifiedNameGetCreationNamespace(names, &typeName);

	/* Check we have creation rights in target namespace */
	aclresult = pg_namespace_aclcheck(typeNamespace, GetUserId(), ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
					   get_namespace_name(typeNamespace));

	/*
	 * Type names must be one character shorter than other names, allowing
	 * room to create the corresponding array type name with prepended "_".
	 */
	if (strlen(typeName) > (NAMEDATALEN - 2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("type names must be %d characters or less",
						NAMEDATALEN - 2)));

	/*
	 * Look to see if type already exists (presumably as a shell; if not,
	 * TypeCreate will complain).  If it doesn't, create it as a shell, so
	 * that the OID is known for use in the I/O function definitions.
	 */
	typoid = caql_getoid(
			NULL,
			cql("SELECT oid FROM pg_type "
				" WHERE typname = :1 "
				" AND typnamespace = :2 ",
				CStringGetDatum(typeName),
				ObjectIdGetDatum(typeNamespace)));

	if (!OidIsValid(typoid))
	{
		/*
		 * See if oid is specified for the shell type during upgrade, if so use it
		 * to create shell type.
		 */
		bool createShellOnly = (parameters==NIL);
		typoid = TypeShellMake(typeName, typeNamespace, GetUserId(), newOid);

		/* Make new shell type visible for modification below */
		CommandCounterIncrement();

		/*
		 * If the command was a parameterless CREATE TYPE, we're done ---
		 * creating the shell type was all we're supposed to do.
		 */
		if (createShellOnly)
		{
			/* Must dispatch shell type creation */
			if (Gp_role == GP_ROLE_DISPATCH)
			{
				DefineStmt * stmt = makeNode(DefineStmt);
				stmt->kind = OBJECT_TYPE;
				stmt->oldstyle = false; /*?*/
				stmt->defnames = names;
				stmt->args = NIL;
				stmt->definition = NIL;
				stmt->newOid = typoid;
				stmt->shadowOid = shadowOid;
				CdbDispatchUtilityStatement((Node *) stmt, "DefineType");
			}
			return;
		}
	}
	else
	{
		/* Complain if dummy CREATE TYPE and entry already exists */
		if (parameters == NIL)
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("type \"%s\" already exists", typeName)));
	}

	foreach(pl, parameters)
	{
		DefElem    *defel = (DefElem *) lfirst(pl);

		if (pg_strcasecmp(defel->defname, "internallength") == 0)
			internalLength = defGetTypeLength(defel);
		else if (pg_strcasecmp(defel->defname, "externallength") == 0)
			;					/* ignored -- remove after 7.3 */
		else if (pg_strcasecmp(defel->defname, "input") == 0)
			inputName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "output") == 0)
			outputName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "receive") == 0)
			receiveName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "send") == 0)
			sendName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "typmod_in") == 0)
			typmodinName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "typmod_out") == 0)
			typmodoutName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "analyze") == 0 ||
				 pg_strcasecmp(defel->defname, "analyse") == 0)
			analyzeName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "delimiter") == 0)
		{
			char	   *p = defGetString(defel);

			delimiter = p[0];
		}
		else if (pg_strcasecmp(defel->defname, "element") == 0)
		{
			elemType = typenameTypeId(NULL, defGetTypeName(defel));
			/* disallow arrays of pseudotypes */
			if (get_typtype(elemType) == TYPTYPE_PSEUDO)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("array element type cannot be %s",
								format_type_be(elemType))));
		}
		else if (pg_strcasecmp(defel->defname, "default") == 0)
			defaultValue = defGetString(defel);
		else if (pg_strcasecmp(defel->defname, "passedbyvalue") == 0)
			byValue = defGetBoolean(defel);
		else if (pg_strcasecmp(defel->defname, "alignment") == 0)
		{
			char	   *a = defGetString(defel);

			/*
			 * Note: if argument was an unquoted identifier, parser will have
			 * applied translations to it, so be prepared to recognize
			 * translated type names as well as the nominal form.
			 */
			if (pg_strcasecmp(a, "double") == 0 ||
				pg_strcasecmp(a, "float8") == 0 ||
				pg_strcasecmp(a, "pg_catalog.float8") == 0)
				alignment = 'd';
			else if (pg_strcasecmp(a, "int4") == 0 ||
					 pg_strcasecmp(a, "pg_catalog.int4") == 0)
				alignment = 'i';
			else if (pg_strcasecmp(a, "int2") == 0 ||
					 pg_strcasecmp(a, "pg_catalog.int2") == 0)
				alignment = 's';
			else if (pg_strcasecmp(a, "char") == 0 ||
					 pg_strcasecmp(a, "pg_catalog.bpchar") == 0)
				alignment = 'c';
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("alignment \"%s\" not recognized", a)));
		}
		else if (pg_strcasecmp(defel->defname, "storage") == 0)
		{
			char	   *a = defGetString(defel);

			if (pg_strcasecmp(a, "plain") == 0)
				storage = 'p';
			else if (pg_strcasecmp(a, "external") == 0)
				storage = 'e';
			else if (pg_strcasecmp(a, "extended") == 0)
				storage = 'x';
			else if (pg_strcasecmp(a, "main") == 0)
				storage = 'm';
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("storage \"%s\" not recognized", a)));
		}
		else if (is_storage_encoding_directive(defel->defname))
		{
			encoding = lappend(encoding, defel);
		}
		else
			ereport(WARNING,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("type attribute \"%s\" not recognized",
							defel->defname)));
	}

	if (encoding)
	{
		encoding = transformStorageEncodingClause(encoding);
		typoptions = transformRelOptions((Datum) 0, encoding, true, false);
	}

	/*
	 * make sure we have our required definitions
	 */
	if (inputName == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("type input function must be specified")));
	if (outputName == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("type output function must be specified")));

	if (typmodinName == NIL && typmodoutName != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("type modifier output function is useless without a type modifier input function")));

	/*
	 * Convert I/O proc names to OIDs
	 */
	inputOid = findTypeInputFunction(inputName, typoid);
	outputOid = findTypeOutputFunction(outputName, typoid);
	if (receiveName)
		receiveOid = findTypeReceiveFunction(receiveName, typoid);
	if (sendName)
		sendOid = findTypeSendFunction(sendName, typoid);

	/*
	 * Verify that I/O procs return the expected thing.  If we see OPAQUE,
	 * complain and change it to the correct type-safe choice.
	 */
	resulttype = get_func_rettype(inputOid);
	if (resulttype != typoid)
	{
		if (resulttype == OPAQUEOID)
		{
			/* backwards-compatibility hack */
			ereport(WARNING,
					(errmsg("changing return type of function %s from \"opaque\" to %s",
							NameListToString(inputName), typeName)));
			SetFunctionReturnType(inputOid, typoid);
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
					 errmsg("type input function %s must return type %s",
							NameListToString(inputName), typeName)));
	}
	resulttype = get_func_rettype(outputOid);
	if (resulttype != CSTRINGOID)
	{
		if (resulttype == OPAQUEOID)
		{
			/* backwards-compatibility hack */
			ereport(WARNING,
					(errmsg("changing return type of function %s from \"opaque\" to \"cstring\"",
							NameListToString(outputName))));
			SetFunctionReturnType(outputOid, CSTRINGOID);
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
			   errmsg("type output function %s must return type \"cstring\"",
					  NameListToString(outputName))));
	}
	if (receiveOid)
	{
		resulttype = get_func_rettype(receiveOid);
		if (resulttype != typoid)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
					 errmsg("type receive function %s must return type %s",
							NameListToString(receiveName), typeName)));
	}
	if (sendOid)
	{
		resulttype = get_func_rettype(sendOid);
		if (resulttype != BYTEAOID)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				   errmsg("type send function %s must return type \"bytea\"",
						  NameListToString(sendName))));
	}

	/*
	 * Convert typmodin/out function proc names to OIDs.
	 */
	if (typmodinName)
		typmodinOid = findTypeTypmodinFunction(typmodinName);
	if (typmodoutName)
		typmodoutOid = findTypeTypmodoutFunction(typmodoutName);

	/*
	 * Convert analysis function proc name to an OID. If no analysis function
	 * is specified, we'll use zero to select the built-in default algorithm.
	 */
	if (analyzeName)
		analyzeOid = findTypeAnalyzeFunction(analyzeName, typoid);

	/*
	 * Check permissions on functions.	We choose to require the creator/owner
	 * of a type to also own the underlying functions.	Since creating a type
	 * is tantamount to granting public execute access on the functions, the
	 * minimum sane check would be for execute-with-grant-option.  But we
	 * don't have a way to make the type go away if the grant option is
	 * revoked, so ownership seems better.
	 */
	if (inputOid && !pg_proc_ownercheck(inputOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(inputName));
	if (outputOid && !pg_proc_ownercheck(outputOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(outputName));
	if (receiveOid && !pg_proc_ownercheck(receiveOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(receiveName));
	if (sendOid && !pg_proc_ownercheck(sendOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(sendName));
	if (typmodinOid && !pg_proc_ownercheck(typmodinOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(typmodinName));
	if (typmodoutOid && !pg_proc_ownercheck(typmodoutOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(typmodoutName));
	if (analyzeOid && !pg_proc_ownercheck(analyzeOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_PROC,
					   NameListToString(analyzeName));

	/*
	 * now have TypeCreate do all the real work.
	 */
	typoid =
		TypeCreateWithOid(typeName,	/* type name */
				   typeNamespace,		/* namespace */
				   InvalidOid,	/* relation oid (n/a here) */
				   0,			/* relation kind (ditto) */
				   GetUserId(),	/* owner's ID */
				   internalLength,		/* internal size */
				   typtype,		/* type-type (base type) */
				   delimiter,	/* array element delimiter */
				   inputOid,	/* input procedure */
				   outputOid,	/* output procedure */
				   receiveOid,	/* receive procedure */
				   sendOid,		/* send procedure */
				   typmodinOid, /* typmodin procedure */
				   typmodoutOid,/* typmodout procedure */
				   analyzeOid,	/* analyze procedure */
				   elemType,	/* element type ID */
				   InvalidOid,	/* base type ID (only for domains) */
				   defaultValue,	/* default type value */
				   NULL,		/* no binary form available */
				   byValue,		/* passed by value */
				   alignment,	/* required alignment */
				   storage,		/* TOAST strategy */
				   -1,			/* typMod (Domains only) */
				   0,			/* Array Dimensions of typbasetype */
				   false,		/* Type NOT NULL */
				   newOid,
				   typoptions);

	/*
	 * When we create a base type (as opposed to a complex type) we need to
	 * have an array entry for it in pg_type as well.
	 *
	 * Create a array type only if we're not creating an internal type during upgrade.
	 */
	if (typtype == TYPTYPE_BASE)
	{
		shadow_type = makeArrayTypeName(typeName);

		/* alignment must be 'i' or 'd' for arrays */
		alignment = (alignment == 'd') ? 'd' : 'i';

		shadowOid = TypeCreateWithOid(shadow_type,		/* type name */
			   typeNamespace,	/* namespace */
			   InvalidOid,		/* relation oid (n/a here) */
			   0,				/* relation kind (ditto) */
			   GetUserId(),		/* owner's ID */
			   -1,				/* internal size */
			   TYPTYPE_BASE,	/* type-type (base type) */
			   delimiter,		/* array element delimiter */
			   F_ARRAY_IN,		/* input procedure */
			   F_ARRAY_OUT,		/* output procedure */
			   F_ARRAY_RECV,	/* receive procedure */
			   F_ARRAY_SEND,	/* send procedure */
			   typmodinOid,		/* typmodin procedure */
			   typmodoutOid,	/* typmodout procedure */
			   InvalidOid,		/* analyze procedure - default */
			   typoid,			/* element type ID */
			   InvalidOid,		/* base type ID */
			   NULL,			/* never a default type value */
			   NULL,			/* binary default isn't sent either */
			   false,			/* never passed by value */
			   alignment,		/* see above */
			   'x',				/* ARRAY is always toastable */
			   -1,				/* typMod (Domains only) */
			   0,				/* Array dimensions of typbasetype */
			   false,			/* Type NOT NULL */
			   shadowOid,
			   typoptions);

		pfree(shadow_type);
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		DefineStmt * stmt = makeNode(DefineStmt);
		stmt->kind = OBJECT_TYPE;
		stmt->oldstyle = false; /*?*/
		stmt->defnames = names;
		stmt->args = NIL;
		stmt->definition = parameters;
		stmt->newOid = typoid;
		stmt->shadowOid = shadowOid;
		CdbDispatchUtilityStatement((Node *) stmt, "DefineType");
	}
}


/*
 *	RemoveType
 *		Removes a datatype.
 */
void
RemoveType(List *names, DropBehavior behavior, bool missing_ok)
{
	TypeName   *typename;
	Oid			typeoid;
	Oid			typeNsp;
	ObjectAddress object;
	int			 fetchCount;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);

	/* Use LookupTypeName here so that shell types can be removed. */
	typeoid = LookupTypeName(NULL, typename);
	if (!OidIsValid(typeoid))
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("type \"%s\" does not exist",
							TypeNameToString(typename))));
		}
		else
		{
			if (Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("type \"%s\" does not exist, skipping",
							TypeNameToString(typename))));
		}

		return;
	}

	typeNsp = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT typnamespace FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typeoid)));

	if (!fetchCount)
		elog(ERROR, "cache lookup failed for type %u", typeoid);

	/* Permission check: must own type or its namespace */
	if (!pg_type_ownercheck(typeoid, GetUserId()) &&
		!pg_namespace_ownercheck(typeNsp, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE,
					   TypeNameToString(typename));


	/*
	 * Remove any storage encoding
	 */
	remove_type_encoding(typeoid);

	/*
	 * Do the deletion
	 */
	object.classId = TypeRelationId;
	object.objectId = typeoid;
	object.objectSubId = 0;

	performDeletion(&object, behavior);
}


/*
 * Guts of type deletion.
 */
void
RemoveTypeById(Oid typeOid)
{
	Relation	relation;
	cqContext	cqc;

	/* 
	 * It might look like the call in RemoveType() is enough for
	 * pg_type_encoding but it's not. This case catches array type derivations
	 * of base types.
	 */
	remove_type_encoding(typeOid);

	relation = heap_open(TypeRelationId, RowExclusiveLock);

	if (0 ==
		caql_getcount(
			caql_addrel(cqclr(&cqc), relation),
			cql("DELETE FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typeOid))))
	{
		elog(ERROR, "cache lookup failed for type %u", typeOid);
	}

	heap_close(relation,  Gp_role == GP_ROLE_DISPATCH ? NoLock : RowExclusiveLock);
}


/*
 * DefineDomain
 *		Registers a new domain.
 */
void
DefineDomain(CreateDomainStmt *stmt)
{
	char	   *domainName;
	Oid			domainNamespace;
	AclResult	aclresult;
	int16		internalLength;
	Oid			inputProcedure;
	Oid			outputProcedure;
	Oid			receiveProcedure;
	Oid			sendProcedure;
	Oid			analyzeProcedure;
	bool		byValue;
	Oid			typelem;
	char		delimiter;
	char		alignment;
	char		storage;
	char		typtype;
	Datum		datum;
	bool		isnull;
	char	   *defaultValue = NULL;
	char	   *defaultValueBin = NULL;
	bool		saw_default = false;
	bool		typNotNull = false;
	bool		nullDefined = false;
	int32		typNDims = list_length(stmt->typname->arrayBounds);
	HeapTuple	typeTup;
	List	   *schema = stmt->constraints;
	ListCell   *listptr;
	Oid			basetypeoid;
	Oid			domainoid;
	Form_pg_type baseType;
	int32		basetypeMod;

	/* Convert list of names to a name and namespace */
	domainNamespace = QualifiedNameGetCreationNamespace(stmt->domainname,
														&domainName);

	/* Check we have creation rights in target namespace */
	aclresult = pg_namespace_aclcheck(domainNamespace, GetUserId(),
									  ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
					   get_namespace_name(domainNamespace));

	/*
	 * Domainnames, unlike typenames don't need to account for the '_' prefix.
	 * So they can be one character longer.  (This test is presently useless
	 * since the parser will have truncated the name to fit.  But leave it
	 * here since we may someday support arrays of domains, in which case
	 * we'll be back to needing to enforce NAMEDATALEN-2.)
	 */
	if (strlen(domainName) > (NAMEDATALEN - 1))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("domain names must be %d characters or less",
						NAMEDATALEN - 1)));

	/*
	 * Look up the base type.
	 */
	typeTup = typenameType(NULL, stmt->typname);
	baseType = (Form_pg_type) GETSTRUCT(typeTup);
	basetypeoid = HeapTupleGetOid(typeTup);
	basetypeMod = typenameTypeMod(NULL, stmt->typname, basetypeoid);

	/*
	 * Base type must be a plain base type or another domain.  Domains over
	 * pseudotypes would create a security hole.  Domains over composite types
	 * might be made to work in the future, but not today.
	 */
	typtype = baseType->typtype;
	if (typtype != TYPTYPE_BASE && typtype != TYPTYPE_DOMAIN)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("\"%s\" is not a valid base type for a domain",
						TypeNameToString(stmt->typname))));

	/* passed by value */
	byValue = baseType->typbyval;

	/* Required Alignment */
	alignment = baseType->typalign;

	/* TOAST Strategy */
	storage = baseType->typstorage;

	/* Storage Length */
	internalLength = baseType->typlen;

	/* Array element type (in case base type is an array) */
	typelem = baseType->typelem;

	/* Array element Delimiter */
	delimiter = baseType->typdelim;

	/* I/O Functions */
	inputProcedure = F_DOMAIN_IN;
	outputProcedure = baseType->typoutput;
	receiveProcedure = F_DOMAIN_RECV;
	sendProcedure = baseType->typsend;

	/* Domains never accept typmods, so no typmodin/typmodout needed */

	/* Analysis function */
	analyzeProcedure = baseType->typanalyze;

	/* Inherited default value */
	datum = SysCacheGetAttr(TYPEOID, typeTup,
							Anum_pg_type_typdefault, &isnull);
	if (!isnull)
		defaultValue = DatumGetCString(DirectFunctionCall1(textout, datum));

	/* Inherited default binary value */
	datum = SysCacheGetAttr(TYPEOID, typeTup,
							Anum_pg_type_typdefaultbin, &isnull);
	if (!isnull)
		defaultValueBin = DatumGetCString(DirectFunctionCall1(textout, datum));

	/*
	 * Run through constraints manually to avoid the additional processing
	 * conducted by DefineRelation() and friends.
	 */
	foreach(listptr, schema)
	{
		Node	   *newConstraint = lfirst(listptr);
		Constraint *constr;

		/* Check for unsupported constraint types */
		if (IsA(newConstraint, FkConstraint))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("foreign key constraints not possible for domains")));

		/* otherwise it should be a plain Constraint */
		if (!IsA(newConstraint, Constraint))
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(newConstraint));

		constr = (Constraint *) newConstraint;

		switch (constr->contype)
		{
			case CONSTR_DEFAULT:

				/*
				 * The inherited default value may be overridden by the user
				 * with the DEFAULT <expr> clause ... but only once.
				 */
				if (saw_default)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("multiple default expressions")));
				saw_default = true;

				if (constr->raw_expr)
				{
					ParseState *pstate;
					Node	   *defaultExpr;

					/* Create a dummy ParseState for transformExpr */
					pstate = make_parsestate(NULL);

					/*
					 * Cook the constr->raw_expr into an expression.
					 * Note: name is strictly for error message
					 */
					defaultExpr = cookDefault(pstate, constr->raw_expr,
											  basetypeoid,
											  basetypeMod,
											  domainName);

					free_parsestate(pstate);

					/*
					 * Expression must be stored as a nodeToString result, but
					 * we also require a valid textual representation (mainly
					 * to make life easier for pg_dump).
					 */
					defaultValue =
						deparse_expression(defaultExpr,
										   deparse_context_for(domainName,
															   InvalidOid),
										   false, false);
					defaultValueBin = nodeToString(defaultExpr);
				}
				else
				{
					/* DEFAULT NULL is same as not having a default */
					defaultValue = NULL;
					defaultValueBin = NULL;
				}
				break;

			case CONSTR_NOTNULL:
				if (nullDefined && !typNotNull)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
						   errmsg("conflicting NULL/NOT NULL constraints")));
				typNotNull = true;
				nullDefined = true;
				break;

			case CONSTR_NULL:
				if (nullDefined && typNotNull)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
						   errmsg("conflicting NULL/NOT NULL constraints")));
				typNotNull = false;
				nullDefined = true;
				break;

			case CONSTR_CHECK:

				/*
				 * Check constraints are handled after domain creation, as
				 * they require the Oid of the domain
				 */
				break;

				/*
				 * All else are error cases
				 */
			case CONSTR_UNIQUE:
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unique constraints not possible for domains")));
				break;

			case CONSTR_PRIMARY:
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("primary key constraints not possible for domains")));
				break;

			case CONSTR_ATTR_DEFERRABLE:
			case CONSTR_ATTR_NOT_DEFERRABLE:
			case CONSTR_ATTR_DEFERRED:
			case CONSTR_ATTR_IMMEDIATE:
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("specifying constraint deferrability not supported for domains")));
				break;

			default:
				elog(ERROR, "unrecognized constraint subtype: %d",
					 (int) constr->contype);
				break;
		}
	}

	/*
	 * Have TypeCreate do all the real work.
	 */
	domainoid =
		TypeCreateWithOid(domainName,	/* type name */
				   domainNamespace,		/* namespace */
				   InvalidOid,	/* relation oid (n/a here) */
				   0,			/* relation kind (ditto) */
				   GetUserId(),	/* owner's ID */
				   internalLength,		/* internal size */
				   TYPTYPE_DOMAIN,		/* type-type (domain type) */
				   delimiter,	/* array element delimiter */
				   inputProcedure,		/* input procedure */
				   outputProcedure,		/* output procedure */
				   receiveProcedure,	/* receive procedure */
				   sendProcedure,		/* send procedure */
				   InvalidOid,			/* typmodin procedure - none */
				   InvalidOid,			/* typmodout procedure - none */
				   analyzeProcedure,	/* analyze procedure */
				   typelem,		/* element type ID */
				   basetypeoid, /* base type ID */
				   defaultValue,	/* default type value (text) */
				   defaultValueBin,		/* default type value (binary) */
				   byValue,		/* passed by value */
				   alignment,	/* required alignment */
				   storage,		/* TOAST strategy */
				   basetypeMod, /* typeMod value */
				   typNDims,	/* Array dimensions for base type */
				   typNotNull,	/* Type NOT NULL */
				   stmt->domainOid,
				   0);

	stmt->domainOid = domainoid;
	/*
	 * Process constraints which refer to the domain ID returned by TypeCreate
	 */
	foreach(listptr, schema)
	{
		Constraint *constr = lfirst(listptr);

		/* it must be a Constraint, per check above */

		switch (constr->contype)
		{
			case CONSTR_CHECK:
				domainAddConstraint(domainoid, domainNamespace,
									basetypeoid, basetypeMod,
									constr, domainName);
				break;

				/* Other constraint types were fully processed above */

			default:
				break;
		}

		/* CCI so we can detect duplicate constraint names */
		CommandCounterIncrement();
	}

	/*
	 * Now we can clean up.
	 */
	ReleaseType(typeTup);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt, "DefineDomain");
	}
}


/*
 *	RemoveDomain
 *		Removes a domain.
 *
 * This is identical to RemoveType except we insist it be a domain.
 */
void
RemoveDomain(List *names, DropBehavior behavior, bool missing_ok)
{
	TypeName   *typename;
	Oid			typeoid;
	HeapTuple	tup;
	char		typtype;
	ObjectAddress object;
	cqContext  *pcqCtx;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);

	/* Use LookupTypeName here so that shell types can be removed. */
	typeoid = LookupTypeName(NULL, typename);
	if (!OidIsValid(typeoid))
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("type \"%s\" does not exist",
							TypeNameToString(typename))));
		}
		else
		{
			if (Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("type \"%s\" does not exist, skipping",
							TypeNameToString(typename))));
		}

		return;
	}

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typeoid)));

	tup = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", typeoid);

	/* Permission check: must own type or its namespace */
	if (!pg_type_ownercheck(typeoid, GetUserId()) &&
	  !pg_namespace_ownercheck(((Form_pg_type) GETSTRUCT(tup))->typnamespace,
							   GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE,
					   TypeNameToString(typename));

	/* Check that this is actually a domain */
	typtype = ((Form_pg_type) GETSTRUCT(tup))->typtype;

	if (typtype != TYPTYPE_DOMAIN)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a domain",
						TypeNameToString(typename))));

	caql_endscan(pcqCtx);

	/*
	 * Do the deletion
	 */
	object.classId = TypeRelationId;
	object.objectId = typeoid;
	object.objectSubId = 0;

	performDeletion(&object, behavior);
}


/*
 * Find suitable I/O functions for a type.
 *
 * typeOid is the type's OID (which will already exist, if only as a shell
 * type).
 */

static Oid
findTypeInputFunction(List *procname, Oid typeOid)
{
	Oid			argList[3];
	Oid			procOid;

	/*
	 * Input functions can take a single argument of type CSTRING, or three
	 * arguments (string, typioparam OID, typmod).
	 *
	 * For backwards compatibility we allow OPAQUE in place of CSTRING; if we
	 * see this, we issue a warning and fix up the pg_proc entry.
	 */
	argList[0] = CSTRINGOID;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (OidIsValid(procOid))
		return procOid;

	argList[1] = OIDOID;
	argList[2] = INT4OID;

	procOid = LookupFuncName(procname, 3, argList, true);
	if (OidIsValid(procOid))
		return procOid;

	/* No luck, try it with OPAQUE */
	argList[0] = OPAQUEOID;

	procOid = LookupFuncName(procname, 1, argList, true);

	if (!OidIsValid(procOid))
	{
		argList[1] = OIDOID;
		argList[2] = INT4OID;

		procOid = LookupFuncName(procname, 3, argList, true);
	}

	if (OidIsValid(procOid))
	{
		/* Found, but must complain and fix the pg_proc entry */
		ereport(WARNING,
				(errmsg("changing argument type of function %s from \"opaque\" to \"cstring\"",
						NameListToString(procname))));
		SetFunctionArgType(procOid, 0, CSTRINGOID);

		/*
		 * Need CommandCounterIncrement since DefineType will likely try to
		 * alter the pg_proc tuple again.
		 */
		CommandCounterIncrement();

		return procOid;
	}

	/* Use CSTRING (preferred) in the error message */
	argList[0] = CSTRINGOID;

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_FUNCTION),
			 errmsg("function %s does not exist",
					func_signature_string(procname, 1, argList))));

	return InvalidOid;			/* keep compiler quiet */
}

static Oid
findTypeOutputFunction(List *procname, Oid typeOid)
{
	Oid			argList[1];
	Oid			procOid;

	/*
	 * Output functions can take a single argument of the type.
	 *
	 * For backwards compatibility we allow OPAQUE in place of the actual type
	 * name; if we see this, we issue a warning and fix up the pg_proc entry.
	 */
	argList[0] = typeOid;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (OidIsValid(procOid))
		return procOid;

	/* No luck, try it with OPAQUE */
	argList[0] = OPAQUEOID;

	procOid = LookupFuncName(procname, 1, argList, true);

	if (OidIsValid(procOid))
	{
		/* Found, but must complain and fix the pg_proc entry */
		ereport(WARNING,
		(errmsg("changing argument type of function %s from \"opaque\" to %s",
				NameListToString(procname), format_type_be(typeOid))));
		SetFunctionArgType(procOid, 0, typeOid);

		/*
		 * Need CommandCounterIncrement since DefineType will likely try to
		 * alter the pg_proc tuple again.
		 */
		CommandCounterIncrement();

		return procOid;
	}

	/* Use type name, not OPAQUE, in the failure message. */
	argList[0] = typeOid;

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_FUNCTION),
			 errmsg("function %s does not exist",
					func_signature_string(procname, 1, argList))));

	return InvalidOid;			/* keep compiler quiet */
}

static Oid
findTypeReceiveFunction(List *procname, Oid typeOid)
{
	Oid			argList[3];
	Oid			procOid;

	/*
	 * Receive functions can take a single argument of type INTERNAL, or three
	 * arguments (internal, typioparam OID, typmod).
	 */
	argList[0] = INTERNALOID;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (OidIsValid(procOid))
		return procOid;

	argList[1] = OIDOID;
	argList[2] = INT4OID;

	procOid = LookupFuncName(procname, 3, argList, true);
	if (OidIsValid(procOid))
		return procOid;

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_FUNCTION),
			 errmsg("function %s does not exist",
					func_signature_string(procname, 1, argList))));

	return InvalidOid;			/* keep compiler quiet */
}

static Oid
findTypeSendFunction(List *procname, Oid typeOid)
{
	Oid			argList[1];
	Oid			procOid;

	/*
	 * Send functions can take a single argument of the type.
	 */
	argList[0] = typeOid;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (OidIsValid(procOid))
		return procOid;

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_FUNCTION),
			 errmsg("function %s does not exist",
					func_signature_string(procname, 1, argList))));

	return InvalidOid;			/* keep compiler quiet */
}

static Oid
findTypeTypmodinFunction(List *procname)
{
	Oid			argList[1];
	Oid			procOid;

	/*
	 * typmodin functions always take one cstring[] argument and return int4.
	 */
	argList[0] = CSTRINGARRAYOID;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (!OidIsValid(procOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function %s does not exist",
						func_signature_string(procname, 1, argList))));

	if (get_func_rettype(procOid) != INT4OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("typmod_in function %s must return type \"integer\"",
						NameListToString(procname))));

	return procOid;
}

static Oid
findTypeTypmodoutFunction(List *procname)
{
	Oid			argList[1];
	Oid			procOid;

	/*
	 * typmodout functions always take one int4 argument and return cstring.
	 */
	argList[0] = INT4OID;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (!OidIsValid(procOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function %s does not exist",
						func_signature_string(procname, 1, argList))));

	if (get_func_rettype(procOid) != CSTRINGOID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("typmod_out function %s must return type \"cstring\"",
						NameListToString(procname))));

	return procOid;
}

static Oid
findTypeAnalyzeFunction(List *procname, Oid typeOid)
{
	Oid			argList[1];
	Oid			procOid;

	/*
	 * Analyze functions always take one INTERNAL argument and return bool.
	 */
	argList[0] = INTERNALOID;

	procOid = LookupFuncName(procname, 1, argList, true);
	if (!OidIsValid(procOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function %s does not exist",
						func_signature_string(procname, 1, argList))));

	if (get_func_rettype(procOid) != BOOLOID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
			  errmsg("type analyze function %s must return type \"boolean\"",
					 NameListToString(procname))));

	return procOid;
}


/*-------------------------------------------------------------------
 * DefineCompositeType
 *
 * Create a Composite Type relation.
 * `DefineRelation' does all the work, we just provide the correct
 * arguments!
 *
 * If the relation already exists, then 'DefineRelation' will abort
 * the xact...
 *
 * DefineCompositeType returns relid for use when creating
 * an implicit composite type during function creation
 *-------------------------------------------------------------------
 */
Oid
DefineCompositeType(const RangeVar *typevar, List *coldeflist, Oid relOid, Oid comptypeOid)
{
	CreateStmt *createStmt = makeNode(CreateStmt);

	createStmt->oidInfo.relOid = relOid;
	createStmt->oidInfo.comptypeOid = comptypeOid;
	createStmt->oidInfo.toastOid = 0;
	createStmt->oidInfo.toastIndexOid = 0;
	createStmt->oidInfo.aosegOid = 0;
	createStmt->oidInfo.aosegIndexOid = 0;
	createStmt->oidInfo.aoblkdirOid = 0;
	createStmt->oidInfo.aoblkdirIndexOid = 0;
	createStmt->oidInfo.aovisimapOid = 0;
	createStmt->oidInfo.aovisimapIndexOid = 0;
	createStmt->ownerid = GetUserId();

	if (coldeflist == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("composite type must have at least one attribute")));

	/*
	 * now set the parameters for keys/inheritance etc. All of these are
	 * uninteresting for composite types...
	 */
	createStmt->relation = (RangeVar *) typevar;
	createStmt->tableElts = coldeflist;
	createStmt->inhRelations = NIL;
	createStmt->constraints = NIL;
	createStmt->options = list_make1(defWithOids(false));
	createStmt->oncommit = ONCOMMIT_NOOP;
	createStmt->tablespacename = NULL;

	/*
	 * finally create the relation...
	 */
	return  DefineRelation(createStmt, RELKIND_COMPOSITE_TYPE, RELSTORAGE_VIRTUAL);

	/*
	 * DefineRelation already dispatches this.
	 *
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CompositeTypeStmt *stmt = makeNode(CompositeTypeStmt);
		stmt->typevar = (RangeVar *)typevar;
		stmt->coldeflist = coldeflist;
		stmt->relOid = newRelOid;
		stmt->comptypeOid = createStmt->comptypeOid;

		CdbDispatchUtilityStatement((Node *) stmt);
	}*/

}

/*
 * AlterDomainDefault
 *
 * Routine implementing ALTER DOMAIN SET/DROP DEFAULT statements.
 */
void
AlterDomainDefault(List *names, Node *defaultRaw)
{
	TypeName   *typename;
	Oid			domainoid;
	HeapTuple	tup;
	ParseState *pstate;
	Relation	rel;
	char	   *defaultValue;
	Node	   *defaultExpr = NULL;		/* NULL if no default specified */
	Datum		new_record[Natts_pg_type];
	bool		new_record_nulls[Natts_pg_type];
	bool		new_record_repl[Natts_pg_type];
	HeapTuple	newtuple;
	Form_pg_type typTup;
	cqContext	*pcqCtx;
	cqContext	 cqc;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);
	domainoid = typenameTypeId(NULL, typename);

	/* Look up the domain in the type table */
	rel = heap_open(TypeRelationId, RowExclusiveLock);
	
	pcqCtx = caql_addrel(cqclr(&cqc), rel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(domainoid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", domainoid);
	typTup = (Form_pg_type) GETSTRUCT(tup);

	/* Check it's a domain and check user has permission for ALTER DOMAIN */
	checkDomainOwner(tup, typename);

	/* Setup new tuple */
	MemSet(new_record, (Datum) 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));
	MemSet(new_record_repl, false, sizeof(new_record_repl));

	/* Store the new default, if null then skip this step */
	if (defaultRaw)
	{
		/* Create a dummy ParseState for transformExpr */
		pstate = make_parsestate(NULL);

		/*
		 * Cook the colDef->raw_expr into an expression. Note: Name is
		 * strictly for error message
		 */
		defaultExpr = cookDefault(pstate, defaultRaw,
								  typTup->typbasetype,
								  typTup->typtypmod,
								  NameStr(typTup->typname));

		free_parsestate(pstate);

		/*
		 * Expression must be stored as a nodeToString result, but we also
		 * require a valid textual representation (mainly to make life easier
		 * for pg_dump).
		 */
		defaultValue = deparse_expression(defaultExpr,
								deparse_context_for(NameStr(typTup->typname),
													InvalidOid),
										  false, false);

		/*
		 * Form an updated tuple with the new default and write it back.
		 */
		new_record[Anum_pg_type_typdefaultbin - 1] = DirectFunctionCall1(textin,
															 CStringGetDatum(
												 nodeToString(defaultExpr)));

		new_record_repl[Anum_pg_type_typdefaultbin - 1] = true;
		new_record[Anum_pg_type_typdefault - 1] = DirectFunctionCall1(textin,
											  CStringGetDatum(defaultValue));
		new_record_repl[Anum_pg_type_typdefault - 1] = true;
	}
	else
		/* Default is NULL, drop it */
	{
		new_record_nulls[Anum_pg_type_typdefaultbin - 1] = true;
		new_record_repl[Anum_pg_type_typdefaultbin - 1] = true;
		new_record_nulls[Anum_pg_type_typdefault - 1] = true;
		new_record_repl[Anum_pg_type_typdefault - 1] = true;
	}

	newtuple = heap_modify_tuple(tup, RelationGetDescr(rel),
								new_record, new_record_nulls,
								new_record_repl);

	caql_update_current(pcqCtx, newtuple);

	/* Rebuild dependencies */
	GenerateTypeDependencies(typTup->typnamespace,
							 domainoid,
							 typTup->typrelid,
							 0, /* relation kind is n/a */
							 typTup->typowner,
							 typTup->typinput,
							 typTup->typoutput,
							 typTup->typreceive,
							 typTup->typsend,
							 typTup->typmodin,
							 typTup->typmodout,
							 typTup->typanalyze,
							 typTup->typelem,
							 typTup->typbasetype,
							 defaultExpr,
							 true);		/* Rebuild is true */

	/* Clean up */
	heap_close(rel, NoLock);
	heap_freetuple(newtuple);
}

/*
 * AlterDomainNotNull
 *
 * Routine implementing ALTER DOMAIN SET/DROP NOT NULL statements.
 */
void
AlterDomainNotNull(List *names, bool notNull)
{
	TypeName   *typename;
	Oid			domainoid;
	Relation	typrel;
	HeapTuple	tup;
	Form_pg_type typTup;
	cqContext	*pcqCtx;
	cqContext	 cqc;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);
	domainoid = typenameTypeId(NULL, typename);

	/* Look up the domain in the type table */
	typrel = heap_open(TypeRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), typrel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(domainoid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", domainoid);
	typTup = (Form_pg_type) GETSTRUCT(tup);

	/* Check it's a domain and check user has permission for ALTER DOMAIN */
	checkDomainOwner(tup, typename);

	/* Is the domain already set to the desired constraint? */
	if (typTup->typnotnull == notNull)
	{
		heap_close(typrel, RowExclusiveLock);
		return;
	}

	/* Adding a NOT NULL constraint requires checking existing columns */
	if (notNull)
	{
		List	   *rels;
		ListCell   *rt;

		/* Fetch relation list with attributes based on this domain */
		/* ShareLock is sufficient to prevent concurrent data changes */

		rels = get_rels_with_domain(domainoid, ShareLock);

		foreach(rt, rels)
		{
			RelToCheck *rtc = (RelToCheck *) lfirst(rt);
			Relation	testrel = rtc->rel;
			TupleDesc	tupdesc = RelationGetDescr(testrel);
			HeapScanDesc scan;
			HeapTuple	tuple;

			/* Scan all tuples in this relation */
			scan = heap_beginscan(testrel, SnapshotNow, 0, NULL);
			while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
			{
				int			i;

				/* Test attributes that are of the domain */
				for (i = 0; i < rtc->natts; i++)
				{
					int			attnum = rtc->atts[i];

					if (heap_attisnull(tuple, attnum))
						ereport(ERROR,
								(errcode(ERRCODE_NOT_NULL_VIOLATION),
								 errmsg("column \"%s\" of table \"%s\" contains null values",
								NameStr(tupdesc->attrs[attnum - 1]->attname),
										RelationGetRelationName(testrel))));
				}
			}
			heap_endscan(scan);

			/* Close each rel after processing, but keep lock */
			heap_close(testrel, NoLock);
		}
	}

	/*
	 * Okay to update pg_type row.	We can scribble on typTup because it's a
	 * copy.
	 */
	typTup->typnotnull = notNull;

	caql_update_current(pcqCtx, tup);

	/* Clean up */
	heap_freetuple(tup);
	heap_close(typrel, RowExclusiveLock);
}

/*
 * AlterDomainDropConstraint
 *
 * Implements the ALTER DOMAIN DROP CONSTRAINT statement
 */
void
AlterDomainDropConstraint(List *names, const char *constrName,
						  DropBehavior behavior)
{
	TypeName   *typename;
	Oid			domainoid;
	HeapTuple	tup;
	Relation	rel;
	cqContext  *pcqCtx;
	cqContext	cqc;
	HeapTuple	contup;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);
	domainoid = typenameTypeId(NULL, typename);

	/* Look up the domain in the type table */
	rel = heap_open(TypeRelationId, RowExclusiveLock);

	/* NOTE: separate cqc and pcqCtx here -- 
	 * cqc for pg_type, pcqCtx for pg_constraint 
	 */
	tup = caql_getfirst(
			caql_addrel(cqclr(&cqc), rel),
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(domainoid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", domainoid);

	/* Check it's a domain and check user has permission for ALTER DOMAIN */
	checkDomainOwner(tup, typename);

	/* Grab an appropriate lock on the pg_constraint relation */
	/* Use the index to scan only constraints of the target relation */

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_constraint "
				" WHERE contypid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(HeapTupleGetOid(tup))));
	/*
	 * Scan over the result set, removing any matching entries.
	 */
	while (HeapTupleIsValid(contup = caql_getnext(pcqCtx)))
	{
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(contup);

		if (strcmp(NameStr(con->conname), constrName) == 0)
		{
			ObjectAddress conobj;

			conobj.classId = ConstraintRelationId;
			conobj.objectId = HeapTupleGetOid(contup);
			conobj.objectSubId = 0;

			performDeletion(&conobj, behavior);
		}
	}
	/* Clean up after the scan */
	caql_endscan(pcqCtx);

	heap_close(rel, NoLock);
}

/*
 * AlterDomainAddConstraint
 *
 * Implements the ALTER DOMAIN .. ADD CONSTRAINT statement.
 */
void
AlterDomainAddConstraint(List *names, Node *newConstraint)
{
	TypeName   *typename;
	Oid			domainoid;
	Relation	typrel;
	HeapTuple	tup;
	Form_pg_type typTup;
	List	   *rels;
	ListCell   *rt;
	EState	   *estate;
	ExprContext *econtext;
	char	   *ccbin;
	Expr	   *expr;
	ExprState  *exprstate;
	Constraint *constr;
	cqContext	cqc;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);
	domainoid = typenameTypeId(NULL, typename);

	/* Look up the domain in the type table */
	typrel = heap_open(TypeRelationId, RowExclusiveLock);

	tup = caql_getfirst(
			caql_addrel(cqclr(&cqc), typrel),
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(domainoid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", domainoid);
	typTup = (Form_pg_type) GETSTRUCT(tup);

	/* Check it's a domain and check user has permission for ALTER DOMAIN */
	checkDomainOwner(tup, typename);

	/* Check for unsupported constraint types */
	if (IsA(newConstraint, FkConstraint))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("foreign key constraints not possible for domains")));

	/* otherwise it should be a plain Constraint */
	if (!IsA(newConstraint, Constraint))
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(newConstraint));

	constr = (Constraint *) newConstraint;

	switch (constr->contype)
	{
		case CONSTR_CHECK:
			/* processed below */
			break;

		case CONSTR_UNIQUE:
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unique constraints not possible for domains")));
			break;

		case CONSTR_PRIMARY:
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("primary key constraints not possible for domains")));
			break;

		case CONSTR_ATTR_DEFERRABLE:
		case CONSTR_ATTR_NOT_DEFERRABLE:
		case CONSTR_ATTR_DEFERRED:
		case CONSTR_ATTR_IMMEDIATE:
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("specifying constraint deferrability not supported for domains")));
			break;

		default:
			elog(ERROR, "unrecognized constraint subtype: %d",
				 (int) constr->contype);
			break;
	}

	/*
	 * Since all other constraint types throw errors, this must be a check
	 * constraint.	First, process the constraint expression and add an entry
	 * to pg_constraint.
	 */

	ccbin = domainAddConstraint(HeapTupleGetOid(tup), typTup->typnamespace,
								typTup->typbasetype, typTup->typtypmod,
								constr, NameStr(typTup->typname));

	/*
	 * Test all values stored in the attributes based on the domain the
	 * constraint is being added to.
	 */
	expr = (Expr *) stringToNode(ccbin);

	/* Need an EState to run ExecEvalExpr */
	estate = CreateExecutorState();
	econtext = GetPerTupleExprContext(estate);

	/* build execution state for expr */
	exprstate = ExecPrepareExpr(expr, estate);

	/* Fetch relation list with attributes based on this domain */
	/* ShareLock is sufficient to prevent concurrent data changes */

	rels = get_rels_with_domain(domainoid, ShareLock);

	foreach(rt, rels)
	{
		RelToCheck *rtc = (RelToCheck *) lfirst(rt);
		Relation	testrel = rtc->rel;
		TupleDesc	tupdesc = RelationGetDescr(testrel);
		HeapScanDesc scan;
		HeapTuple	tuple;

		/* Scan all tuples in this relation */
		scan = heap_beginscan(testrel, SnapshotNow, 0, NULL);
		while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
		{
			int			i;

			/* Test attributes that are of the domain */
			for (i = 0; i < rtc->natts; i++)
			{
				int			attnum = rtc->atts[i];
				Datum		d;
				bool		isNull;
				Datum		conResult;

				d = heap_getattr(tuple, attnum, tupdesc, &isNull);

				econtext->domainValue_datum = d;
				econtext->domainValue_isNull = isNull;

				conResult = ExecEvalExprSwitchContext(exprstate,
													  econtext,
													  &isNull, NULL);

				if (!isNull && !DatumGetBool(conResult))
					ereport(ERROR,
							(errcode(ERRCODE_CHECK_VIOLATION),
							 errmsg("column \"%s\" of table \"%s\" contains values that violate the new constraint",
								NameStr(tupdesc->attrs[attnum - 1]->attname),
									RelationGetRelationName(testrel))));
			}

			ResetExprContext(econtext);
		}
		heap_endscan(scan);

		/* Hold relation lock till commit (XXX bad for concurrency) */
		heap_close(testrel, NoLock);
	}

	FreeExecutorState(estate);

	/* Clean up */
	heap_close(typrel, RowExclusiveLock);
}

/*
 * get_rels_with_domain
 *
 * Fetch all relations / attributes which are using the domain
 *
 * The result is a list of RelToCheck structs, one for each distinct
 * relation, each containing one or more attribute numbers that are of
 * the domain type.  We have opened each rel and acquired the specified lock
 * type on it.
 *
 * We support nested domains by including attributes that are of derived
 * domain types.  Current callers do not need to distinguish between attributes
 * that are of exactly the given domain and those that are of derived domains.
 *
 * XXX this is completely broken because there is no way to lock the domain
 * to prevent columns from being added or dropped while our command runs.
 * We can partially protect against column drops by locking relations as we
 * come across them, but there is still a race condition (the window between
 * seeing a pg_depend entry and acquiring lock on the relation it references).
 * Also, holding locks on all these relations simultaneously creates a non-
 * trivial risk of deadlock.  We can minimize but not eliminate the deadlock
 * risk by using the weakest suitable lock (ShareLock for most callers).
 *
 * XXX the API for this is not sufficient to support checking domain values
 * that are inside composite types or arrays.  Currently we just error out
 * if a composite type containing the target domain is stored anywhere.
 * There are not currently arrays of domains; if there were, we could take
 * the same approach, but it'd be nicer to fix it properly.
 *
 * Generally used for retrieving a list of tests when adding
 * new constraints to a domain.
 */
static List *
get_rels_with_domain(Oid domainOid, LOCKMODE lockmode)
{
	List	   *result = NIL;
	cqContext  *pcqCtx;
	HeapTuple	depTup;

	Assert(lockmode != NoLock);

	/*
	 * We scan pg_depend to find those things that depend on the domain. (We
	 * assume we can ignore refobjsubid for a domain.)
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_depend "
				" WHERE refclassid = :1 "
				" AND refobjid = :2 ",
				ObjectIdGetDatum(TypeRelationId),
				ObjectIdGetDatum(domainOid)));

	while (HeapTupleIsValid(depTup = caql_getnext(pcqCtx)))
	{
		Form_pg_depend pg_depend = (Form_pg_depend) GETSTRUCT(depTup);
		RelToCheck *rtc = NULL;
		ListCell   *rellist;
		Form_pg_attribute pg_att;
		int			ptr;

		/* Check for directly dependent types --- must be domains */
		if (pg_depend->classid == TypeRelationId)
		{
			Assert(get_typtype(pg_depend->objid) == TYPTYPE_DOMAIN);
			/*
			 * Recursively add dependent columns to the output list.  This
			 * is a bit inefficient since we may fail to combine RelToCheck
			 * entries when attributes of the same rel have different derived
			 * domain types, but it's probably not worth improving.
			 */
			result = list_concat(result,
								 get_rels_with_domain(pg_depend->objid,
													  lockmode));
			continue;
		}

		/* Else, ignore dependees that aren't user columns of relations */
		/* (we assume system columns are never of domain types) */
		if (pg_depend->classid != RelationRelationId ||
			pg_depend->objsubid <= 0)
			continue;

		/* See if we already have an entry for this relation */
		foreach(rellist, result)
		{
			RelToCheck *rt = (RelToCheck *) lfirst(rellist);

			if (RelationGetRelid(rt->rel) == pg_depend->objid)
			{
				rtc = rt;
				break;
			}
		}

		if (rtc == NULL)
		{
			/* First attribute found for this relation */
			Relation	rel;

			/* Acquire requested lock on relation */
			rel = relation_open(pg_depend->objid, lockmode);

			/*
			 * Check to see if rowtype is stored anyplace as a composite-type
			 * column; if so we have to fail, for now anyway.
			 */
			if (OidIsValid(rel->rd_rel->reltype))
				find_composite_type_dependencies(rel->rd_rel->reltype,
												 NULL,
												 format_type_be(domainOid));

			/* Otherwise we can ignore views, composite types, etc */
			if (rel->rd_rel->relkind != RELKIND_RELATION)
			{
				relation_close(rel, lockmode);
				continue;
			}

			/* Build the RelToCheck entry with enough space for all atts */
			rtc = (RelToCheck *) palloc(sizeof(RelToCheck));
			rtc->rel = rel;
			rtc->natts = 0;
			rtc->atts = (int *) palloc(sizeof(int) * RelationGetNumberOfAttributes(rel));
			result = lcons(rtc, result);
		}

		/*
		 * Confirm column has not been dropped, and is of the expected type.
		 * This defends against an ALTER DROP COLUMN occuring just before we
		 * acquired lock ... but if the whole table were dropped, we'd still
		 * have a problem.
		 */
		if (pg_depend->objsubid > RelationGetNumberOfAttributes(rtc->rel))
			continue;
		pg_att = rtc->rel->rd_att->attrs[pg_depend->objsubid - 1];
		if (pg_att->attisdropped || pg_att->atttypid != domainOid)
			continue;

		/*
		 * Okay, add column to result.	We store the columns in column-number
		 * order; this is just a hack to improve predictability of regression
		 * test output ...
		 */
		Assert(rtc->natts < RelationGetNumberOfAttributes(rtc->rel));

		ptr = rtc->natts++;
		while (ptr > 0 && rtc->atts[ptr - 1] > pg_depend->objsubid)
		{
			rtc->atts[ptr] = rtc->atts[ptr - 1];
			ptr--;
		}
		rtc->atts[ptr] = pg_depend->objsubid;
	}

	caql_endscan(pcqCtx);

	return result;
}

/*
 * checkDomainOwner
 *
 * Check that the type is actually a domain and that the current user
 * has permission to do ALTER DOMAIN on it.  Throw an error if not.
 */
static void
checkDomainOwner(HeapTuple tup, TypeName *typename)
{
	Form_pg_type typTup = (Form_pg_type) GETSTRUCT(tup);

	/* Check that this is actually a domain */
	if (typTup->typtype != TYPTYPE_DOMAIN)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a domain",
						TypeNameToString(typename))));

	/* Permission check: must own type */
	if (!pg_type_ownercheck(HeapTupleGetOid(tup), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE,
					   TypeNameToString(typename));
}

/*
 * domainAddConstraint - code shared between CREATE and ALTER DOMAIN
 */
static char *
domainAddConstraint(Oid domainOid, Oid domainNamespace, Oid baseTypeOid,
					int typMod, Constraint *constr,
					char *domainName)
{
	Node	   *expr;
	char	   *ccsrc;
	char	   *ccbin;
	ParseState *pstate;
	CoerceToDomainValue *domVal;

	/*
	 * Assign or validate constraint name
	 */
	if (constr->name)
	{
		if (ConstraintNameIsUsed(CONSTRAINT_DOMAIN,
								 domainOid,
								 domainNamespace,
								 constr->name))
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("constraint \"%s\" for domain \"%s\" already exists",
						constr->name, domainName)));
	}
	else
		constr->name = ChooseConstraintName(domainName,
											NULL,
											"check",
											domainNamespace,
											NIL);

	/*
	 * Convert the A_EXPR in raw_expr into an EXPR
	 */
	pstate = make_parsestate(NULL);

	/*
	 * Set up a CoerceToDomainValue to represent the occurrence of VALUE in
	 * the expression.	Note that it will appear to have the type of the base
	 * type, not the domain.  This seems correct since within the check
	 * expression, we should not assume the input value can be considered a
	 * member of the domain.
	 */
	domVal = makeNode(CoerceToDomainValue);
	domVal->typeId = baseTypeOid;
	domVal->typeMod = typMod;

	pstate->p_value_substitute = (Node *) domVal;

	expr = transformExpr(pstate, constr->raw_expr);

	/*
	 * Make sure it yields a boolean result.
	 */
	expr = coerce_to_boolean(pstate, expr, "CHECK");

	/*
	 * Make sure no outside relations are referred to.
	 */
	if (list_length(pstate->p_rtable) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
		  errmsg("cannot use table references in domain check constraint")));

	/*
	 * Domains don't allow var clauses (this should be redundant with the
	 * above check, but make it anyway)
	 */
	if (contain_var_clause(expr))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
		  errmsg("cannot use table references in domain check constraint")));

	/*
	 * No subplans or aggregates, either...
	 */
	if (pstate->p_hasSubLinks)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot use subquery in check constraint")));
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
			   errmsg("cannot use aggregate function in check constraint")));
	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
			   errmsg("cannot use window function in check constraint")));

	free_parsestate(pstate);

	/*
	 * Convert to string form for storage.
	 */
	ccbin = nodeToString(expr);

	/*
	 * Deparse it to produce text for consrc.
	 *
	 * Since VARNOs aren't allowed in domain constraints, relation context
	 * isn't required as anything other than a shell.
	 */
	ccsrc = deparse_expression(expr,
							   deparse_context_for(domainName,
												   InvalidOid),
							   false, false);

	/*
	 * Store the constraint in pg_constraint
	 */
	constr->conoid = CreateConstraintEntry(constr->name, /* Constraint Name */
										   constr->conoid,	/* Constraint Oid */
										   domainNamespace,		/* namespace */
										   CONSTRAINT_CHECK,		/* Constraint Type */
										   false,	/* Is Deferrable */
										   false,	/* Is Deferred */
										   InvalidOid,	/* not a relation constraint */
										   NULL,
										   0,
										   domainOid,	/* domain constraint */
										   InvalidOid,	/* Foreign key fields */
										   NULL,
										   NULL,
										   NULL,
										   NULL,
										   0,
										   ' ',
										   ' ',
										   ' ',
										   InvalidOid,
										   expr, /* Tree form check constraint */
										   ccbin,	/* Binary form check constraint */
										   ccsrc);		/* Source form check constraint */

	/*
	 * Return the compiled constraint expression so the calling routine can
	 * perform any additional required tests.
	 */
	return ccbin;
}

/*
 * GetDomainConstraints - get a list of the current constraints of domain
 *
 * Returns a possibly-empty list of DomainConstraintState nodes.
 *
 * This is called by the executor during plan startup for a CoerceToDomain
 * expression node.  The given constraints will be checked for each value
 * passed through the node.
 *
 * We allow this to be called for non-domain types, in which case the result
 * is always NIL.
 */
List *
GetDomainConstraints(Oid typeOid)
{
	List	   *result = NIL;
	bool		notNull = false;
	Relation	conRel;

	conRel = heap_open(ConstraintRelationId, AccessShareLock);

	for (;;)
	{
		HeapTuple	tup;
		HeapTuple	conTup;
		Form_pg_type typTup;
		cqContext	*typcqCtx;
		cqContext	*pcqCtx;
		cqContext	 cqc;

		typcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_type "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(typeOid)));

		tup = caql_getnext(typcqCtx);

		if (!HeapTupleIsValid(tup))
			elog(ERROR, "cache lookup failed for type %u", typeOid);
		typTup = (Form_pg_type) GETSTRUCT(tup);

		if (typTup->typtype != TYPTYPE_DOMAIN)
		{
			/* Not a domain, so done */
			caql_endscan(typcqCtx);
			break;
		}

		/* Test for NOT NULL Constraint */
		if (typTup->typnotnull)
			notNull = true;

		/* Look for CHECK Constraints on this domain */

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), conRel),
				cql("SELECT * FROM pg_constraint "
					" WHERE contypid = :1 ",
					ObjectIdGetDatum(typeOid)));

		while (HeapTupleIsValid(conTup = caql_getnext(pcqCtx)))
		{
			Form_pg_constraint c = (Form_pg_constraint) GETSTRUCT(conTup);
			Datum		val;
			bool		isNull;
			Expr	   *check_expr;
			DomainConstraintState *r;

			/* Ignore non-CHECK constraints (presently, shouldn't be any) */
			if (c->contype != CONSTRAINT_CHECK)
				continue;

			/*
			 * Not expecting conbin to be NULL, but we'll test for it anyway
			 */
			val = caql_getattr(pcqCtx, Anum_pg_constraint_conbin,
							   &isNull);
			if (isNull)
				elog(ERROR, "domain \"%s\" constraint \"%s\" has NULL conbin",
					 NameStr(typTup->typname), NameStr(c->conname));

			check_expr = (Expr *)
				stringToNode(DatumGetCString(DirectFunctionCall1(textout,
																 val)));

			/* ExecInitExpr assumes we already fixed opfuncids */
			fix_opfuncids((Node *) check_expr);

			r = makeNode(DomainConstraintState);
			r->constrainttype = DOM_CONSTRAINT_CHECK;
			r->name = pstrdup(NameStr(c->conname));
			r->check_expr = ExecInitExpr(check_expr, NULL);

			/*
			 * use lcons() here because constraints of lower domains should be
			 * applied earlier.
			 */
			result = lcons(r, result);
		}

		caql_endscan(pcqCtx);

		/* loop to next domain in stack */
		typeOid = typTup->typbasetype;
		caql_endscan(typcqCtx);
	}

	heap_close(conRel, AccessShareLock);

	/*
	 * Only need to add one NOT NULL check regardless of how many domains in
	 * the stack request it.
	 */
	if (notNull)
	{
		DomainConstraintState *r = makeNode(DomainConstraintState);

		r->constrainttype = DOM_CONSTRAINT_NOTNULL;
		r->name = pstrdup("NOT NULL");
		r->check_expr = NULL;

		/* lcons to apply the nullness check FIRST */
		result = lcons(r, result);
	}

	return result;
}

/*
 * Change the owner of a type.
 */
void
AlterTypeOwner(List *names, Oid newOwnerId)
{
	TypeName   *typename;
	Oid			typeOid;
	Relation	rel;
	HeapTuple	tup;
	Form_pg_type typTup;
	AclResult	aclresult;
	cqContext  *pcqCtx;
	cqContext	cqc;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);

	/* Use LookupTypeName here so that shell types can be processed (why?) */
	typeOid = LookupTypeName(NULL, typename);
	if (!OidIsValid(typeOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type \"%s\" does not exist",
						TypeNameToString(typename))));

	/* Look up the type in the type table */
	rel = heap_open(TypeRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), rel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(typeOid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", typeOid);
	typTup = (Form_pg_type) GETSTRUCT(tup);

	/*
	 * If it's a composite type, we need to check that it really is a
	 * free-standing composite type, and not a table's underlying type. We
	 * want people to use ALTER TABLE not ALTER TYPE for that case.
	 */
	if (typTup->typtype == TYPTYPE_COMPOSITE &&
		get_rel_relkind(typTup->typrelid) != RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a table's row type",
						TypeNameToString(typename))));

	/*
	 * If the new owner is the same as the existing owner, consider the
	 * command to have succeeded.  This is for dump restoration purposes.
	 */
	if (typTup->typowner != newOwnerId)
	{
		/* Superusers can always do it */
		if (!superuser())
		{
			/* Otherwise, must be owner of the existing object */
			if (!pg_type_ownercheck(HeapTupleGetOid(tup), GetUserId()))
				aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE,
							   TypeNameToString(typename));

			/* Must be able to become new owner */
			check_is_member_of_role(GetUserId(), newOwnerId);

			/* New owner must have CREATE privilege on namespace */
			aclresult = pg_namespace_aclcheck(typTup->typnamespace,
											  newOwnerId,
											  ACL_CREATE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
							   get_namespace_name(typTup->typnamespace));
		}

		/*
		 * If it's a composite type, invoke ATExecChangeOwner so that we
		 * fix up the pg_class entry properly.  That will call back to
		 * AlterTypeOwnerInternal to take care of the pg_type entry(s).
		 */
		if (typTup->typtype == TYPTYPE_COMPOSITE)
			ATExecChangeOwner(typTup->typrelid, newOwnerId, true);
		else
		{
			/*
			 * We can just apply the modification directly.
			 *
			 * okay to scribble on typTup because it's a copy
			 */
			typTup->typowner = newOwnerId;

			caql_update_current(pcqCtx, tup);

			/* Update owner dependency reference */
			changeDependencyOnOwner(TypeRelationId, typeOid, newOwnerId);
		}
	}

	/* Clean up */
	heap_close(rel, RowExclusiveLock);
}

/*
 * AlterTypeOwnerInternal - change type owner unconditionally
 *
 * This is currently only used to propagate ALTER TABLE/TYPE OWNER to a
 * table's rowtype or an array type, and to implement REASSIGN OWNED BY.
 * It assumes the caller has done all needed checks.  The function will
 * automatically recurse to an array type if the type has one.
 *
 * hasDependEntry should be TRUE if type is expected to have a pg_shdepend
 * entry (ie, it's not a table rowtype nor an array type).
 */
void
AlterTypeOwnerInternal(Oid typeOid, Oid newOwnerId,
					   bool hasDependEntry)
{
	Relation	rel;
	HeapTuple	tup;
	Form_pg_type typTup;
	cqContext  *pcqCtx;
	cqContext	cqc;

	rel = heap_open(TypeRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), rel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(typeOid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", typeOid);
	typTup = (Form_pg_type) GETSTRUCT(tup);

	/*
	 * Modify the owner --- okay to scribble on typTup because it's a copy
	 */
	typTup->typowner = newOwnerId;

	caql_update_current(pcqCtx, tup);

	/* Update owner dependency reference, if it has one */
	if (hasDependEntry)
		changeDependencyOnOwner(TypeRelationId, typeOid, newOwnerId);

	/* Clean up */
	heap_close(rel, RowExclusiveLock);
}

/*
 * Execute ALTER TYPE SET SCHEMA
 */
void
AlterTypeNamespace(List *names, const char *newschema)
{
	TypeName   *typename;
	Oid			typeOid;
	Oid			nspOid;

	/* Make a TypeName so we can use standard type lookup machinery */
	typename = makeTypeNameFromNameList(names);
	typeOid = typenameTypeId(NULL, typename);

	/* check permissions on type */
	if (!pg_type_ownercheck(typeOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE,
					   format_type_be(typeOid));

	/* get schema OID and check its permissions */
	nspOid = LookupCreationNamespace(newschema);

	/* and do the work */
	AlterTypeNamespaceInternal(typeOid, nspOid, true);
}

/*
 * Move specified type to new namespace.
 *
 * Caller must have already checked privileges.
 *
 * If errorOnTableType is TRUE, the function errors out if the type is
 * a table type.  ALTER TABLE has to be used to move a table to a new
 * namespace.
 */
void
AlterTypeNamespaceInternal(Oid typeOid, Oid nspOid,
						   bool errorOnTableType)
{
	Relation	rel;
	HeapTuple	tup;
	Form_pg_type typform;
	Oid			oldNspOid;
	bool		isCompositeType;
	cqContext  *pcqCtx;
	cqContext	cqc, cqc2;

	rel = heap_open(TypeRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), rel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(typeOid)));

	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", typeOid);
	typform = (Form_pg_type) GETSTRUCT(tup);

	oldNspOid = typform->typnamespace;

	if (oldNspOid == nspOid)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("type %s is already in schema \"%s\"",
						format_type_be(typeOid),
						get_namespace_name(nspOid))));

	/* disallow renaming into or out of temp schemas */
	if (isAnyTempNamespace(nspOid) || isAnyTempNamespace(oldNspOid))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("cannot move objects into or out of temporary schemas")));

	/* same for TOAST schema */
	if (nspOid == PG_TOAST_NAMESPACE || oldNspOid == PG_TOAST_NAMESPACE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot move objects into or out of TOAST schema")));

	/* same for AO SEGMENT schema */
	if (nspOid == PG_AOSEGMENT_NAMESPACE || oldNspOid == PG_AOSEGMENT_NAMESPACE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot move objects into or out of AO SEGMENT schema")));

	/* check for duplicate name (more friendly than unique-index failure) */
	if (caql_getcount(
				caql_addrel(cqclr(&cqc2), rel),
					cql("SELECT COUNT(*) FROM pg_type "
						" WHERE typname = :1 "
						" AND typnamespace = :2 ",
						CStringGetDatum(NameStr(typform->typname)),
						ObjectIdGetDatum(nspOid))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("type \"%s\" already exists in schema \"%s\"",
						NameStr(typform->typname),
						get_namespace_name(nspOid))));
	}

	/* Detect whether type is a composite type (but not a table rowtype) */
	isCompositeType =
		(typform->typtype == TYPTYPE_COMPOSITE &&
		 get_rel_relkind(typform->typrelid) == RELKIND_COMPOSITE_TYPE);

	/* Enforce not-table-type if requested */
	if (typform->typtype == TYPTYPE_COMPOSITE && 
		!isCompositeType && errorOnTableType)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("%s is a table's row type",
						format_type_be(typeOid)),
				 errhint("Use ALTER TABLE SET SCHEMA instead.")));

	/* OK, modify the pg_type row */

	/* tup is a copy, so we can scribble directly on it */
	typform->typnamespace = nspOid;

	caql_update_current(pcqCtx, tup);

	/*
	 * Composite types have pg_class entries.
	 *
	 * We need to modify the pg_class tuple as well to reflect the change of
	 * schema.
	 */
	if (isCompositeType)
	{
		Relation	classRel;

		classRel = heap_open(RelationRelationId, RowExclusiveLock);

		AlterRelationNamespaceInternal(classRel, typform->typrelid,
									   oldNspOid, nspOid,
									   false);

		heap_close(classRel, RowExclusiveLock);

		/*
		 * Check for constraints associated with the composite type (we don't
		 * currently support this, but probably will someday).
		 */
		AlterConstraintNamespaces(typform->typrelid, oldNspOid,
								  nspOid, false);
	}
	else
	{
		/* If it's a domain, it might have constraints */
		if (typform->typtype == TYPTYPE_DOMAIN)
			AlterConstraintNamespaces(typeOid, oldNspOid, nspOid, true);
	}

	/*
	 * Update dependency on schema, if any --- a table rowtype has not got
	 * one.
	 */
	if (isCompositeType || typform->typtype != TYPTYPE_COMPOSITE)
		if (changeDependencyFor(TypeRelationId, typeOid,
							NamespaceRelationId, oldNspOid, nspOid) != 1)
			elog(ERROR, "failed to change schema dependency for type %s",
				 format_type_be(typeOid));

	heap_freetuple(tup);

	heap_close(rel, RowExclusiveLock);
}

/*
 * Currently, we only land here if the user has issued:
 *
 * ALTER TYPE <typname> SET DEFAULT ENCODING (...)
 */
void
AlterType(AlterTypeStmt *stmt)
{
	TypeName	   *typname = stmt->typname;
	Oid				typid;
	cqContext	   *pcqCtx;
	cqContext		cqc;
	HeapTuple		tup;
	Datum			typoptions;
	bool			typmod_set = false;
	List		   *encoding = NIL;
	Relation 		pgtypeenc = heap_open(TypeEncodingRelationId,
										  RowExclusiveLock);

	/* Make a TypeName so we can use standard type lookup machinery */
	typid = LookupTypeName(NULL, typname);

	if (!OidIsValid(typid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type \"%s\" does not exist",
						TypeNameToString(typname))));

	if (type_is_rowtype(typid))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("type \"%s\" is not a base type",
						TypeNameToString(typname)),
				 errhint("The ENCODING clause cannot be used with row or "
						 "composite types.")));

	if (typid == BPCHAROID)
	{
		/* 
		 * for character, the user specified character (N) if typmod is not
		 * VARHDRSZ + 1.
		 */
		int32		typmod;

		typmod = typenameTypeMod(NULL, typname, typid);
		if (typmod != VARHDRSZ + 1)
			typmod_set = true;
	}
	else if (typid == BITOID)
	{
		/*
		 * For bit, we assume bit(1) in the parser. So only reject cases where
		 * the typmod is not 1.
		 */
		int32		typmod;

		typmod = typenameTypeMod(NULL, typname, typid);
		if (typmod != 1)
			typmod_set = true;
	}
	else if (typname->typmods || typname->typemod != -1)
		typmod_set = true;

	if (typmod_set)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("type precision cannot be specified when setting "
						"DEFAULT ENCODING")));

	if (typname->arrayBounds)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("type array bounds cannot be specified when setting "
						"DEFAULT ENCODING")));


	/* check permissions on type */
	if (!pg_type_ownercheck(typid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE,
					   format_type_be(typid));

	encoding = transformStorageEncodingClause(stmt->encoding);

	typoptions = transformRelOptions(PointerGetDatum(NULL),
									 encoding,
									 false,
									 false);

	pcqCtx = caql_addrel(cqclr(&cqc), pgtypeenc);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type_encoding "
				" WHERE typid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(typid)));

	if (HeapTupleIsValid(tup))
	{
		/* update case */
		Datum values[Natts_pg_type_encoding];
		bool nulls[Natts_pg_type_encoding];
		bool replaces[Natts_pg_type_encoding];
		HeapTuple newtuple;

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_type_encoding_typoptions - 1] = true;
		values[Anum_pg_type_encoding_typoptions - 1] = typoptions;

		newtuple = heap_modify_tuple(tup, RelationGetDescr(pgtypeenc),
									 values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
	}
	else
	{
		add_type_encoding(typid, typoptions);
	}	

	if (Gp_role == GP_ROLE_DISPATCH)
		CdbDispatchUtilityStatement((Node *)stmt, NULL);

	heap_close(pgtypeenc, NoLock);
}

/*
 * Remove the default type encoding for typid.
 */
static void
remove_type_encoding(Oid typid)
{
	int numDel;

	numDel = caql_getcount(
			NULL,
			cql("DELETE FROM pg_type_encoding "
				" WHERE typid = :1 ",
				ObjectIdGetDatum(typid)));
}
