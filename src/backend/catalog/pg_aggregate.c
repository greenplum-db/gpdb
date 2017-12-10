/*-------------------------------------------------------------------------
 *
 * pg_aggregate.c
 *	  routines to support manipulation of the pg_aggregate relation
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/pg_aggregate.c,v 1.102 2009/06/11 14:48:55 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_proc_fn.h"
#include "catalog/pg_type.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"


static Oid lookup_agg_function(List *fnName, int nargs, Oid *input_types,
					Oid variadicArgType,
					Oid *rettype);


/*
 * AggregateCreate
 */
void
AggregateCreate(const char *aggName,
				Oid aggNamespace,
				char aggKind,
				int numArgs,
				int numDirectArgs,
				oidvector *parameterTypes,
				Datum allParameterTypes,
				Datum parameterModes,
				Datum parameterNames,
				List *parameterDefaults,
				Oid variadicArgType,
				List *aggtransfnName,
				List *aggprelimfnName,
				List *aggfinalfnName,
				bool finalfnExtraArgs,
				List *aggsortopName,
				Oid aggTransType,
				const char *agginitval)
{
	Relation	aggdesc;
	HeapTuple	tup;
	bool		nulls[Natts_pg_aggregate];
	Datum		values[Natts_pg_aggregate];
	Form_pg_proc proc;
	Oid			transfn;
	Oid			invtransfn = InvalidOid; /* MPP windowing optimization */
	Oid			prelimfn = InvalidOid;	/* if omitted, disables MPP 2-stage for this aggregate */
	Oid			invprelimfn = InvalidOid; /* MPP windowing optimization */
	Oid			finalfn = InvalidOid;	/* can be omitted */
	Oid			sortop = InvalidOid;	/* can be omitted */
	Oid		   *aggArgTypes = parameterTypes->values;
	bool		hasPolyArg;
	bool		hasInternalArg;
	Oid			rettype;
	Oid			finaltype;
	Oid			prelimrettype;
	Oid			fnArgs[FUNC_MAX_ARGS];
	int			nargs_transfn;
	int			nargs_finalfn;
	Oid			procOid;
	TupleDesc	tupDesc;
	int			i;
	ObjectAddress myself,
				referenced;

	/* sanity checks (caller should have caught these) */
	if (!aggName)
		elog(ERROR, "no aggregate name supplied");

	if (!aggtransfnName)
		elog(ERROR, "aggregate must have a transition function");

	if (numDirectArgs < 0 || numDirectArgs > numArgs)
		elog(ERROR, "incorrect number of direct args for aggregate");

	/*
	 * Aggregates can have at most FUNC_MAX_ARGS-1 args, else the transfn
	 * and/or finalfn will be unrepresentable in pg_proc.  We must check now
	 * to protect fixed-size arrays here and possibly in called functions.
	 */
	if (numArgs < 0 || numArgs > FUNC_MAX_ARGS - 1)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_ARGUMENTS),
				 errmsg_plural("aggregates cannot have more than %d argument",
							 "aggregates cannot have more than %d arguments",
							   FUNC_MAX_ARGS - 1,
							   FUNC_MAX_ARGS - 1)));

	/* check for polymorphic and INTERNAL arguments */
	hasPolyArg = false;
	hasInternalArg = false;
	for (i = 0; i < numArgs; i++)
	{
		if (IsPolymorphicType(aggArgTypes[i]))
			hasPolyArg = true;
		else if (aggArgTypes[i] == INTERNALOID)
			hasInternalArg = true;
	}

	/*
	 * If transtype is polymorphic, must have polymorphic argument also; else
	 * we will have no way to deduce the actual transtype.
	 */
	if (IsPolymorphicType(aggTransType) && !hasPolyArg)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("cannot determine transition data type"),
				 errdetail("An aggregate using a polymorphic transition type must have at least one polymorphic argument.")));

	/*
	 * An ordered-set aggregate that is VARIADIC must be VARIADIC ANY.	In
	 * principle we could support regular variadic types, but it would make
	 * things much more complicated because we'd have to assemble the correct
	 * subsets of arguments into array values.	Since no standard aggregates
	 * have use for such a case, we aren't bothering for now.
	 */
	if (AGGKIND_IS_ORDERED_SET(aggKind) && OidIsValid(variadicArgType) &&
		variadicArgType != ANYOID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("a variadic ordered-set aggregate must use VARIADIC type ANY")));

	/*
	 * If it's a hypothetical-set aggregate, there must be at least as many
	 * direct arguments as aggregated ones, and the last N direct arguments
	 * must match the aggregated ones in type.	(We have to check this again
	 * when the aggregate is called, in case ANY is involved, but it makes
	 * sense to reject the aggregate definition now if the declared arg types
	 * don't match up.)  It's unconditionally OK if numDirectArgs == numArgs,
	 * indicating that the grammar merged identical VARIADIC entries from both
	 * lists.  Otherwise, if the agg is VARIADIC, then we had VARIADIC only on
	 * the aggregated side, which is not OK.  Otherwise, insist on the last N
	 * parameter types on each side matching exactly.
	 */
	if (aggKind == AGGKIND_HYPOTHETICAL &&
		numDirectArgs < numArgs)
	{
		int			numAggregatedArgs = numArgs - numDirectArgs;

		if (OidIsValid(variadicArgType) ||
			numDirectArgs < numAggregatedArgs ||
			memcmp(aggArgTypes + (numDirectArgs - numAggregatedArgs),
				   aggArgTypes + numDirectArgs,
				   numAggregatedArgs * sizeof(Oid)) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					 errmsg("a hypothetical-set aggregate must have direct arguments matching its aggregated arguments")));
	}

	/*
	 * Find the transfn.  For ordinary aggs, it takes the transtype plus all
	 * aggregate arguments.  For ordered-set aggs, it takes the transtype plus
	 * all aggregated args, but not direct args.  However, we have to treat
	 * specially the case where a trailing VARIADIC item is considered to
	 * cover both direct and aggregated args.
	 */
	if (AGGKIND_IS_ORDERED_SET(aggKind))
	{
		if (numDirectArgs < numArgs)
			nargs_transfn = numArgs - numDirectArgs + 1;
		else
		{
			/* special case with VARIADIC last arg */
			Assert(variadicArgType != InvalidOid);
			nargs_transfn = 2;
		}
		fnArgs[0] = aggTransType;
		memcpy(fnArgs + 1, aggArgTypes + (numArgs - (nargs_transfn - 1)),
			   (nargs_transfn - 1) * sizeof(Oid));
	}
	else
	{
		nargs_transfn = numArgs + 1;
		fnArgs[0] = aggTransType;
		memcpy(fnArgs + 1, aggArgTypes, numArgs * sizeof(Oid));
	}
	transfn = lookup_agg_function(aggtransfnName, nargs_transfn,
								  fnArgs, variadicArgType,
								  &rettype);

	/*
	 * Return type of transfn (possibly after refinement by
	 * enforce_generic_type_consistency, if transtype isn't polymorphic) must
	 * exactly match declared transtype.
	 *
	 * In the non-polymorphic-transtype case, it might be okay to allow a
	 * rettype that's binary-coercible to transtype, but I'm not quite
	 * convinced that it's either safe or useful.  When transtype is
	 * polymorphic we *must* demand exact equality.
	 */
	if (rettype != aggTransType)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("return type of transition function %s is not %s",
						NameListToString(aggtransfnName),
						format_type_be(aggTransType))));

	tup = SearchSysCache(PROCOID,
						 ObjectIdGetDatum(transfn),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for function %u", transfn);
	proc = (Form_pg_proc) GETSTRUCT(tup);

	/*
	 * If the transfn is strict and the initval is NULL, make sure first input
	 * type and transtype are the same (or at least binary-compatible), so
	 * that it's OK to use the first input value as the initial transValue.
	 */
	if (proc->proisstrict && agginitval == NULL)
	{
		if (numArgs < 1 ||
			!IsBinaryCoercible(aggArgTypes[0], aggTransType))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					 errmsg("must not omit initial value when transition function is strict and transition type is not compatible with input type")));
	}
	ReleaseSysCache(tup);
	
	/* handle prelimfn, if supplied */
	if (aggprelimfnName)
	{
		/* 
		 * The preliminary state function (pfunc) input arguments are the results of the 
		 * state transition function (sfunc) and therefore must be of the same types.
		 */
		fnArgs[0] = rettype;
		fnArgs[1] = rettype;
		
		/*
		 * Check that such a function name and prototype exists in the catalog.
		 */		
		prelimfn = lookup_agg_function(aggprelimfnName, 2,
									   fnArgs, variadicArgType,
									   &prelimrettype);
		
		elog(DEBUG5,"AggregateCreateWithOid: successfully located preliminary "
					"function %s with return type %d", 
					func_signature_string(aggprelimfnName, 2, fnArgs), 
					prelimrettype);
		
		Assert(OidIsValid(prelimrettype));
		
		/*
		 * The preliminary return type must be of the same type as the internal 
		 * state. (See similar error checking for transition types above)
		 */
		if (prelimrettype != rettype)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("return type of preliminary function %s is not %s",
							NameListToString(aggprelimfnName),
							format_type_be(rettype))));		
	}

	/* handle finalfn, if supplied */
	if (aggfinalfnName)
	{
		/*
		 * If finalfnExtraArgs is specified, the transfn takes the transtype
		 * plus all args; otherwise, it just takes the transtype plus any
		 * direct args.  (Non-direct args are useless at runtime, and are
		 * actually passed as NULLs, but we may need them in the function
		 * signature to allow resolution of a polymorphic agg's result type.)
		 */
		Oid			ffnVariadicArgType = variadicArgType;

		fnArgs[0] = aggTransType;
		memcpy(fnArgs + 1, aggArgTypes, numArgs * sizeof(Oid));
		if (finalfnExtraArgs)
			nargs_finalfn = numArgs + 1;
		else
		{
			nargs_finalfn = numDirectArgs + 1;
			if (numDirectArgs < numArgs)
			{
				/* variadic argument doesn't affect finalfn */
				ffnVariadicArgType = InvalidOid;
			}
		}

		finalfn = lookup_agg_function(aggfinalfnName, nargs_finalfn,
									  fnArgs, ffnVariadicArgType,
									  &finaltype);

		/*
		 * When finalfnExtraArgs is specified, the finalfn will certainly be
		 * passed at least one null argument, so complain if it's strict.
		 * Nothing bad would happen at runtime (you'd just get a null result),
		 * but it's surely not what the user wants, so let's complain now.
		 */
		if (finalfnExtraArgs && func_strict(finalfn))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					 errmsg("final function with extra arguments must not be declared STRICT")));
	}
	else
	{
		/*
		 * If no finalfn, aggregate result type is type of the state value
		 */
		finaltype = aggTransType;
	}
	Assert(OidIsValid(finaltype));

	/*
	 * If finaltype (i.e. aggregate return type) is polymorphic, inputs must
	 * be polymorphic also, else parser will fail to deduce result type.
	 * (Note: given the previous test on transtype and inputs, this cannot
	 * happen, unless someone has snuck a finalfn definition into the catalogs
	 * that itself violates the rule against polymorphic result with no
	 * polymorphic input.)
	 */
	if (IsPolymorphicType(finaltype) && !hasPolyArg)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot determine result data type"),
				 errdetail("An aggregate returning a polymorphic type "
						   "must have at least one polymorphic argument.")));

	/*
	 * Also, the return type can't be INTERNAL unless there's at least one
	 * INTERNAL argument.  This is the same type-safety restriction we enforce
	 * for regular functions, but at the level of aggregates.  We must test
	 * this explicitly because we allow INTERNAL as the transtype.
	 */
	if (finaltype == INTERNALOID && !hasInternalArg)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("unsafe use of pseudo-type \"internal\""),
				 errdetail("A function returning \"internal\" must have at least one \"internal\" argument.")));

	/* handle sortop, if supplied */
	if (aggsortopName)
	{
		if (numArgs != 1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					 errmsg("sort operator can only be specified for single-argument aggregates")));
		sortop = LookupOperName(NULL, aggsortopName,
								aggArgTypes[0], aggArgTypes[0],
								false, -1);
	}

	/*
	 * Everything looks okay.  Try to create the pg_proc entry for the
	 * aggregate.  (This could fail if there's already a conflicting entry.)
	 */

	procOid = ProcedureCreate(aggName,
							  aggNamespace,
							  false,	/* no replacement */
							  false,	/* doesn't return a set */
							  finaltype,		/* returnType */
							  INTERNALlanguageId,		/* languageObjectId */
							  InvalidOid,		/* no validator */
							  InvalidOid,		/* no describe function */
							  "aggregate_dummy",		/* placeholder proc */
							  NULL,		/* probin */
							  true,		/* isAgg */
							  false,	/* isWindowFunc */
							  false,	/* security invoker (currently not
										 * definable for agg) */
							  false,	/* isStrict (not needed for agg) */
							  PROVOLATILE_IMMUTABLE,	/* volatility (not
														 * needed for agg) */
							  parameterTypes,	/* paramTypes */
							  allParameterTypes,		/* allParamTypes */
							  parameterModes,	/* parameterModes */
							  parameterNames,	/* parameterNames */
							  parameterDefaults,		/* parameterDefaults */
							  PointerGetDatum(NULL),	/* proconfig */
							  1,				/* procost */
							  0,				/* prorows */
							  PRODATAACCESS_NONE,		/* prodataaccess */
							  PROEXECLOCATION_ANY);		/* proexeclocation */

	/*
	 * Okay to create the pg_aggregate entry.
	 */

	/* initialize nulls and values */
	for (i = 0; i < Natts_pg_aggregate; i++)
	{
		nulls[i] = false;
		values[i] = (Datum) NULL;
	}
	values[Anum_pg_aggregate_aggfnoid - 1] = ObjectIdGetDatum(procOid);
	values[Anum_pg_aggregate_aggkind - 1] = CharGetDatum(aggKind);
	values[Anum_pg_aggregate_aggnumdirectargs - 1] = Int16GetDatum(numDirectArgs);
	values[Anum_pg_aggregate_aggtransfn - 1] = ObjectIdGetDatum(transfn);
	values[Anum_pg_aggregate_agginvtransfn - 1] = ObjectIdGetDatum(invtransfn); 
	values[Anum_pg_aggregate_aggprelimfn - 1] = ObjectIdGetDatum(prelimfn);
	values[Anum_pg_aggregate_agginvprelimfn - 1] = ObjectIdGetDatum(invprelimfn);
	values[Anum_pg_aggregate_aggfinalfn - 1] = ObjectIdGetDatum(finalfn);
	values[Anum_pg_aggregate_aggfinalextra - 1] = BoolGetDatum(finalfnExtraArgs);
	values[Anum_pg_aggregate_aggsortop - 1] = ObjectIdGetDatum(sortop);
	values[Anum_pg_aggregate_aggtranstype - 1] = ObjectIdGetDatum(aggTransType);
	if (agginitval)
		values[Anum_pg_aggregate_agginitval - 1] = CStringGetTextDatum(agginitval);
	else
		nulls[Anum_pg_aggregate_agginitval - 1] = true;

	aggdesc = heap_open(AggregateRelationId, RowExclusiveLock);
	tupDesc = aggdesc->rd_att;

	tup = heap_form_tuple(tupDesc, values, nulls);
	simple_heap_insert(aggdesc, tup);

	CatalogUpdateIndexes(aggdesc, tup);

	heap_close(aggdesc, RowExclusiveLock);

	/*
	 * Create dependencies for the aggregate (above and beyond those already
	 * made by ProcedureCreate).  Note: we don't need an explicit dependency
	 * on aggTransType since we depend on it indirectly through transfn.
	 */
	myself.classId = ProcedureRelationId;
	myself.objectId = procOid;
	myself.objectSubId = 0;

	/* Depends on transition function */
	referenced.classId = ProcedureRelationId;
	referenced.objectId = transfn;
	referenced.objectSubId = 0;
	recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

	/* Depends on inverse transition function, if any */
	if (OidIsValid(invtransfn))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = invtransfn;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* Depends on preliminary aggregation function, if any */
	if (OidIsValid(prelimfn))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = prelimfn;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* Depends on inverse preliminary aggregation function, if any */
	if (OidIsValid(invprelimfn))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = invprelimfn;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* Depends on final function, if any */
	if (OidIsValid(finalfn))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = finalfn;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* Depends on sort operator, if any */
	if (OidIsValid(sortop))
	{
		referenced.classId = OperatorRelationId;
		referenced.objectId = sortop;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}
}

/*
 * lookup_agg_function -- common code for finding transfn, prelimfn and finalfn
 */
static Oid
lookup_agg_function(List *fnName,
					int nargs,
					Oid *input_types,
					Oid variadicArgType,
					Oid *rettype)
{
	Oid			fnOid;
	bool		retset;
	int			nvargs;
	Oid			vatype;
	Oid		   *true_oid_array;
	FuncDetailCode fdresult;
	AclResult	aclresult;
	int			i;

	/*
	 * func_get_detail looks up the function in the catalogs, does
	 * disambiguation for polymorphic functions, handles inheritance, and
	 * returns the funcid and type and set or singleton status of the
	 * function's return value.  it also returns the true argument types to
	 * the function.
	 */
	fdresult = func_get_detail(fnName, NIL, nargs, input_types, false, false,
							   &fnOid, rettype, &retset,
							   &nvargs, &vatype,
							   &true_oid_array, NULL);

	/* only valid case is a normal function not returning a set */
	if (fdresult != FUNCDETAIL_NORMAL || !OidIsValid(fnOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function %s does not exist",
						func_signature_string(fnName, nargs, input_types))));
	if (retset)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("function %s returns a set",
						func_signature_string(fnName, nargs, input_types))));

	/*
	 * If the agg is declared to take VARIADIC ANY, the underlying functions
	 * had better be declared that way too, else they may receive too many
	 * parameters; but func_get_detail would have been happy with plain ANY.
	 * (Probably nothing very bad would happen, but it wouldn't work as the
	 * user expects.)  Other combinations should work without any special
	 * pushups, given that we told func_get_detail not to expand VARIADIC.
	 */
	if (variadicArgType == ANYOID && vatype != ANYOID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("function %s must accept VARIADIC ANY to be used in this aggregate",
						func_signature_string(fnName, nargs,
											  input_types))));

	/*
	 * If there are any polymorphic types involved, enforce consistency, and
	 * possibly refine the result type.  It's OK if the result is still
	 * polymorphic at this point, though.
	 */
	*rettype = enforce_generic_type_consistency(input_types,
												true_oid_array,
												nargs,
												*rettype,
												true);

	/*
	 * func_get_detail will find functions requiring run-time argument type
	 * coercion, but nodeAgg.c isn't prepared to deal with that
	 */
	for (i = 0; i < nargs; i++)
	{
		if (!IsBinaryCoercible(input_types[i], true_oid_array[i]))
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("function %s requires run-time type coercion",
					 func_signature_string(fnName, nargs, true_oid_array))));
	}

	/* Check aggregate creator has permission to call the function */
	aclresult = pg_proc_aclcheck(fnOid, GetUserId(), ACL_EXECUTE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_PROC, get_func_name(fnOid));

	return fnOid;
}
