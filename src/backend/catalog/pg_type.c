/*-------------------------------------------------------------------------
 *
 * pg_type.c
 *	  routines to support manipulation of the pg_type relation
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/pg_type.c,v 1.110 2007/01/05 22:19:25 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_encoding.h"
#include "commands/typecmds.h"
#include "miscadmin.h"
#include "parser/scansup.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/syscache.h"

#include "cdb/cdbvars.h"

/*
 * Record a type's default encoding clause in the catalog.
 */
void
add_type_encoding(Oid typid, Datum typoptions)
{
	Datum		 values[Natts_pg_type_encoding];
	bool		 nulls[Natts_pg_type_encoding];
	HeapTuple	 tuple;
	cqContext	*pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("INSERT INTO pg_type_encoding ",
				NULL));

	MemSet(nulls, false, sizeof(nulls));
	
	values[Anum_pg_type_encoding_typid - 1] = ObjectIdGetDatum(typid);
	values[Anum_pg_type_encoding_typoptions - 1] = typoptions;

	tuple = caql_form_tuple(pcqCtx, values, nulls);

	/* Insert tuple into the relation */
	caql_insert(pcqCtx, tuple); /* implicit update of index as well */

	caql_endscan(pcqCtx);
}

/* ----------------------------------------------------------------
 *		TypeShellMake
 *
 *		This procedure inserts a "shell" tuple into the pg_type relation.
 *		The type tuple inserted has valid but dummy values, and its
 *		"typisdefined" field is false indicating it's not really defined.
 *
 *		This is used so that a tuple exists in the catalogs.  The I/O
 *		functions for the type will link to this tuple.  When the full
 *		CREATE TYPE command is issued, the bogus values will be replaced
 *		with correct ones, and "typisdefined" will be set to true.
 * ----------------------------------------------------------------
 */
Oid
TypeShellMake(const char *typeName, Oid typeNamespace, Oid ownerId,
			  Oid shelloid)
{
	return TypeShellMakeWithOid(typeName,typeNamespace,ownerId, shelloid);
}

Oid
TypeShellMakeWithOid(const char *typeName, Oid typeNamespace, Oid ownerId,
					 Oid shelltypeOid)
{
	int			i;
	HeapTuple	tup;
	Datum		values[Natts_pg_type];
	bool		nulls[Natts_pg_type];
	Oid			typoid;
	NameData	name;
	cqContext  *pcqCtx;

	Assert(PointerIsValid(typeName));

	/*
	 * open pg_type
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("INSERT INTO pg_type ",
				NULL));

	/*
	 * initialize our *nulls and *values arrays
	 */
	for (i = 0; i < Natts_pg_type; ++i)
	{
		nulls[i] = false;
		values[i] = (Datum) 0;		/* redundant, but safe */
	}

	/*
	 * initialize *values with the type name and dummy values
	 *
	 * The representational details are the same as int4 ... it doesn't really
	 * matter what they are so long as they are consistent.  Also note that we
	 * give it typtype = 'p' (pseudotype) as extra insurance that it won't be
	 * mistaken for a usable type.
	 */
	i = 0;
	namestrcpy(&name, typeName);
	values[i++] = NameGetDatum(&name);	/* typname */
	values[i++] = ObjectIdGetDatum(typeNamespace);		/* typnamespace */
	values[i++] = ObjectIdGetDatum(ownerId);		/* typowner */
	values[i++] = Int16GetDatum(sizeof(int4));	/* typlen */
	values[i++] = BoolGetDatum(true);	/* typbyval */
	values[i++] = CharGetDatum('p');	/* typtype */
	values[i++] = BoolGetDatum(false);	/* typisdefined */
	values[i++] = CharGetDatum(DEFAULT_TYPDELIM);		/* typdelim */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typrelid */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typelem */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typarray */
	values[i++] = ObjectIdGetDatum(F_SHELL_IN); /* typinput */
	values[i++] = ObjectIdGetDatum(F_SHELL_OUT);		/* typoutput */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typreceive */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typsend */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typmodin */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typmodout */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typanalyze */
	values[i++] = CharGetDatum('i');	/* typalign */
	values[i++] = CharGetDatum('p');	/* typstorage */
	values[i++] = BoolGetDatum(false);	/* typnotnull */
	values[i++] = ObjectIdGetDatum(InvalidOid); /* typbasetype */
	values[i++] = Int32GetDatum(-1);	/* typtypmod */
	values[i++] = Int32GetDatum(0);		/* typndims */
	nulls[i++] = true;			/* typdefaultbin */
	nulls[i++] = true;			/* typdefault */

	/*
	 * create a new type tuple
	 */
	tup = caql_form_tuple(pcqCtx, values, nulls);

	/*
	 * MPP: If we are on the QEs, we need to use the same Oid as the QD used
	 */
	if (shelltypeOid != InvalidOid)
		HeapTupleSetOid(tup, shelltypeOid);
	/*
	 * insert the tuple in the relation and get the tuple's oid.
	 */
	typoid = caql_insert(pcqCtx, tup); /* implicit update of index as well */

	/*
	 * Create dependencies.  We can/must skip this in bootstrap mode.
	 */
	if (!IsBootstrapProcessingMode())
		GenerateTypeDependencies(typeNamespace,
								 typoid,
								 InvalidOid,
								 0,
								 ownerId,
								 F_SHELL_IN,
								 F_SHELL_OUT,
								 InvalidOid,
								 InvalidOid,
								 InvalidOid,
								 InvalidOid,
								 InvalidOid,
								 InvalidOid,
								 false,
								 InvalidOid,
								 NULL,
								 false);

	/*
	 * clean up and return the type-oid
	 */
	heap_freetuple(tup);
	caql_endscan(pcqCtx);

	return typoid;
}

/* ----------------------------------------------------------------
 *		TypeCreate
 *
 *		This does all the necessary work needed to define a new type.
 *
 *		Returns the OID assigned to the new type.
 * ----------------------------------------------------------------
 */
Oid
TypeCreateWithOid(const char *typeName,
		   Oid typeNamespace,
		   Oid relationOid,		/* only for 'c'atalog types */
		   char relationKind,	/* ditto */
		   Oid ownerId,
		   int16 internalSize,
		   char typeType,
		   char typDelim,
		   Oid inputProcedure,
		   Oid outputProcedure,
		   Oid receiveProcedure,
		   Oid sendProcedure,
		   Oid typmodinProcedure,
		   Oid typmodoutProcedure,
		   Oid analyzeProcedure,
		   Oid elementType,
		   bool isImplicitArray,
		   Oid arrayType,
		   Oid baseType,
		   const char *defaultTypeValue,		/* human readable rep */
		   char *defaultTypeBin,	/* cooked rep */
		   bool passedByValue,
		   char alignment,
		   char storage,
		   int32 typeMod,
		   int32 typNDims,		/* Array dimensions for baseType */
		   bool typeNotNull,
		   Oid newtypeOid,
		   Datum typoptions)
{
	Relation	pg_type_desc;
	Oid			typeObjectId;
	bool		rebuildDeps = false;
	HeapTuple	tup;
	bool		nulls[Natts_pg_type];
	bool		replaces[Natts_pg_type];
	Datum		values[Natts_pg_type];
	NameData	name;
	int			i;
	cqContext	*pcqCtx;
	cqContext	 cqc;

	/*
	 * We assume that the caller validated the arguments individually, but did
	 * not check for bad combinations.
	 *
	 * Validate size specifications: either positive (fixed-length) or -1
	 * (varlena) or -2 (cstring).  Pass-by-value types must have a fixed
	 * length not more than sizeof(Datum).
	 */
	if (!(internalSize > 0 ||
		  internalSize == -1 ||
		  internalSize == -2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("invalid type internal size %d",
						internalSize)));
	if (passedByValue &&
		(internalSize <= 0 || internalSize > (int16) sizeof(Datum)))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
			   errmsg("internal size %d is invalid for passed-by-value type",
					  internalSize)));

	/* Only varlena types can be toasted */
	if (storage != 'p' && internalSize != -1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("fixed-size types must have storage PLAIN")));

	/*
	 * initialize arrays needed for heap_form_tuple or heap_modify_tuple
	 */
	for (i = 0; i < Natts_pg_type; ++i)
	{
		nulls[i] = false;
		replaces[i] = true;
		values[i] = (Datum) 0;
	}

	/*
	 * initialize the *values information
	 */
	i = 0;
	namestrcpy(&name, typeName);
	values[i++] = NameGetDatum(&name);	/* typname */
	values[i++] = ObjectIdGetDatum(typeNamespace);		/* typnamespace */
	values[i++] = ObjectIdGetDatum(ownerId);		/* typowner */
	values[i++] = Int16GetDatum(internalSize);	/* typlen */
	values[i++] = BoolGetDatum(passedByValue);	/* typbyval */
	values[i++] = CharGetDatum(typeType);		/* typtype */
	values[i++] = BoolGetDatum(true);	/* typisdefined */
	values[i++] = CharGetDatum(typDelim);		/* typdelim */
	values[i++] = ObjectIdGetDatum(relationOid);		/* typrelid */
	values[i++] = ObjectIdGetDatum(elementType);		/* typelem */
	values[i++] = ObjectIdGetDatum(arrayType);			/* typarray */
	values[i++] = ObjectIdGetDatum(inputProcedure);		/* typinput */
	values[i++] = ObjectIdGetDatum(outputProcedure);	/* typoutput */
	values[i++] = ObjectIdGetDatum(receiveProcedure);	/* typreceive */
	values[i++] = ObjectIdGetDatum(sendProcedure);		/* typsend */
	values[i++] = ObjectIdGetDatum(typmodinProcedure);	/* typmodin */
	values[i++] = ObjectIdGetDatum(typmodoutProcedure);	/* typmodout */
	values[i++] = ObjectIdGetDatum(analyzeProcedure);	/* typanalyze */
	values[i++] = CharGetDatum(alignment);		/* typalign */
	values[i++] = CharGetDatum(storage);		/* typstorage */
	values[i++] = BoolGetDatum(typeNotNull);	/* typnotnull */
	values[i++] = ObjectIdGetDatum(baseType);	/* typbasetype */
	values[i++] = Int32GetDatum(typeMod);		/* typtypmod */
	values[i++] = Int32GetDatum(typNDims);		/* typndims */

	/*
	 * initialize the default binary value for this type.  Check for nulls of
	 * course.
	 */
	if (defaultTypeBin)
		values[i] = CStringGetTextDatum(defaultTypeBin);
	else
		nulls[i] = true;
	i++;						/* typdefaultbin */

	/*
	 * initialize the default value for this type.
	 */
	if (defaultTypeValue)
		values[i] = CStringGetTextDatum(defaultTypeValue);
	else
		nulls[i] = true;
	i++;						/* typdefault */

	/*
	 * open pg_type and prepare to insert or update a row.
	 *
	 * NOTE: updating will not work correctly in bootstrap mode; but we don't
	 * expect to be overwriting any shell types in bootstrap mode.
	 */
	pg_type_desc = heap_open(TypeRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), pg_type_desc);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE typname = :1 "
				" AND typnamespace = :2 "
				" FOR UPDATE ",
				CStringGetDatum((char *) typeName),
				ObjectIdGetDatum(typeNamespace)));

	if (HeapTupleIsValid(tup))
	{
		/*
		 * check that the type is not already defined.	It may exist as a
		 * shell type, however.
		 */
		if (((Form_pg_type) GETSTRUCT(tup))->typisdefined)
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("type \"%s\" already exists", typeName)));

		/*
		 * shell type must have been created by same owner
		 */
		if (((Form_pg_type) GETSTRUCT(tup))->typowner != ownerId)
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_TYPE, typeName);

		/*  trouble if caller wanted to force the OID */
		if (OidIsValid(newtypeOid) &&
			newtypeOid != HeapTupleHeaderGetOid((tup)->t_data))
			elog(ERROR, "cannot assign new OID to existing shell type");

		/*
		 * Okay to update existing shell type tuple
		 */
		tup = caql_modify_current(pcqCtx,
								  values,
								  nulls,
								  replaces);

		caql_update_current(pcqCtx, tup);
		/* and Update indexes (implicit) */

		typeObjectId = HeapTupleGetOid(tup);

		rebuildDeps = true;		/* get rid of shell type's dependencies */
	}
	else
	{
		tup = caql_form_tuple(pcqCtx, values, nulls);
						 
		if (newtypeOid != InvalidOid)
		{
			elog(DEBUG5," Setting Oid in new pg_type tuple");
			HeapTupleSetOid(tup, newtypeOid);
		}
		else if (Gp_role == GP_ROLE_EXECUTE) elog(ERROR," newtypeOid NULL");

		/* Insert tuple into the relation */
		typeObjectId = caql_insert(pcqCtx, tup);
		/* and Update indexes (implicit) */
	}

	/*
	 * Create dependencies.  We can/must skip this in bootstrap mode.
	 */
	if (!IsBootstrapProcessingMode())
		GenerateTypeDependencies(typeNamespace,
								 typeObjectId,
								 relationOid,
								 relationKind,
								 ownerId,
								 inputProcedure,
								 outputProcedure,
								 receiveProcedure,
								 sendProcedure,
								 typmodinProcedure,
								 typmodoutProcedure,
								 analyzeProcedure,
								 elementType,
								 isImplicitArray,
								 baseType,
								 (defaultTypeBin ?
								  stringToNode(defaultTypeBin) :
								  NULL),
								 rebuildDeps);

	/*
	 * finish up with pg_type
	 */
	heap_close(pg_type_desc, RowExclusiveLock);

	/* now pg_type_encoding */
	if (DatumGetPointer(typoptions) != NULL)
		add_type_encoding(typeObjectId, typoptions);

	return typeObjectId;
}

Oid TypeCreate(const char *typeName,
		   Oid typeNamespace,
		   Oid relationOid,
		   char relationKind,
		   Oid ownerId,
		   int16 internalSize,
		   char typeType,
		   char typDelim,
		   Oid inputProcedure,
		   Oid outputProcedure,
		   Oid receiveProcedure,
		   Oid sendProcedure,
		   Oid typmodinProcedure,
		   Oid typmodoutProcedure,
		   Oid analyzeProcedure,
		   Oid elementType,
		   bool isImplicitArray,
		   Oid arrayType,
		   Oid baseType,
		   const char *defaultTypeValue,
		   char *defaultTypeBin,
		   bool passedByValue,
		   char alignment,
		   char storage,
		   int32 typeMod,
		   int32 typNDims,
		   bool typeNotNull)
{
	return TypeCreateWithOid(
							typeName,
							typeNamespace,
							relationOid,
							relationKind,
							ownerId,
							internalSize,
							typeType,
							typDelim,
							inputProcedure,
							outputProcedure,
							receiveProcedure,
							sendProcedure,
							typmodinProcedure,
							typmodoutProcedure,
							analyzeProcedure,
							elementType,
							isImplicitArray,
							arrayType,
							baseType,
							defaultTypeValue,
							defaultTypeBin,
							passedByValue,
							alignment,
							storage,
							typeMod,
							typNDims,
							typeNotNull,
							InvalidOid,
							0);
}

/*
 * GenerateTypeDependencies: build the dependencies needed for a type
 *
 * If rebuild is true, we remove existing dependencies and rebuild them
 * from scratch.  This is needed for ALTER TYPE, and also when replacing
 * a shell type.
 */
void
GenerateTypeDependencies(Oid typeNamespace,
						 Oid typeObjectId,
						 Oid relationOid,		/* only for 'c'atalog types */
						 char relationKind,		/* ditto */
						 Oid owner,
						 Oid inputProcedure,
						 Oid outputProcedure,
						 Oid receiveProcedure,
						 Oid sendProcedure,
						 Oid typmodinProcedure,
						 Oid typmodoutProcedure,
						 Oid analyzeProcedure,
						 Oid elementType,
						 bool isImplicitArray,
						 Oid baseType,
						 Node *defaultExpr,
						 bool rebuild)
{
	ObjectAddress myself,
				referenced;

	if (rebuild)
	{
		deleteDependencyRecordsFor(TypeRelationId, typeObjectId);
		deleteSharedDependencyRecordsFor(TypeRelationId, typeObjectId);
	}

	myself.classId = TypeRelationId;
	myself.objectId = typeObjectId;
	myself.objectSubId = 0;

	/*
	 * Make dependency on namespace and shared dependency on owner.
	 *
	 * For a relation rowtype (that's not a composite type), we should skip
	 * these because we'll depend on them indirectly through the pg_class
	 * entry.  Likewise, skip for implicit arrays since we'll depend on them
	 * through the element type.
	 */
	if ((!OidIsValid(relationOid) || relationKind == RELKIND_COMPOSITE_TYPE) &&
		!isImplicitArray)
	{
		referenced.classId = NamespaceRelationId;
		referenced.objectId = typeNamespace;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

		recordDependencyOnOwner(TypeRelationId, typeObjectId, owner);
		/* dependency on extension */
		recordDependencyOnCurrentExtension(&myself, false);
	}

	/* Normal dependencies on the I/O functions */
	if (OidIsValid(inputProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = inputProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	if (OidIsValid(outputProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = outputProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	if (OidIsValid(receiveProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = receiveProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	if (OidIsValid(sendProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = sendProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	if (OidIsValid(typmodinProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = typmodinProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	if (OidIsValid(typmodoutProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = typmodoutProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	if (OidIsValid(analyzeProcedure))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = analyzeProcedure;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/*
	 * If the type is a rowtype for a relation, mark it as internally
	 * dependent on the relation, *unless* it is a stand-alone composite type
	 * relation. For the latter case, we have to reverse the dependency.
	 *
	 * In the former case, this allows the type to be auto-dropped when the
	 * relation is, and not otherwise. And in the latter, of course we get the
	 * opposite effect.
	 */
	if (OidIsValid(relationOid))
	{
		referenced.classId = RelationRelationId;
		referenced.objectId = relationOid;
		referenced.objectSubId = 0;

		if (relationKind != RELKIND_COMPOSITE_TYPE)
			recordDependencyOn(&myself, &referenced, DEPENDENCY_INTERNAL);
		else
			recordDependencyOn(&referenced, &myself, DEPENDENCY_INTERNAL);
	}

	/*
	 * If the type is an implicitly-created array type, mark it as internally
	 * dependent on the element type.  Otherwise, if it has an element type,
	 * the dependency is a normal one.
	 */
	if (OidIsValid(elementType))
	{
		referenced.classId = TypeRelationId;
		referenced.objectId = elementType;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced,
								isImplicitArray ? DEPENDENCY_INTERNAL : DEPENDENCY_NORMAL);
	}

	/* Normal dependency from a domain to its base type. */
	if (OidIsValid(baseType))
	{
		referenced.classId = TypeRelationId;
		referenced.objectId = baseType;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* Normal dependency on the default expression. */
	if (defaultExpr)
		recordDependencyOnExpr(&myself, defaultExpr, NIL, DEPENDENCY_NORMAL);
}

/*
 * TypeRename
 *		This renames a type
 *
 * Note: any associated array type is *not* renamed; caller must make
 * another call to handle that case.  Currently this is only used for
 * renaming types associated with tables, for which there are no arrays.
 */
void
TypeRename(Oid typeOid, const char *newTypeName)
{
	Relation		pg_type_desc;
	HeapTuple		tuple;
	Form_pg_type	form;
	cqContext	   *pcqCtx;
	cqContext		cqc, cqc2;

	pg_type_desc = heap_open(TypeRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), pg_type_desc);

	tuple = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(typeOid)));

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type with OID \"%d\" does not exist", typeOid)));

	form = (Form_pg_type) GETSTRUCT(tuple);
	if (namestrcmp(&(form->typname), newTypeName))
	{
		if (caql_getcount(
					caql_addrel(cqclr(&cqc2), pg_type_desc),
					cql("SELECT COUNT(*) FROM pg_type "
						" WHERE typname = :1 "
						" AND typnamespace = :2 ",
						CStringGetDatum((char *) newTypeName),
						ObjectIdGetDatum(form->typnamespace))))
		{
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("type \"%s\" already exists", newTypeName)));
		}

		namestrcpy(&(form->typname), newTypeName);

		caql_update_current(pcqCtx, tuple);
		/* update the system catalog indexes (implicit) */
	}

	heap_freetuple(tuple);
	heap_close(pg_type_desc, RowExclusiveLock);
}

/*
 * makeArrayTypeName(typeName);
 *	  - given a base type name, make an array type name for it
 *
 * the caller is responsible for pfreeing the result
 */
char *
makeArrayTypeName(const char *typeName, Oid typeNamespace)
{
	char		*arr;
	int			i;
	Relation	pg_type_desc;

	/*
	 * The idea is to prepend underscores as needed until we make a name that
	 * doesn't collide with anything...
	 */
	arr = (char*)palloc(NAMEDATALEN);

	pg_type_desc = heap_open(TypeRelationId, AccessShareLock);

	for (i = 1; i < NAMEDATALEN - 1; i++)
	{
		arr[i - 1] = '_';
		strlcpy(arr + i, typeName, NAMEDATALEN - i);
		truncate_identifier(arr, strlen(arr), false);
		if (!SearchSysCacheExists(TYPENAMENSP,
								  CStringGetDatum(arr),
								  ObjectIdGetDatum(typeNamespace),
								  0, 0))
			break;
	}

	heap_close(pg_type_desc, AccessShareLock);

	if (i >= NAMEDATALEN-1)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("could not form array type name for type \"%s\"",
						typeName)));

	return arr;
}
