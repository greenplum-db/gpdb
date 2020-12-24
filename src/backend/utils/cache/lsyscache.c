/*-------------------------------------------------------------------------
 *
 * lsyscache.c
 *	  Convenience routines for common queries in the system catalog cache.
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/lsyscache.c
 *
 * NOTES
 *	  Eventually, the index information should go through here, too.
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/table.h"
#include "bootstrap/bootstrap.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_range.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_type.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_clause.h"			/* for sort_op_can_sort() */
#include "parser/parse_coerce.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "utils/fmgroids.h"
#include "funcapi.h"
#include "cdb/cdbhash.h"

#include "catalog/heap.h"                   /* SystemAttributeDefinition() */
#include "catalog/pg_aggregate.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_trigger.h"

/* Hook for plugins to get control in get_attavgwidth() */
get_attavgwidth_hook_type get_attavgwidth_hook = NULL;


/*				---------- AMOP CACHES ----------						 */

/*
 * op_in_opfamily
 *
 *		Return t iff operator 'opno' is in operator family 'opfamily'.
 *
 * This function only considers search operators, not ordering operators.
 */
bool
op_in_opfamily(Oid opno, Oid opfamily)
{
	return SearchSysCacheExists3(AMOPOPID,
								 ObjectIdGetDatum(opno),
								 CharGetDatum(AMOP_SEARCH),
								 ObjectIdGetDatum(opfamily));
}

/*
 * get_op_opfamily_strategy
 *
 *		Get the operator's strategy number within the specified opfamily,
 *		or 0 if it's not a member of the opfamily.
 *
 * This function only considers search operators, not ordering operators.
 */
int
get_op_opfamily_strategy(Oid opno, Oid opfamily)
{
	HeapTuple	tp;
	Form_pg_amop amop_tup;
	int			result;

	tp = SearchSysCache3(AMOPOPID,
						 ObjectIdGetDatum(opno),
						 CharGetDatum(AMOP_SEARCH),
						 ObjectIdGetDatum(opfamily));
	if (!HeapTupleIsValid(tp))
		return 0;
	amop_tup = (Form_pg_amop) GETSTRUCT(tp);
	result = amop_tup->amopstrategy;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_op_opfamily_sortfamily
 *
 *		If the operator is an ordering operator within the specified opfamily,
 *		return its amopsortfamily OID; else return InvalidOid.
 */
Oid
get_op_opfamily_sortfamily(Oid opno, Oid opfamily)
{
	HeapTuple	tp;
	Form_pg_amop amop_tup;
	Oid			result;

	tp = SearchSysCache3(AMOPOPID,
						 ObjectIdGetDatum(opno),
						 CharGetDatum(AMOP_ORDER),
						 ObjectIdGetDatum(opfamily));
	if (!HeapTupleIsValid(tp))
		return InvalidOid;
	amop_tup = (Form_pg_amop) GETSTRUCT(tp);
	result = amop_tup->amopsortfamily;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_op_opfamily_properties
 *
 *		Get the operator's strategy number and declared input data types
 *		within the specified opfamily.
 *
 * Caller should already have verified that opno is a member of opfamily,
 * therefore we raise an error if the tuple is not found.
 */
void
get_op_opfamily_properties(Oid opno, Oid opfamily, bool ordering_op,
						   int *strategy,
						   Oid *lefttype,
						   Oid *righttype)
{
	HeapTuple	tp;
	Form_pg_amop amop_tup;

	tp = SearchSysCache3(AMOPOPID,
						 ObjectIdGetDatum(opno),
						 CharGetDatum(ordering_op ? AMOP_ORDER : AMOP_SEARCH),
						 ObjectIdGetDatum(opfamily));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "operator %u is not a member of opfamily %u",
			 opno, opfamily);
	amop_tup = (Form_pg_amop) GETSTRUCT(tp);
	*strategy = amop_tup->amopstrategy;
	*lefttype = amop_tup->amoplefttype;
	*righttype = amop_tup->amoprighttype;
	ReleaseSysCache(tp);
}

/*
 * get_opfamily_member
 *		Get the OID of the operator that implements the specified strategy
 *		with the specified datatypes for the specified opfamily.
 *
 * Returns InvalidOid if there is no pg_amop entry for the given keys.
 */
Oid
get_opfamily_member(Oid opfamily, Oid lefttype, Oid righttype,
					int16 strategy)
{
	HeapTuple	tp;
	Form_pg_amop amop_tup;
	Oid			result;

	tp = SearchSysCache4(AMOPSTRATEGY,
						 ObjectIdGetDatum(opfamily),
						 ObjectIdGetDatum(lefttype),
						 ObjectIdGetDatum(righttype),
						 Int16GetDatum(strategy));
	if (!HeapTupleIsValid(tp))
		return InvalidOid;
	amop_tup = (Form_pg_amop) GETSTRUCT(tp);
	result = amop_tup->amopopr;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_ordering_op_properties
 *		Given the OID of an ordering operator (a btree "<" or ">" operator),
 *		determine its opfamily, its declared input datatype, and its
 *		strategy number (BTLessStrategyNumber or BTGreaterStrategyNumber).
 *
 * Returns true if successful, false if no matching pg_amop entry exists.
 * (This indicates that the operator is not a valid ordering operator.)
 *
 * Note: the operator could be registered in multiple families, for example
 * if someone were to build a "reverse sort" opfamily.  This would result in
 * uncertainty as to whether "ORDER BY USING op" would default to NULLS FIRST
 * or NULLS LAST, as well as inefficient planning due to failure to match up
 * pathkeys that should be the same.  So we want a determinate result here.
 * Because of the way the syscache search works, we'll use the interpretation
 * associated with the opfamily with smallest OID, which is probably
 * determinate enough.  Since there is no longer any particularly good reason
 * to build reverse-sort opfamilies, it doesn't seem worth expending any
 * additional effort on ensuring consistency.
 */
bool
get_ordering_op_properties(Oid opno,
						   Oid *opfamily, Oid *opcintype, int16 *strategy)
{
	bool		result = false;
	CatCList   *catlist;
	int			i;

	/* ensure outputs are initialized on failure */
	*opfamily = InvalidOid;
	*opcintype = InvalidOid;
	*strategy = 0;

	/*
	 * Search pg_amop to see if the target operator is registered as the "<"
	 * or ">" operator of any btree opfamily.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		/* must be btree */
		if (aform->amopmethod != BTREE_AM_OID)
			continue;

		if (aform->amopstrategy == BTLessStrategyNumber ||
			aform->amopstrategy == BTGreaterStrategyNumber)
		{
			/* Found it ... should have consistent input types */
			if (aform->amoplefttype == aform->amoprighttype)
			{
				/* Found a suitable opfamily, return info */
				*opfamily = aform->amopfamily;
				*opcintype = aform->amoplefttype;
				*strategy = aform->amopstrategy;
				result = true;
				break;
			}
		}
	}

	ReleaseSysCacheList(catlist);

	return result;
}

/*
 * get_compare_function_for_ordering_op
 *		Get the OID of the datatype-specific btree comparison function
 *		associated with an ordering operator (a "<" or ">" operator).
 *
 * *cmpfunc receives the comparison function OID.
 * *reverse is set FALSE if the operator is "<", TRUE if it's ">"
 * (indicating the comparison result must be negated before use).
 *
 * Returns TRUE if successful, FALSE if no btree function can be found.
 * (This indicates that the operator is not a valid ordering operator.)
 */
bool
get_compare_function_for_ordering_op(Oid opno, Oid *cmpfunc, bool *reverse)
{
	Oid			opfamily;
	Oid			opcintype;
	int16		strategy;

	/* Find the operator in pg_amop */
	if (get_ordering_op_properties(opno,
								   &opfamily, &opcintype, &strategy))
	{
		/* Found a suitable opfamily, get matching support function */
		*cmpfunc = get_opfamily_proc(opfamily,
									 opcintype,
									 opcintype,
									 BTORDER_PROC);

		if (!OidIsValid(*cmpfunc))		/* should not happen */
			elog(ERROR, "missing support function %d(%u,%u) in opfamily %u",
				 BTORDER_PROC, opcintype, opcintype, opfamily);
		*reverse = (strategy == BTGreaterStrategyNumber);
		return true;
	}

	/* ensure outputs are set on failure */
	*cmpfunc = InvalidOid;

	*reverse = false;
	return false;
}


/*
 * get_equality_op_for_ordering_op
 *		Get the OID of the datatype-specific btree equality operator
 *		associated with an ordering operator (a "<" or ">" operator).
 *
 * If "reverse" isn't NULL, also set *reverse to false if the operator is "<",
 * true if it's ">"
 *
 * Returns InvalidOid if no matching equality operator can be found.
 * (This indicates that the operator is not a valid ordering operator.)
 */
Oid
get_equality_op_for_ordering_op(Oid opno, bool *reverse)
{
	Oid			result = InvalidOid;
	Oid			opfamily;
	Oid			opcintype;
	int16		strategy;

	/* Find the operator in pg_amop */
	if (get_ordering_op_properties(opno,
								   &opfamily, &opcintype, &strategy))
	{
		/* Found a suitable opfamily, get matching equality operator */
		result = get_opfamily_member(opfamily,
									 opcintype,
									 opcintype,
									 BTEqualStrategyNumber);
		if (reverse)
			*reverse = (strategy == BTGreaterStrategyNumber);
	}

	return result;
}

/*
 * get_ordering_op_for_equality_op
 *		Get the OID of a datatype-specific btree ordering operator
 *		associated with an equality operator.  (If there are multiple
 *		possibilities, assume any one will do.)
 *
 * This function is used when we have to sort data before unique-ifying,
 * and don't much care which sorting op is used as long as it's compatible
 * with the intended equality operator.  Since we need a sorting operator,
 * it should be single-data-type even if the given operator is cross-type.
 * The caller specifies whether to find an op for the LHS or RHS data type.
 *
 * Returns InvalidOid if no matching ordering operator can be found.
 */
Oid
get_ordering_op_for_equality_op(Oid opno, bool use_lhs_type)
{
	Oid			result = InvalidOid;
	CatCList   *catlist;
	int			i;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any btree opfamily.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		/* must be btree */
		if (aform->amopmethod != BTREE_AM_OID)
			continue;

		if (aform->amopstrategy == BTEqualStrategyNumber)
		{
			/* Found a suitable opfamily, get matching ordering operator */
			Oid			typid;

			typid = use_lhs_type ? aform->amoplefttype : aform->amoprighttype;
			result = get_opfamily_member(aform->amopfamily,
										 typid, typid,
										 BTLessStrategyNumber);
			if (OidIsValid(result))
				break;
			/* failure probably shouldn't happen, but keep looking if so */
		}
	}

	ReleaseSysCacheList(catlist);

	return result;
}

/*
 * get_mergejoin_opfamilies
 *		Given a putatively mergejoinable operator, return a list of the OIDs
 *		of the btree opfamilies in which it represents equality.
 *
 * It is possible (though at present unusual) for an operator to be equality
 * in more than one opfamily, hence the result is a list.  This also lets us
 * return NIL if the operator is not found in any opfamilies.
 *
 * The planner currently uses simple equal() tests to compare the lists
 * returned by this function, which makes the list order relevant, though
 * strictly speaking it should not be.  Because of the way syscache list
 * searches are handled, in normal operation the result will be sorted by OID
 * so everything works fine.  If running with system index usage disabled,
 * the result ordering is unspecified and hence the planner might fail to
 * recognize optimization opportunities ... but that's hardly a scenario in
 * which performance is good anyway, so there's no point in expending code
 * or cycles here to guarantee the ordering in that case.
 */
List *
get_mergejoin_opfamilies(Oid opno)
{
	List	   *result = NIL;
	CatCList   *catlist;
	int			i;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any btree opfamily.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		/* must be btree equality */
		if (aform->amopmethod == BTREE_AM_OID &&
			aform->amopstrategy == BTEqualStrategyNumber)
			result = lappend_oid(result, aform->amopfamily);
	}

	ReleaseSysCacheList(catlist);

	return result;
}

/*
 * get_compatible_hash_operators
 *		Get the OID(s) of hash equality operator(s) compatible with the given
 *		operator, but operating on its LHS and/or RHS datatype.
 *
 * An operator for the LHS type is sought and returned into *lhs_opno if
 * lhs_opno isn't NULL.  Similarly, an operator for the RHS type is sought
 * and returned into *rhs_opno if rhs_opno isn't NULL.
 *
 * If the given operator is not cross-type, the results should be the same
 * operator, but in cross-type situations they will be different.
 *
 * Returns true if able to find the requested operator(s), false if not.
 * (This indicates that the operator should not have been marked oprcanhash.)
 */
bool
get_compatible_hash_operators(Oid opno,
							  Oid *lhs_opno, Oid *rhs_opno)
{
	return get_compatible_hash_operators_and_family(opno, lhs_opno, rhs_opno,
													NULL);
}

/*
 * Extended version of get_compatible_hash_operators, which also returns
 * the operator family that the returned operators belong to.
 */
bool
get_compatible_hash_operators_and_family(Oid opno,
										 Oid *lhs_opno, Oid *rhs_opno,
										 Oid *opfamily)
{
	bool		result = false;
	CatCList   *catlist;
	int			i;
	Oid			this_opfamily = InvalidOid;

	/* Ensure output args are initialized on failure */
	if (lhs_opno)
		*lhs_opno = InvalidOid;
	if (rhs_opno)
		*rhs_opno = InvalidOid;
	if (opfamily)
		*opfamily = InvalidOid;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any hash opfamily.  If the operator is registered in
	 * multiple opfamilies, assume we can use any one.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		this_opfamily = aform->amopfamily;

		if (aform->amopmethod == HASH_AM_OID &&
			aform->amopstrategy == HTEqualStrategyNumber)
		{
			/* No extra lookup needed if given operator is single-type */
			if (aform->amoplefttype == aform->amoprighttype)
			{
				if (lhs_opno)
					*lhs_opno = opno;
				if (rhs_opno)
					*rhs_opno = opno;
				result = true;
				break;
			}

			/*
			 * Get the matching single-type operator(s).  Failure probably
			 * shouldn't happen --- it implies a bogus opfamily --- but
			 * continue looking if so.
			 */
			if (lhs_opno)
			{
				*lhs_opno = get_opfamily_member(aform->amopfamily,
												aform->amoplefttype,
												aform->amoplefttype,
												HTEqualStrategyNumber);
				if (!OidIsValid(*lhs_opno))
					continue;
				/* Matching LHS found, done if caller doesn't want RHS */
				if (!rhs_opno)
				{
					result = true;
					break;
				}
			}
			if (rhs_opno)
			{
				*rhs_opno = get_opfamily_member(aform->amopfamily,
												aform->amoprighttype,
												aform->amoprighttype,
												HTEqualStrategyNumber);
				if (!OidIsValid(*rhs_opno))
				{
					/* Forget any LHS operator from this opfamily */
					if (lhs_opno)
						*lhs_opno = InvalidOid;
					continue;
				}
				/* Matching RHS found, so done */
				result = true;
				break;
			}
		}
	}

	if (result && opfamily)
		*opfamily = this_opfamily;

	ReleaseSysCacheList(catlist);

	return result;
}

Oid
get_compatible_hash_opfamily(Oid opno)
{
	Oid			result = InvalidOid;
	CatCList   *catlist;
	int			i;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any hash opfamily.  If the operator is registered in
	 * multiple opfamilies, assume we can use any one.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		if (aform->amopmethod == HASH_AM_OID &&
			aform->amopstrategy == HTEqualStrategyNumber)
		{
			result = aform->amopfamily;
			break;
		}
	}

	ReleaseSysCacheList(catlist);

	return result;
}

Oid
get_compatible_legacy_hash_opfamily(Oid opno)
{
	Oid			result = InvalidOid;
	CatCList   *catlist;
	int			i;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any hash opfamily.  If the operator is registered in
	 * multiple opfamilies, assume we can use any one.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		if (aform->amopmethod == HASH_AM_OID &&
			aform->amopstrategy == HTEqualStrategyNumber)
		{
			Oid hashfunc = cdb_hashproc_in_opfamily(aform->amopfamily, aform->amoplefttype);
			if (isLegacyCdbHashFunction(hashfunc))
			{
				result = aform->amopfamily;
				break;
			}
		}
	}

	ReleaseSysCacheList(catlist);

	return result;
}

/*
 * get_op_hash_functions
 *		Get the OID(s) of the standard hash support function(s) compatible with
 *		the given operator, operating on its LHS and/or RHS datatype as required.
 *
 * A function for the LHS type is sought and returned into *lhs_procno if
 * lhs_procno isn't NULL.  Similarly, a function for the RHS type is sought
 * and returned into *rhs_procno if rhs_procno isn't NULL.
 *
 * If the given operator is not cross-type, the results should be the same
 * function, but in cross-type situations they will be different.
 *
 * Returns true if able to find the requested function(s), false if not.
 * (This indicates that the operator should not have been marked oprcanhash.)
 */
bool
get_op_hash_functions(Oid opno,
					  RegProcedure *lhs_procno, RegProcedure *rhs_procno)
{
	bool		result = false;
	CatCList   *catlist;
	int			i;

	/* Ensure output args are initialized on failure */
	if (lhs_procno)
		*lhs_procno = InvalidOid;
	if (rhs_procno)
		*rhs_procno = InvalidOid;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any hash opfamily.  If the operator is registered in
	 * multiple opfamilies, assume we can use any one.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		if (aform->amopmethod == HASH_AM_OID &&
			aform->amopstrategy == HTEqualStrategyNumber)
		{
			/*
			 * Get the matching support function(s).  Failure probably
			 * shouldn't happen --- it implies a bogus opfamily --- but
			 * continue looking if so.
			 */
			if (lhs_procno)
			{
				*lhs_procno = get_opfamily_proc(aform->amopfamily,
												aform->amoplefttype,
												aform->amoplefttype,
												HASHSTANDARD_PROC);
				if (!OidIsValid(*lhs_procno))
					continue;
				/* Matching LHS found, done if caller doesn't want RHS */
				if (!rhs_procno)
				{
					result = true;
					break;
				}
				/* Only one lookup needed if given operator is single-type */
				if (aform->amoplefttype == aform->amoprighttype)
				{
					*rhs_procno = *lhs_procno;
					result = true;
					break;
				}
			}
			if (rhs_procno)
			{
				*rhs_procno = get_opfamily_proc(aform->amopfamily,
												aform->amoprighttype,
												aform->amoprighttype,
												HASHSTANDARD_PROC);
				if (!OidIsValid(*rhs_procno))
				{
					/* Forget any LHS function from this opfamily */
					if (lhs_procno)
						*lhs_procno = InvalidOid;
					continue;
				}
				/* Matching RHS found, so done */
				result = true;
				break;
			}
		}
	}

	ReleaseSysCacheList(catlist);

	return result;
}

/*
 * get_op_btree_interpretation
 *		Given an operator's OID, find out which btree opfamilies it belongs to,
 *		and what properties it has within each one.  The results are returned
 *		as a palloc'd list of OpBtreeInterpretation structs.
 *
 * In addition to the normal btree operators, we consider a <> operator to be
 * a "member" of an opfamily if its negator is an equality operator of the
 * opfamily.  ROWCOMPARE_NE is returned as the strategy number for this case.
 */
List *
get_op_btree_interpretation(Oid opno)
{
	List	   *result = NIL;
	OpBtreeInterpretation *thisresult;
	CatCList   *catlist;
	int			i;

	/*
	 * Find all the pg_amop entries containing the operator.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	op_tuple = &catlist->members[i]->tuple;
		Form_pg_amop op_form = (Form_pg_amop) GETSTRUCT(op_tuple);
		StrategyNumber op_strategy;

		/* must be btree */
		if (op_form->amopmethod != BTREE_AM_OID)
			continue;

		/* Get the operator's btree strategy number */
		op_strategy = (StrategyNumber) op_form->amopstrategy;
		Assert(op_strategy >= 1 && op_strategy <= 5);

		thisresult = (OpBtreeInterpretation *)
			palloc(sizeof(OpBtreeInterpretation));
		thisresult->opfamily_id = op_form->amopfamily;
		thisresult->strategy = op_strategy;
		thisresult->oplefttype = op_form->amoplefttype;
		thisresult->oprighttype = op_form->amoprighttype;
		result = lappend(result, thisresult);
	}

	ReleaseSysCacheList(catlist);

	/*
	 * If we didn't find any btree opfamily containing the operator, perhaps
	 * it is a <> operator.  See if it has a negator that is in an opfamily.
	 */
	if (result == NIL)
	{
		Oid			op_negator = get_negator(opno);

		if (OidIsValid(op_negator))
		{
			catlist = SearchSysCacheList1(AMOPOPID,
										  ObjectIdGetDatum(op_negator));

			for (i = 0; i < catlist->n_members; i++)
			{
				HeapTuple	op_tuple = &catlist->members[i]->tuple;
				Form_pg_amop op_form = (Form_pg_amop) GETSTRUCT(op_tuple);
				StrategyNumber op_strategy;

				/* must be btree */
				if (op_form->amopmethod != BTREE_AM_OID)
					continue;

				/* Get the operator's btree strategy number */
				op_strategy = (StrategyNumber) op_form->amopstrategy;
				Assert(op_strategy >= 1 && op_strategy <= 5);

				/* Only consider negators that are = */
				if (op_strategy != BTEqualStrategyNumber)
					continue;

				/* OK, report it with "strategy" ROWCOMPARE_NE */
				thisresult = (OpBtreeInterpretation *)
					palloc(sizeof(OpBtreeInterpretation));
				thisresult->opfamily_id = op_form->amopfamily;
				thisresult->strategy = ROWCOMPARE_NE;
				thisresult->oplefttype = op_form->amoplefttype;
				thisresult->oprighttype = op_form->amoprighttype;
				result = lappend(result, thisresult);
			}

			ReleaseSysCacheList(catlist);
		}
	}

	return result;
}

/*
 * equality_ops_are_compatible
 *		Return true if the two given equality operators have compatible
 *		semantics.
 *
 * This is trivially true if they are the same operator.  Otherwise,
 * we look to see if they can be found in the same btree or hash opfamily.
 * Either finding allows us to assume that they have compatible notions
 * of equality.  (The reason we need to do these pushups is that one might
 * be a cross-type operator; for instance int24eq vs int4eq.)
 */
bool
equality_ops_are_compatible(Oid opno1, Oid opno2)
{
	bool		result;
	CatCList   *catlist;
	int			i;

	/* Easy if they're the same operator */
	if (opno1 == opno2)
		return true;

	/*
	 * We search through all the pg_amop entries for opno1.
	 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno1));

	result = false;
	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	op_tuple = &catlist->members[i]->tuple;
		Form_pg_amop op_form = (Form_pg_amop) GETSTRUCT(op_tuple);

		/* must be btree or hash */
		if (op_form->amopmethod == BTREE_AM_OID ||
			op_form->amopmethod == HASH_AM_OID)
		{
			if (op_in_opfamily(opno2, op_form->amopfamily))
			{
				result = true;
				break;
			}
		}
	}

	ReleaseSysCacheList(catlist);

	return result;
}


/*				---------- AMPROC CACHES ----------						 */

/*
 * get_opfamily_proc
 *		Get the OID of the specified support function
 *		for the specified opfamily and datatypes.
 *
 * Returns InvalidOid if there is no pg_amproc entry for the given keys.
 */
Oid
get_opfamily_proc(Oid opfamily, Oid lefttype, Oid righttype, int16 procnum)
{
	HeapTuple	tp;
	Form_pg_amproc amproc_tup;
	RegProcedure result;

	tp = SearchSysCache4(AMPROCNUM,
						 ObjectIdGetDatum(opfamily),
						 ObjectIdGetDatum(lefttype),
						 ObjectIdGetDatum(righttype),
						 Int16GetDatum(procnum));
	if (!HeapTupleIsValid(tp))
		return InvalidOid;
	amproc_tup = (Form_pg_amproc) GETSTRUCT(tp);
	result = amproc_tup->amproc;
	ReleaseSysCache(tp);
	return result;
}


/*				---------- ATTRIBUTE CACHES ----------					 */

/*
 * get_attname
 *		Given the relation id and the attribute number, return the "attname"
 *		field from the attribute relation as a palloc'ed string.
 *
 * If no such attribute exists and missing_ok is true, NULL is returned;
 * otherwise a not-intended-for-user-consumption error is thrown.
 */
char *
get_attname(Oid relid, AttrNumber attnum, bool missing_ok)
{
	HeapTuple	tp;

	tp = SearchSysCache2(ATTNUM,
						 ObjectIdGetDatum(relid), Int16GetDatum(attnum));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_attribute att_tup = (Form_pg_attribute) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(att_tup->attname));
		ReleaseSysCache(tp);
		return result;
	}

	if (!missing_ok)
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	return NULL;
}

/*
 * get_attnum
 *
 *		Given the relation id and the attribute name,
 *		return the "attnum" field from the attribute relation.
 *
 *		Returns InvalidAttrNumber if the attr doesn't exist (or is dropped).
 */
AttrNumber
get_attnum(Oid relid, const char *attname)
{
	HeapTuple	tp;

	tp = SearchSysCacheAttName(relid, attname);
	if (HeapTupleIsValid(tp))
	{
		Form_pg_attribute att_tup = (Form_pg_attribute) GETSTRUCT(tp);
		AttrNumber	result;

		result = att_tup->attnum;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidAttrNumber;
}

/*
 * get_attgenerated
 *
 *		Given the relation id and the attribute name,
 *		return the "attgenerated" field from the attribute relation.
 *
 *		Errors if not found.
 *
 *		Since not generated is represented by '\0', this can also be used as a
 *		Boolean test.
 */
char
get_attgenerated(Oid relid, AttrNumber attnum)
{
	HeapTuple	tp;
	Form_pg_attribute att_tup;
	char		result;

	tp = SearchSysCache2(ATTNUM,
						 ObjectIdGetDatum(relid),
						 Int16GetDatum(attnum));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	att_tup = (Form_pg_attribute) GETSTRUCT(tp);
	result = att_tup->attgenerated;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_atttype
 *
 *		Given the relation OID and the attribute number with the relation,
 *		return the attribute type OID.
 */
Oid
get_atttype(Oid relid, AttrNumber attnum)
{
	HeapTuple	tp;

	tp = SearchSysCache2(ATTNUM,
						 ObjectIdGetDatum(relid),
						 Int16GetDatum(attnum));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_attribute att_tup = (Form_pg_attribute) GETSTRUCT(tp);
		Oid			result;

		result = att_tup->atttypid;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_atttypetypmodcoll
 *
 *		A three-fer: given the relation id and the attribute number,
 *		fetch atttypid, atttypmod, and attcollation in a single cache lookup.
 *
 * Unlike the otherwise-similar get_atttype, this routine
 * raises an error if it can't obtain the information.
 */
void
get_atttypetypmodcoll(Oid relid, AttrNumber attnum,
					  Oid *typid, int32 *typmod, Oid *collid)
{
	HeapTuple	tp;
	Form_pg_attribute att_tup;

	/* CDB: Get type for sysattr even if relid is no good (e.g. SubqueryScan) */
	if (attnum < 0 &&
		attnum > FirstLowInvalidHeapAttributeNumber)
	{
		const FormData_pg_attribute *sysatt_tup;

		sysatt_tup = SystemAttributeDefinition(attnum);
		*typid = sysatt_tup->atttypid;
		*typmod = sysatt_tup->atttypmod;
		*collid = sysatt_tup->attcollation;
		return;
	}

	tp = SearchSysCache2(ATTNUM,
						 ObjectIdGetDatum(relid),
						 Int16GetDatum(attnum));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	att_tup = (Form_pg_attribute) GETSTRUCT(tp);

	*typid = att_tup->atttypid;
	*typmod = att_tup->atttypmod;
	*collid = att_tup->attcollation;
	ReleaseSysCache(tp);
}

/*				---------- COLLATION CACHE ----------					 */

/*
 * get_collation_name
 *		Returns the name of a given pg_collation entry.
 *
 * Returns a palloc'd copy of the string, or NULL if no such collation.
 *
 * NOTE: since collation name is not unique, be wary of code that uses this
 * for anything except preparing error messages.
 */
char *
get_collation_name(Oid colloid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(colloid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_collation colltup = (Form_pg_collation) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(colltup->collname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

bool
get_collation_isdeterministic(Oid colloid)
{
	HeapTuple	tp;
	Form_pg_collation colltup;
	bool		result;

	tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(colloid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for collation %u", colloid);
	colltup = (Form_pg_collation) GETSTRUCT(tp);
	result = colltup->collisdeterministic;
	ReleaseSysCache(tp);
	return result;
}

/*				---------- CONSTRAINT CACHE ----------					 */

/*
 * get_constraint_name
 *		Returns the name of a given pg_constraint entry.
 *
 * Returns a palloc'd copy of the string, or NULL if no such constraint.
 *
 * NOTE: since constraint name is not unique, be wary of code that uses this
 * for anything except preparing error messages.
 */
char *
get_constraint_name(Oid conoid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(CONSTROID, ObjectIdGetDatum(conoid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_constraint contup = (Form_pg_constraint) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(contup->conname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

/*				---------- LANGUAGE CACHE ----------					 */

char *
get_language_name(Oid langoid, bool missing_ok)
{
	HeapTuple	tp;

	tp = SearchSysCache1(LANGOID, ObjectIdGetDatum(langoid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_language lantup = (Form_pg_language) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(lantup->lanname));
		ReleaseSysCache(tp);
		return result;
	}

	if (!missing_ok)
		elog(ERROR, "cache lookup failed for language %u",
			 langoid);
	return NULL;
}

/*				---------- OPCLASS CACHE ----------						 */

/*
 * get_opclass_family
 *
 *		Returns the OID of the operator family the opclass belongs to.
 */
Oid
get_opclass_family(Oid opclass)
{
	HeapTuple	tp;
	Form_pg_opclass cla_tup;
	Oid			result;

	tp = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);
	cla_tup = (Form_pg_opclass) GETSTRUCT(tp);

	result = cla_tup->opcfamily;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_opclass_input_type
 *
 *		Returns the OID of the datatype the opclass indexes.
 */
Oid
get_opclass_input_type(Oid opclass)
{
	HeapTuple	tp;
	Form_pg_opclass cla_tup;
	Oid			result;

	tp = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);
	cla_tup = (Form_pg_opclass) GETSTRUCT(tp);

	result = cla_tup->opcintype;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_opclass_opfamily_and_input_type
 *
 *		Returns the OID of the operator family the opclass belongs to,
 *				the OID of the datatype the opclass indexes
 */
bool
get_opclass_opfamily_and_input_type(Oid opclass, Oid *opfamily, Oid *opcintype)
{
	HeapTuple	tp;
	Form_pg_opclass cla_tup;

	tp = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
	if (!HeapTupleIsValid(tp))
		return false;

	cla_tup = (Form_pg_opclass) GETSTRUCT(tp);

	*opfamily = cla_tup->opcfamily;
	*opcintype = cla_tup->opcintype;

	ReleaseSysCache(tp);

	return true;
}

/*				---------- OPERATOR CACHE ----------					 */

/*
 * get_opcode
 *
 *		Returns the regproc id of the routine used to implement an
 *		operator given the operator oid.
 */
RegProcedure
get_opcode(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		RegProcedure result;

		result = optup->oprcode;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return (RegProcedure) InvalidOid;
}

/*
 * get_opname
 *	  returns the name of the operator with the given opno
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such operator.
 */
char *
get_opname(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(optup->oprname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

/*
 * get_op_rettype
 *		Given operator oid, return the operator's result type.
 */
Oid
get_op_rettype(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		Oid			result;

		result = optup->oprresult;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * op_input_types
 *
 *		Returns the left and right input datatypes for an operator
 *		(InvalidOid if not relevant).
 */
void
op_input_types(Oid opno, Oid *lefttype, Oid *righttype)
{
	HeapTuple	tp;
	Form_pg_operator optup;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (!HeapTupleIsValid(tp))	/* shouldn't happen */
		elog(ERROR, "cache lookup failed for operator %u", opno);
	optup = (Form_pg_operator) GETSTRUCT(tp);
	*lefttype = optup->oprleft;
	*righttype = optup->oprright;
	ReleaseSysCache(tp);
}

/*
 * op_mergejoinable
 *
 * Returns true if the operator is potentially mergejoinable.  (The planner
 * will fail to find any mergejoin plans unless there are suitable btree
 * opfamily entries for this operator and associated sortops.  The pg_operator
 * flag is just a hint to tell the planner whether to bother looking.)
 *
 * In some cases (currently only array_eq and record_eq), mergejoinability
 * depends on the specific input data type the operator is invoked for, so
 * that must be passed as well. We currently assume that only one input's type
 * is needed to check this --- by convention, pass the left input's data type.
 */
bool
op_mergejoinable(Oid opno, Oid inputtype)
{
	bool		result = false;
	HeapTuple	tp;
	TypeCacheEntry *typentry;

	/*
	 * For array_eq or record_eq, we can sort if the element or field types
	 * are all sortable.  We could implement all the checks for that here, but
	 * the typcache already does that and caches the results too, so let's
	 * rely on the typcache.
	 */
	if (opno == ARRAY_EQ_OP)
	{
		typentry = lookup_type_cache(inputtype, TYPECACHE_CMP_PROC);
		if (typentry->cmp_proc == F_BTARRAYCMP)
			result = true;
	}
	else if (opno == RECORD_EQ_OP)
	{
		typentry = lookup_type_cache(inputtype, TYPECACHE_CMP_PROC);
		if (typentry->cmp_proc == F_BTRECORDCMP)
			result = true;
	}
	else
	{
		/* For all other operators, rely on pg_operator.oprcanmerge */
		tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
		if (HeapTupleIsValid(tp))
		{
			Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);

			result = optup->oprcanmerge;
			ReleaseSysCache(tp);
		}
	}
	return result;
}

/*
 * op_hashjoinable
 *
 * Returns true if the operator is hashjoinable.  (There must be a suitable
 * hash opfamily entry for this operator if it is so marked.)
 *
 * In some cases (currently only array_eq), hashjoinability depends on the
 * specific input data type the operator is invoked for, so that must be
 * passed as well.  We currently assume that only one input's type is needed
 * to check this --- by convention, pass the left input's data type.
 */
bool
op_hashjoinable(Oid opno, Oid inputtype)
{
	bool		result = false;
	HeapTuple	tp;
	TypeCacheEntry *typentry;

	/* As in op_mergejoinable, let the typcache handle the hard cases */
	/* Eventually we'll need a similar case for record_eq ... */
	if (opno == ARRAY_EQ_OP)
	{
		typentry = lookup_type_cache(inputtype, TYPECACHE_HASH_PROC);
		if (typentry->hash_proc == F_HASH_ARRAY)
			result = true;
	}
	else
	{
		/* For all other operators, rely on pg_operator.oprcanhash */
		tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
		if (HeapTupleIsValid(tp))
		{
			Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);

			result = optup->oprcanhash;
			ReleaseSysCache(tp);
		}
	}
	return result;
}

/*
 * op_strict
 *
 * Get the proisstrict flag for the operator's underlying function.
 */
bool
op_strict(Oid opno)
{
	RegProcedure funcid = get_opcode(opno);

	if (funcid == (RegProcedure) InvalidOid)
		elog(ERROR, "operator %u does not exist", opno);

	return func_strict((Oid) funcid);
}

/*
 * op_volatile
 *
 * Get the provolatile flag for the operator's underlying function.
 */
char
op_volatile(Oid opno)
{
	RegProcedure funcid = get_opcode(opno);

	if (funcid == (RegProcedure) InvalidOid)
		elog(ERROR, "operator %u does not exist", opno);

	return func_volatile((Oid) funcid);
}

/*
 * get_commutator
 *
 *		Returns the corresponding commutator of an operator.
 */
Oid
get_commutator(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		Oid			result;

		result = optup->oprcom;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_negator
 *
 *		Returns the corresponding negator of an operator.
 */
Oid
get_negator(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		Oid			result;

		result = optup->oprnegate;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_oprrest
 *
 *		Returns procedure id for computing selectivity of an operator.
 */
RegProcedure
get_oprrest(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		RegProcedure result;

		result = optup->oprrest;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return (RegProcedure) InvalidOid;
}

/*
 * get_oprjoin
 *
 *		Returns procedure id for computing selectivity of a join.
 */
RegProcedure
get_oprjoin(Oid opno)
{
	HeapTuple	tp;

	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);
		RegProcedure result;

		result = optup->oprjoin;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return (RegProcedure) InvalidOid;
}

/*				---------- TRIGGER CACHE ----------					 */


/* Does table have update triggers? */
bool
has_update_triggers(Oid relid)
{
	Relation	relation;
	bool		result = false;

	/* Assume the caller already holds a suitable lock. */
	relation = table_open(relid, NoLock);

	if (relation->rd_rel->relhastriggers)
	{
		bool	found = false;

		if (relation->trigdesc == NULL)
			RelationBuildTriggers(relation);

		if (relation->trigdesc)
		{
			for (int i = 0; i < relation->trigdesc->numtriggers && !found; i++)
			{
				Trigger trigger = relation->trigdesc->triggers[i];
				found = trigger_enabled(trigger.tgoid) &&
					(get_trigger_type(trigger.tgoid) & TRIGGER_TYPE_UPDATE) == TRIGGER_TYPE_UPDATE;
				if (found)
					break;
			}
		}

		/* GPDB_96_MERGE_FIXME: Why is this not allowed? */
		if (found || child_triggers(relation->rd_id, TRIGGER_TYPE_UPDATE))
			result = true;
	}
	table_close(relation, NoLock);

	return result;
}

/*
 * get_trigger_type
 *		Given trigger id, return the trigger's type
 */
int32
get_trigger_type(Oid triggerid)
{
	Relation	rel;
	HeapTuple	tp;
	int32		result = -1;
	ScanKeyData	scankey;
	SysScanDesc sscan;

	ScanKeyInit(&scankey, Anum_pg_trigger_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(triggerid));
	rel = table_open(TriggerRelationId, AccessShareLock);
	sscan = systable_beginscan(rel, TriggerOidIndexId, true,
							   NULL, 1, &scankey);

	tp = systable_getnext(sscan);
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for trigger %u", triggerid);

	result = ((Form_pg_trigger) GETSTRUCT(tp))->tgtype;

	systable_endscan(sscan);
	table_close(rel, AccessShareLock);

	return result;
}

/*
 * trigger_enabled
 *		Given trigger id, return the trigger's enabled flag
 */
bool
trigger_enabled(Oid triggerid)
{
	Relation	rel;
	HeapTuple	tp;
	bool		result;
	ScanKeyData	scankey;
	SysScanDesc sscan;

	ScanKeyInit(&scankey, Anum_pg_trigger_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(triggerid));
	rel = table_open(TriggerRelationId, AccessShareLock);
	sscan = systable_beginscan(rel, TriggerOidIndexId, true,
							   NULL, 1, &scankey);

	tp = systable_getnext(sscan);
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for trigger %u", triggerid);

	char tgenabled = ((Form_pg_trigger) GETSTRUCT(tp))->tgenabled;
	switch (tgenabled)
	{
		case TRIGGER_FIRES_ON_ORIGIN:
			/* fallthrough */
			/*
			 * FIXME: we should probably return false when
			 * SessionReplicationRole isn't SESSION_REPLICATION_ROLE_ORIGIN,
			 * but does that means we'll also have to flush ORCA's metadata
			 * cache on every assignment of session_replication_role?
			 */
		case TRIGGER_FIRES_ALWAYS:
			result = true;
			break;
		case TRIGGER_FIRES_ON_REPLICA:
		case TRIGGER_DISABLED:
			result = false;
			break;
		default:
			elog(ERROR, "Unknown trigger type: %c", tgenabled);
	}

	systable_endscan(sscan);
	table_close(rel, AccessShareLock);

	return result;
}

/*				---------- FUNCTION CACHE ----------					 */

/*
 * get_func_name
 *	  returns the name of the function with the given funcid
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such function.
 */
char *
get_func_name(Oid funcid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_proc functup = (Form_pg_proc) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(functup->proname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

/*
 * get_type_name
 *	  returns the name of the type with the given oid
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such type.
 */
char *
get_type_name(Oid oid)
{
	HeapTuple	tp;

	tp = SearchSysCache(TYPEOID,
						ObjectIdGetDatum(oid),
						0, 0, 0);
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(typtup->typname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

/*
 * get_func_namespace
 *
 *		Returns the pg_namespace OID associated with a given function.
 */
Oid
get_func_namespace(Oid funcid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_proc functup = (Form_pg_proc) GETSTRUCT(tp);
		Oid			result;

		result = functup->pronamespace;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*				---------- RELATION CACHE ----------					 */
 
/*
 * get_func_rettype
 *		Given procedure id, return the function's result type.
 */
Oid
get_func_rettype(Oid funcid)
{
	HeapTuple	tp;
	Oid			result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->prorettype;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_agg_transtype
 *		Given aggregate id, return the aggregate transition function's result type.
 */
Oid
get_agg_transtype(Oid aggid)
{
	HeapTuple	tp;
	Oid			result;

	tp = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(aggid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);

	result = ((Form_pg_aggregate) GETSTRUCT(tp))->aggtranstype;
	ReleaseSysCache(tp);
	return result;
}

/*
 * is_ordered_agg
 *		Given aggregate id, check if it is an ordered aggregate
 */
bool
is_agg_ordered(Oid aggid)
{
	HeapTuple	aggTuple;
	char		aggkind;
	bool		isnull = false;

	aggTuple = SearchSysCache1(AGGFNOID,
							   ObjectIdGetDatum(aggid));
	if (!HeapTupleIsValid(aggTuple))
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);

	aggkind = DatumGetChar(SysCacheGetAttr(AGGFNOID, aggTuple,
										   Anum_pg_aggregate_aggkind, &isnull));
	Assert(!isnull);

	ReleaseSysCache(aggTuple);

	return AGGKIND_IS_ORDERED_SET(aggkind);
}

/*
 * is_agg_partial_capable
 *		Given aggregate id, check if it can be used in 2-phase aggregation.
 *
 * It must have a combine function, and if the transition type is 'internal',
 * also serial/deserial functions.
 */
bool
is_agg_partial_capable(Oid aggid)
{
	HeapTuple	aggTuple;
	Form_pg_aggregate aggform;
	bool		result = true;

	aggTuple = SearchSysCache1(AGGFNOID,
							   ObjectIdGetDatum(aggid));
	if (!HeapTupleIsValid(aggTuple))
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);
	aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);

	if (aggform->aggcombinefn == InvalidOid)
		result = false;
	else if (aggform->aggtranstype == INTERNALOID)
	{
		if (aggform->aggserialfn == InvalidOid ||
			aggform->aggdeserialfn == InvalidOid)
		{
			result = false;
		}
	}

	ReleaseSysCache(aggTuple);

	return result;
}

/*
 * get_rel_tablespace
 *
 *		Returns the pg_tablespace OID associated with a given relation.
 *
 * Note: InvalidOid might mean either that we couldn't find the relation,
 * or that it is in the database's default tablespace.
 */
Oid
get_rel_tablespace(Oid relid)
{
	HeapTuple	tp;

	tp = SearchSysCache(RELOID,
						ObjectIdGetDatum(relid),
						0, 0, 0);
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		Oid			result;

		result = reltup->reltablespace;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}


/*
 * get_func_nargs
 *		Given procedure id, return the number of arguments.
 */
int
get_func_nargs(Oid funcid)
{
	HeapTuple	tp;
	int			result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->pronargs;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_func_signature
 *		Given procedure id, return the function's argument and result types.
 *		(The return value is the result type.)
 *
 * The arguments are returned as a palloc'd array.
 */
Oid
get_func_signature(Oid funcid, Oid **argtypes, int *nargs)
{
	HeapTuple	tp;
	Form_pg_proc procstruct;
	Oid			result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	procstruct = (Form_pg_proc) GETSTRUCT(tp);

	result = procstruct->prorettype;
	*nargs = (int) procstruct->pronargs;
	Assert(*nargs == procstruct->proargtypes.dim1);
	*argtypes = (Oid *) palloc(*nargs * sizeof(Oid));
	memcpy(*argtypes, procstruct->proargtypes.values, *nargs * sizeof(Oid));

	ReleaseSysCache(tp);
	return result;
}

/*
 * pfree_ptr_array
 * 		Free an array of pointers, after freeing each individual element
 */
void
pfree_ptr_array(char **ptrarray, int nelements)
{
	int i;
	if (NULL == ptrarray)
		return;

	for (i = 0; i < nelements; i++)
	{
		if (NULL != ptrarray[i])
		{
			pfree(ptrarray[i]);
		}
	}
	pfree(ptrarray);
}

/*
 * get_func_output_arg_types
 *		Given procedure id, return the function's output argument types
 */
List *
get_func_output_arg_types(Oid funcid)
{
	HeapTuple	tp;
	int			numargs;
	Oid		   *argtypes = NULL;
	char	  **argnames = NULL;
	char	   *argmodes = NULL;
	List	   *l_argtypes = NIL;
	int			i;

	tp = SearchSysCache1(PROCOID,
						 ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	numargs = get_func_arg_info(tp, &argtypes, &argnames, &argmodes);

	if (NULL == argmodes)
	{
		pfree_ptr_array(argnames, numargs);
		if (NULL != argtypes)
		{
			pfree(argtypes);
		}
		ReleaseSysCache(tp);
		return NULL;
	}

	for (i = 0; i < numargs; i++)
	{
		Oid			argtype = argtypes[i];
		char		argmode = argmodes[i];

		if (PROARGMODE_INOUT == argmode || PROARGMODE_OUT == argmode || PROARGMODE_TABLE == argmode)
		{
			l_argtypes = lappend_oid(l_argtypes, argtype);
		}
	}

	pfree_ptr_array(argnames, numargs);
	pfree(argtypes);
	pfree(argmodes);

	ReleaseSysCache(tp);
	return l_argtypes;
}

/*
 * get_func_arg_types
 *		Given procedure id, return all the function's argument types
 */
List *
get_func_arg_types(Oid funcid)
{
	HeapTuple	tp;
	Form_pg_proc procstruct;
	oidvector *args;
	List *result = NIL;

	tp = SearchSysCache(PROCOID,
						ObjectIdGetDatum(funcid),
						0, 0, 0);
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	procstruct = (Form_pg_proc) GETSTRUCT(tp);
	args = &procstruct->proargtypes;
	for (int i = 0; i < args->dim1; i++)
	{
		result = lappend_oid(result, args->values[i]);
	}

	ReleaseSysCache(tp);
	return result;
}

/*
 * get_func_variadictype
 *		Given procedure id, return the function's provariadic field.
 */
Oid
get_func_variadictype(Oid funcid)
{
	HeapTuple	tp;
	Oid			result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->provariadic;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_func_retset
 *		Given procedure id, return the function's proretset flag.
 */
bool
get_func_retset(Oid funcid)
{
	HeapTuple	tp;
	bool		result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->proretset;
	ReleaseSysCache(tp);
	return result;
}

/*
 * func_strict
 *		Given procedure id, return the function's proisstrict flag.
 */
bool
func_strict(Oid funcid)
{
	HeapTuple	tp;
	bool		result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->proisstrict;
	ReleaseSysCache(tp);
	return result;
}

/*
 * func_volatile
 *		Given procedure id, return the function's provolatile flag.
 */
char
func_volatile(Oid funcid)
{
	HeapTuple	tp;
	char		result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->provolatile;
	ReleaseSysCache(tp);
	return result;
}

/*
 * func_parallel
 *		Given procedure id, return the function's proparallel flag.
 */
char
func_parallel(Oid funcid)
{
	HeapTuple	tp;
	char		result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->proparallel;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_func_prokind
 *	   Given procedure id, return the routine kind.
 */
char
get_func_prokind(Oid funcid)
{
	HeapTuple	tp;
	char		result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->prokind;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_func_leakproof
 *	   Given procedure id, return the function's leakproof field.
 */
bool
get_func_leakproof(Oid funcid)
{
	HeapTuple	tp;
	bool		result;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->proleakproof;
	ReleaseSysCache(tp);
	return result;
}

/*
 * get_func_support
 *
 *		Returns the support function OID associated with a given function,
 *		or InvalidOid if there is none.
 */
RegProcedure
get_func_support(Oid funcid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_proc functup = (Form_pg_proc) GETSTRUCT(tp);
		RegProcedure result;

		result = functup->prosupport;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return (RegProcedure) InvalidOid;
}

/*
 * func_data_access
 *		Given procedure id, return the function's data access flag.
 */
char
func_data_access(Oid funcid)
{
	HeapTuple	tp;
	char		result;
	bool		isnull;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = DatumGetChar(
		SysCacheGetAttr(PROCOID, tp, Anum_pg_proc_prodataaccess, &isnull));
	ReleaseSysCache(tp);

	Assert(!isnull);
	return result;
}

/*
 * func_exec_location
 *		Given procedure id, return the function's proexeclocation field
 */
char
func_exec_location(Oid funcid)
{
	HeapTuple	tp;
	char		result;
	bool		isnull;

	tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = DatumGetChar(
		SysCacheGetAttr(PROCOID, tp, Anum_pg_proc_proexeclocation, &isnull));
	ReleaseSysCache(tp);

	Assert(!isnull);
	return result;
}

/*				---------- RELATION CACHE ----------					 */

/*
 * get_relname_relid
 *		Given name and namespace of a relation, look up the OID.
 *
 * Returns InvalidOid if there is no such relation.
 */
Oid
get_relname_relid(const char *relname, Oid relnamespace)
{
	return GetSysCacheOid2(RELNAMENSP, Anum_pg_class_oid,
						   PointerGetDatum(relname),
						   ObjectIdGetDatum(relnamespace));
}

#ifdef NOT_USED
/*
 * get_relnatts
 *
 *		Returns the number of attributes for a given relation.
 */
int
get_relnatts(Oid relid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		int			result;

		result = reltup->relnatts;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidAttrNumber;
}
#endif

/*
 * get_rel_name
 *		Returns the name of a given relation.
 *
 * Returns a palloc'd copy of the string, or NULL if no such relation.
 *
 * NOTE: since relation name is not unique, be wary of code that uses this
 * for anything except preparing error messages.
 */
char *
get_rel_name(Oid relid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(reltup->relname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

/*
 * get_rel_namespace
 *
 *		Returns the pg_namespace OID associated with a given relation.
 */
Oid
get_rel_namespace(Oid relid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		Oid			result;

		result = reltup->relnamespace;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_rel_type_id
 *
 *		Returns the pg_type OID associated with a given relation.
 *
 * Note: not all pg_class entries have associated pg_type OIDs; so be
 * careful to check for InvalidOid result.
 */
Oid
get_rel_type_id(Oid relid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		Oid			result;

		result = reltup->reltype;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_rel_relkind
 *
 *		Returns the relkind associated with a given relation.
 */
char
get_rel_relkind(Oid relid)
{
	HeapTuple	tp;
	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		char		result;

		result = reltup->relkind;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return '\0';
}

/*
 * get_rel_relispartition
 *
 *		Returns the relispartition flag associated with a given relation.
 */
bool
get_rel_relispartition(Oid relid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);
		bool		result;

		result = reltup->relispartition;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return false;
}

/*
 * get_rel_persistence
 *
 *		Returns the relpersistence associated with a given relation.
 */
char
get_rel_persistence(Oid relid)
{
	HeapTuple	tp;
	Form_pg_class reltup;
	char		result;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for relation %u", relid);
	reltup = (Form_pg_class) GETSTRUCT(tp);
	result = reltup->relpersistence;
	ReleaseSysCache(tp);

	return result;
}


/*				---------- TRANSFORM CACHE ----------						 */

Oid
get_transform_fromsql(Oid typid, Oid langid, List *trftypes)
{
	HeapTuple	tup;

	if (!list_member_oid(trftypes, typid))
		return InvalidOid;

	tup = SearchSysCache2(TRFTYPELANG, typid, langid);
	if (HeapTupleIsValid(tup))
	{
		Oid			funcid;

		funcid = ((Form_pg_transform) GETSTRUCT(tup))->trffromsql;
		ReleaseSysCache(tup);
		return funcid;
	}
	else
		return InvalidOid;
}

Oid
get_transform_tosql(Oid typid, Oid langid, List *trftypes)
{
	HeapTuple	tup;

	if (!list_member_oid(trftypes, typid))
		return InvalidOid;

	tup = SearchSysCache2(TRFTYPELANG, typid, langid);
	if (HeapTupleIsValid(tup))
	{
		Oid			funcid;

		funcid = ((Form_pg_transform) GETSTRUCT(tup))->trftosql;
		ReleaseSysCache(tup);
		return funcid;
	}
	else
		return InvalidOid;
}

/*				---------- TYPE CACHE ----------						 */

/*
 * get_typisdefined
 *
 *		Given the type OID, determine whether the type is defined
 *		(if not, it's only a shell).
 */
bool
get_typisdefined(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		bool		result;

		result = typtup->typisdefined;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return false;
}

/*
 * get_typlen
 *
 *		Given the type OID, return the length of the type.
 */
int16
get_typlen(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		int16		result;

		result = typtup->typlen;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return 0;
}

/*
 * get_typbyval
 *
 *		Given the type OID, determine whether the type is returned by value or
 *		not.  Returns true if by value, false if by reference.
 */
bool
get_typbyval(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		bool		result;

		result = typtup->typbyval;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return false;
}

/*
 * get_typlenbyval
 *
 *		A two-fer: given the type OID, return both typlen and typbyval.
 *
 *		Since both pieces of info are needed to know how to copy a Datum,
 *		many places need both.  Might as well get them with one cache lookup
 *		instead of two.  Also, this routine raises an error instead of
 *		returning a bogus value when given a bad type OID.
 */
void
get_typlenbyval(Oid typid, int16 *typlen, bool *typbyval)
{
	HeapTuple	tp;
	Form_pg_type typtup;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typtup = (Form_pg_type) GETSTRUCT(tp);
	*typlen = typtup->typlen;
	*typbyval = typtup->typbyval;
	ReleaseSysCache(tp);
}

/*
 * get_typlenbyvalalign
 *
 *		A three-fer: given the type OID, return typlen, typbyval, typalign.
 */
void
get_typlenbyvalalign(Oid typid, int16 *typlen, bool *typbyval,
					 char *typalign)
{
	HeapTuple	tp;
	Form_pg_type typtup;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typtup = (Form_pg_type) GETSTRUCT(tp);
	*typlen = typtup->typlen;
	*typbyval = typtup->typbyval;
	*typalign = typtup->typalign;
	ReleaseSysCache(tp);
}

/*
 * getTypeIOParam
 *		Given a pg_type row, select the type OID to pass to I/O functions
 *
 * Formerly, all I/O functions were passed pg_type.typelem as their second
 * parameter, but we now have a more complex rule about what to pass.
 * This knowledge is intended to be centralized here --- direct references
 * to typelem elsewhere in the code are wrong, if they are associated with
 * I/O calls and not with actual subscripting operations!  (But see
 * bootstrap.c's boot_get_type_io_data() if you need to change this.)
 *
 * As of PostgreSQL 8.1, output functions receive only the value itself
 * and not any auxiliary parameters, so the name of this routine is now
 * a bit of a misnomer ... it should be getTypeInputParam.
 */
Oid
getTypeIOParam(HeapTuple typeTuple)
{
	Form_pg_type typeStruct = (Form_pg_type) GETSTRUCT(typeTuple);

	/*
	 * Array types get their typelem as parameter; everybody else gets their
	 * own type OID as parameter.
	 */
	if (OidIsValid(typeStruct->typelem))
		return typeStruct->typelem;
	else
		return typeStruct->oid;
}

/*
 * get_type_io_data
 *
 *		A six-fer:	given the type OID, return typlen, typbyval, typalign,
 *					typdelim, typioparam, and IO function OID. The IO function
 *					returned is controlled by IOFuncSelector
 */
void
get_type_io_data(Oid typid,
				 IOFuncSelector which_func,
				 int16 *typlen,
				 bool *typbyval,
				 char *typalign,
				 char *typdelim,
				 Oid *typioparam,
				 Oid *func)
{
	HeapTuple	typeTuple;
	Form_pg_type typeStruct;

	/*
	 * In bootstrap mode, pass it off to bootstrap.c.  This hack allows us to
	 * use array_in and array_out during bootstrap.
	 */
	if (IsBootstrapProcessingMode())
	{
		Oid			typinput;
		Oid			typoutput;

		boot_get_type_io_data(typid,
							  typlen,
							  typbyval,
							  typalign,
							  typdelim,
							  typioparam,
							  &typinput,
							  &typoutput);
		switch (which_func)
		{
			case IOFunc_input:
				*func = typinput;
				break;
			case IOFunc_output:
				*func = typoutput;
				break;
			default:
				elog(ERROR, "binary I/O not supported during bootstrap");
				break;
		}
		return;
	}

	typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typeStruct = (Form_pg_type) GETSTRUCT(typeTuple);

	*typlen = typeStruct->typlen;
	*typbyval = typeStruct->typbyval;
	*typalign = typeStruct->typalign;
	*typdelim = typeStruct->typdelim;
	*typioparam = getTypeIOParam(typeTuple);
	switch (which_func)
	{
		case IOFunc_input:
			*func = typeStruct->typinput;
			break;
		case IOFunc_output:
			*func = typeStruct->typoutput;
			break;
		case IOFunc_receive:
			*func = typeStruct->typreceive;
			break;
		case IOFunc_send:
			*func = typeStruct->typsend;
			break;
	}
	ReleaseSysCache(typeTuple);
}

#ifdef NOT_USED
char
get_typalign(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		char		result;

		result = typtup->typalign;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return 'i';
}
#endif

char
get_typstorage(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		char		result;

		result = typtup->typstorage;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return 'p';
}

/*
 * get_typdefault
 *	  Given a type OID, return the type's default value, if any.
 *
 *	  The result is a palloc'd expression node tree, or NULL if there
 *	  is no defined default for the datatype.
 *
 * NB: caller should be prepared to coerce result to correct datatype;
 * the returned expression tree might produce something of the wrong type.
 */
Node *
get_typdefault(Oid typid)
{
	HeapTuple	typeTuple;
	Form_pg_type type;
	Datum		datum;
	bool		isNull;
	Node	   *expr;

	typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", typid);
	type = (Form_pg_type) GETSTRUCT(typeTuple);

	/*
	 * typdefault and typdefaultbin are potentially null, so don't try to
	 * access 'em as struct fields. Must do it the hard way with
	 * SysCacheGetAttr.
	 */
	datum = SysCacheGetAttr(TYPEOID,
							typeTuple,
							Anum_pg_type_typdefaultbin,
							&isNull);

	if (!isNull)
	{
		/* We have an expression default */
		expr = stringToNode(TextDatumGetCString(datum));
	}
	else
	{
		/* Perhaps we have a plain literal default */
		datum = SysCacheGetAttr(TYPEOID,
								typeTuple,
								Anum_pg_type_typdefault,
								&isNull);

		if (!isNull)
		{
			char	   *strDefaultVal;

			/* Convert text datum to C string */
			strDefaultVal = TextDatumGetCString(datum);
			/* Convert C string to a value of the given type */
			datum = OidInputFunctionCall(type->typinput, strDefaultVal,
										 getTypeIOParam(typeTuple), -1);
			/* Build a Const node containing the value */
			expr = (Node *) makeConst(typid,
									  -1,
									  type->typcollation,
									  type->typlen,
									  datum,
									  false,
									  type->typbyval);
			pfree(strDefaultVal);
		}
		else
		{
			/* No default */
			expr = NULL;
		}
	}

	ReleaseSysCache(typeTuple);

	return expr;
}

/*
 * getBaseType
 *		If the given type is a domain, return its base type;
 *		otherwise return the type's own OID.
 */
Oid
getBaseType(Oid typid)
{
	int32		typmod = -1;

	return getBaseTypeAndTypmod(typid, &typmod);
}

/*
 * getBaseTypeAndTypmod
 *		If the given type is a domain, return its base type and typmod;
 *		otherwise return the type's own OID, and leave *typmod unchanged.
 *
 * Note that the "applied typmod" should be -1 for every domain level
 * above the bottommost; therefore, if the passed-in typid is indeed
 * a domain, *typmod should be -1.
 */
Oid
getBaseTypeAndTypmod(Oid typid, int32 *typmod)
{
	/*
	 * We loop to find the bottom base type in a stack of domains.
	 */
	for (;;)
	{
		HeapTuple	tup;
		Form_pg_type typTup;

		tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
		if (!HeapTupleIsValid(tup))
			elog(ERROR, "cache lookup failed for type %u", typid);
		typTup = (Form_pg_type) GETSTRUCT(tup);
		if (typTup->typtype != TYPTYPE_DOMAIN)
		{
			/* Not a domain, so done */
			ReleaseSysCache(tup);
			break;
		}

		Assert(*typmod == -1);
		typid = typTup->typbasetype;
		*typmod = typTup->typtypmod;

		ReleaseSysCache(tup);
	}

	return typid;
}

/*
 * get_typavgwidth
 *
 *	  Given a type OID and a typmod value (pass -1 if typmod is unknown),
 *	  estimate the average width of values of the type.  This is used by
 *	  the planner, which doesn't require absolutely correct results;
 *	  it's OK (and expected) to guess if we don't know for sure.
 */
int32
get_typavgwidth(Oid typid, int32 typmod)
{
	int			typlen = get_typlen(typid);
	int32		maxwidth;

	/*
	 * Easy if it's a fixed-width type
	 */
	if (typlen > 0)
		return typlen;

	/*
	 * type_maximum_size knows the encoding of typmod for some datatypes;
	 * don't duplicate that knowledge here.
	 */
	maxwidth = type_maximum_size(typid, typmod);
	if (maxwidth > 0)
	{
		/*
		 * For BPCHAR, the max width is also the only width.  Otherwise we
		 * need to guess about the typical data width given the max. A sliding
		 * scale for percentage of max width seems reasonable.
		 */
		if (typid == BPCHAROID)
			return maxwidth;
		if (maxwidth <= 32)
			return maxwidth;	/* assume full width */
		if (maxwidth < 1000)
			return 32 + (maxwidth - 32) / 2;	/* assume 50% */

		/*
		 * Beyond 1000, assume we're looking at something like
		 * "varchar(10000)" where the limit isn't actually reached often, and
		 * use a fixed estimate.
		 */
		return 32 + (1000 - 32) / 2;
	}

	/*
	 * Oops, we have no idea ... wild guess time.
	 */
	return 32;
}

/*
 * get_typtype
 *
 *		Given the type OID, find if it is a basic type, a complex type, etc.
 *		It returns the null char if the cache lookup fails...
 */
char
get_typtype(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		char		result;

		result = typtup->typtype;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return '\0';
}

/*
 * type_is_rowtype
 *
 *		Convenience function to determine whether a type OID represents
 *		a "rowtype" type --- either RECORD or a named composite type
 *		(including a domain over a named composite type).
 */
bool
type_is_rowtype(Oid typid)
{
	if (typid == RECORDOID)
		return true;			/* easy case */
	switch (get_typtype(typid))
	{
		case TYPTYPE_COMPOSITE:
			return true;
		case TYPTYPE_DOMAIN:
			if (get_typtype(getBaseType(typid)) == TYPTYPE_COMPOSITE)
				return true;
			break;
		default:
			break;
	}
	return false;
}

/*
 * type_is_enum
 *	  Returns true if the given type is an enum type.
 */
bool
type_is_enum(Oid typid)
{
	return (get_typtype(typid) == TYPTYPE_ENUM);
}

/*
 * type_is_range
 *	  Returns true if the given type is a range type.
 */
bool
type_is_range(Oid typid)
{
	return (get_typtype(typid) == TYPTYPE_RANGE);
}

/*
 * get_type_category_preferred
 *
 *		Given the type OID, fetch its category and preferred-type status.
 *		Throws error on failure.
 */
void
get_type_category_preferred(Oid typid, char *typcategory, bool *typispreferred)
{
	HeapTuple	tp;
	Form_pg_type typtup;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typtup = (Form_pg_type) GETSTRUCT(tp);
	*typcategory = typtup->typcategory;
	*typispreferred = typtup->typispreferred;
	ReleaseSysCache(tp);
}

/*
 * get_typ_typrelid
 *
 *		Given the type OID, get the typrelid (InvalidOid if not a complex
 *		type).
 */
Oid
get_typ_typrelid(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		Oid			result;

		result = typtup->typrelid;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_element_type
 *
 *		Given the type OID, get the typelem (InvalidOid if not an array type).
 *
 * NB: this only considers varlena arrays to be true arrays; InvalidOid is
 * returned if the input is a fixed-length array type.
 */
Oid
get_element_type(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		Oid			result;

		if (typtup->typlen == -1)
			result = typtup->typelem;
		else
			result = InvalidOid;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_array_type
 *
 *		Given the type OID, get the corresponding "true" array type.
 *		Returns InvalidOid if no array type can be found.
 */
Oid
get_array_type(Oid typid)
{
	HeapTuple	tp;
	Oid			result = InvalidOid;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		result = ((Form_pg_type) GETSTRUCT(tp))->typarray;
		ReleaseSysCache(tp);
	}
	return result;
}

/*
 * get_promoted_array_type
 *
 *		The "promoted" type is what you'd get from an ARRAY(SELECT ...)
 *		construct, that is, either the corresponding "true" array type
 *		if the input is a scalar type that has such an array type,
 *		or the same type if the input is already a "true" array type.
 *		Returns InvalidOid if neither rule is satisfied.
 */
Oid
get_promoted_array_type(Oid typid)
{
	Oid			array_type = get_array_type(typid);

	if (OidIsValid(array_type))
		return array_type;
	if (OidIsValid(get_element_type(typid)))
		return typid;
	return InvalidOid;
}

/*
 * get_base_element_type
 *		Given the type OID, get the typelem, looking "through" any domain
 *		to its underlying array type.
 *
 * This is equivalent to get_element_type(getBaseType(typid)), but avoids
 * an extra cache lookup.  Note that it fails to provide any information
 * about the typmod of the array.
 */
Oid
get_base_element_type(Oid typid)
{
	/*
	 * We loop to find the bottom base type in a stack of domains.
	 */
	for (;;)
	{
		HeapTuple	tup;
		Form_pg_type typTup;

		tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
		if (!HeapTupleIsValid(tup))
			break;
		typTup = (Form_pg_type) GETSTRUCT(tup);
		if (typTup->typtype != TYPTYPE_DOMAIN)
		{
			/* Not a domain, so stop descending */
			Oid			result;

			/* This test must match get_element_type */
			if (typTup->typlen == -1)
				result = typTup->typelem;
			else
				result = InvalidOid;
			ReleaseSysCache(tup);
			return result;
		}

		typid = typTup->typbasetype;
		ReleaseSysCache(tup);
	}

	/* Like get_element_type, silently return InvalidOid for bogus input */
	return InvalidOid;
}

/*
 * getTypeInputInfo
 *
 *		Get info needed for converting values of a type to internal form
 */
void
getTypeInputInfo(Oid type, Oid *typInput, Oid *typIOParam)
{
	HeapTuple	typeTuple;
	Form_pg_type pt;

	typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", type);
	pt = (Form_pg_type) GETSTRUCT(typeTuple);

	if (!pt->typisdefined)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type %s is only a shell",
						format_type_be(type))));
	if (!OidIsValid(pt->typinput))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("no input function available for type %s",
						format_type_be(type))));

	*typInput = pt->typinput;
	*typIOParam = getTypeIOParam(typeTuple);

	ReleaseSysCache(typeTuple);
}

/*
 * getTypeOutputInfo
 *
 *		Get info needed for printing values of a type
 */
void
getTypeOutputInfo(Oid type, Oid *typOutput, bool *typIsVarlena)
{
	HeapTuple	typeTuple;
	Form_pg_type pt;

	typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", type);
	pt = (Form_pg_type) GETSTRUCT(typeTuple);

	if (!pt->typisdefined)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type %s is only a shell",
						format_type_be(type))));
	if (!OidIsValid(pt->typoutput))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("no output function available for type %s",
						format_type_be(type))));

	*typOutput = pt->typoutput;
	*typIsVarlena = (!pt->typbyval) && (pt->typlen == -1);

	ReleaseSysCache(typeTuple);
}

/*
 * getTypeBinaryInputInfo
 *
 *		Get info needed for binary input of values of a type
 */
void
getTypeBinaryInputInfo(Oid type, Oid *typReceive, Oid *typIOParam)
{
	HeapTuple	typeTuple;
	Form_pg_type pt;

	typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", type);
	pt = (Form_pg_type) GETSTRUCT(typeTuple);

	if (!pt->typisdefined)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type %s is only a shell",
						format_type_be(type))));
	if (!OidIsValid(pt->typreceive))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("no binary input function available for type %s",
						format_type_be(type))));

	*typReceive = pt->typreceive;
	*typIOParam = getTypeIOParam(typeTuple);

	ReleaseSysCache(typeTuple);
}

/*
 * getTypeBinaryOutputInfo
 *
 *		Get info needed for binary output of values of a type
 */
void
getTypeBinaryOutputInfo(Oid type, Oid *typSend, bool *typIsVarlena)
{
	HeapTuple	typeTuple;
	Form_pg_type pt;

	typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", type);
	pt = (Form_pg_type) GETSTRUCT(typeTuple);

	if (!pt->typisdefined)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type %s is only a shell",
						format_type_be(type))));
	if (!OidIsValid(pt->typsend))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("no binary output function available for type %s",
						format_type_be(type))));

	*typSend = pt->typsend;
	*typIsVarlena = (!pt->typbyval) && (pt->typlen == -1);

	ReleaseSysCache(typeTuple);
}

/*
 * get_typmodin
 *
 *		Given the type OID, return the type's typmodin procedure, if any.
 */
Oid
get_typmodin(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		Oid			result;

		result = typtup->typmodin;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

#ifdef NOT_USED
/*
 * get_typmodout
 *
 *		Given the type OID, return the type's typmodout procedure, if any.
 */
Oid
get_typmodout(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		Oid			result;

		result = typtup->typmodout;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}
#endif							/* NOT_USED */

/*
 * get_typcollation
 *
 *		Given the type OID, return the type's typcollation attribute.
 */
Oid
get_typcollation(Oid typid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		Oid			result;

		result = typtup->typcollation;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}


/*
 * type_is_collatable
 *
 *		Return whether the type cares about collations
 */
bool
type_is_collatable(Oid typid)
{
	return OidIsValid(get_typcollation(typid));
}


/*				---------- STATISTICS CACHE ----------					 */

/*
 * get_attavgwidth
 *
 *	  Given the table and attribute number of a column, get the average
 *	  width of entries in the column.  Return zero if no data available.
 *
 * Currently this is only consulted for individual tables, not for inheritance
 * trees, so we don't need an "inh" parameter.
 *
 * Calling a hook at this point looks somewhat strange, but is required
 * because the optimizer calls this function without any other way for
 * plug-ins to control the result.
 */
int32
get_attavgwidth(Oid relid, AttrNumber attnum)
{
	HeapTuple	tp;
	int32		stawidth;

	if (get_attavgwidth_hook)
	{
		stawidth = (*get_attavgwidth_hook) (relid, attnum);
		if (stawidth > 0)
			return stawidth;
	}
	tp = SearchSysCache3(STATRELATTINH,
						 ObjectIdGetDatum(relid),
						 Int16GetDatum(attnum),
						 BoolGetDatum(false));
	if (HeapTupleIsValid(tp))
	{
		stawidth = ((Form_pg_statistic) GETSTRUCT(tp))->stawidth;
		ReleaseSysCache(tp);
		if (stawidth > 0)
			return stawidth;
	}
	return 0;
}

/*
 * get_attnullfrac
 *
 *	  Given the table and attribute number of a column, get the null
 *	  fraction of entries in the column.  Return zero if no data.
 */
float4
get_attnullfrac(Oid relid, AttrNumber attnum)
{
	HeapTuple	tp;
	float4		stanullfrac;

	tp = SearchSysCache(STATRELATTINH,
						ObjectIdGetDatum(relid),
						Int16GetDatum(attnum),
						BoolGetDatum(false),
						0);
	if (HeapTupleIsValid(tp))
	{
		stanullfrac = ((Form_pg_statistic) GETSTRUCT(tp))->stanullfrac;
		ReleaseSysCache(tp);
		if (stanullfrac > 0.0)
			return stanullfrac;
	}
	return 0.0;
}

/*
 * get_attstatsslot
 *
 *		Extract the contents of a "slot" of a pg_statistic tuple.
 *		Returns true if requested slot type was found, else false.
 *
 * Unlike other routines in this file, this takes a pointer to an
 * already-looked-up tuple in the pg_statistic cache.  We do this since
 * most callers will want to extract more than one value from the cache
 * entry, and we don't want to repeat the cache lookup unnecessarily.
 * Also, this API allows this routine to be used with statistics tuples
 * that have been provided by a stats hook and didn't really come from
 * pg_statistic.
 *
 * sslot: pointer to output area (typically, a local variable in the caller).
 * statstuple: pg_statistic tuple to be examined.
 * reqkind: STAKIND code for desired statistics slot kind.
 * reqop: STAOP value wanted, or InvalidOid if don't care.
 * flags: bitmask of ATTSTATSSLOT_VALUES and/or ATTSTATSSLOT_NUMBERS.
 *
 * If a matching slot is found, true is returned, and *sslot is filled thus:
 * staop: receives the actual STAOP value.
 * stacoll: receives the actual STACOLL value.
 * valuetype: receives actual datatype of the elements of stavalues.
 * values: receives pointer to an array of the slot's stavalues.
 * nvalues: receives number of stavalues.
 * numbers: receives pointer to an array of the slot's stanumbers (as float4).
 * nnumbers: receives number of stanumbers.
 *
 * valuetype/values/nvalues are InvalidOid/NULL/0 if ATTSTATSSLOT_VALUES
 * wasn't specified.  Likewise, numbers/nnumbers are NULL/0 if
 * ATTSTATSSLOT_NUMBERS wasn't specified.
 *
 * If no matching slot is found, false is returned, and *sslot is zeroed.
 *
 * Note that the current API doesn't allow for searching for a slot with
 * a particular collation.  If we ever actually support recording more than
 * one collation, we'll have to extend the API, but for now simple is good.
 *
 * The data referred to by the fields of sslot is locally palloc'd and
 * is independent of the original pg_statistic tuple.  When the caller
 * is done with it, call free_attstatsslot to release the palloc'd data.
 *
 * If it's desirable to call free_attstatsslot when get_attstatsslot might
 * not have been called, memset'ing sslot to zeroes will allow that.
 */
bool
get_attstatsslot(AttStatsSlot *sslot, HeapTuple statstuple,
				 int reqkind, Oid reqop, int flags)
{
	Form_pg_statistic stats = (Form_pg_statistic) GETSTRUCT(statstuple);
	int			i;
	Datum		val;
	bool		isnull;
	ArrayType  *statarray;
	Oid			arrayelemtype;
	int			narrayelem;
	HeapTuple	typeTuple;
	Form_pg_type typeForm;

	/* initialize *sslot properly */
	memset(sslot, 0, sizeof(AttStatsSlot));

	for (i = 0; i < STATISTIC_NUM_SLOTS; i++)
	{
		if ((&stats->stakind1)[i] == reqkind &&
			(reqop == InvalidOid || (&stats->staop1)[i] == reqop))
			break;
	}
	if (i >= STATISTIC_NUM_SLOTS)
		return false;			/* not there */

	sslot->staop = (&stats->staop1)[i];
	sslot->stacoll = (&stats->stacoll1)[i];

	/*
	 * XXX Hopefully-temporary hack: if stacoll isn't set, inject the default
	 * collation.  This won't matter for non-collation-aware datatypes.  For
	 * those that are, this covers cases where stacoll has not been set.  In
	 * the short term we need this because some code paths involving type NAME
	 * do not pass any collation to prefix_selectivity and related functions.
	 * Even when that's been fixed, it's likely that some add-on typanalyze
	 * functions won't get the word right away about filling stacoll during
	 * ANALYZE, so we'll probably need this for awhile.
	 */
	if (sslot->stacoll == InvalidOid)
		sslot->stacoll = DEFAULT_COLLATION_OID;

	if (flags & ATTSTATSSLOT_VALUES)
	{
		val = SysCacheGetAttr(STATRELATTINH, statstuple,
							  Anum_pg_statistic_stavalues1 + i,
							  &isnull);
		if (isnull)
			elog(ERROR, "stavalues is null");

		/*
		 * Detoast the array if needed, and in any case make a copy that's
		 * under control of this AttStatsSlot.
		 */
		statarray = DatumGetArrayTypePCopy(val);

		/*
		 * Extract the actual array element type, and pass it back in case the
		 * caller needs it.
		 */
		sslot->valuetype = arrayelemtype = ARR_ELEMTYPE(statarray);

		/* Need info about element type */
		typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(arrayelemtype));
		if (!HeapTupleIsValid(typeTuple))
			elog(ERROR, "cache lookup failed for type %u", arrayelemtype);
		typeForm = (Form_pg_type) GETSTRUCT(typeTuple);

		/* Deconstruct array into Datum elements; NULLs not expected */
		deconstruct_array(statarray,
						  arrayelemtype,
						  typeForm->typlen,
						  typeForm->typbyval,
						  typeForm->typalign,
						  &sslot->values, NULL, &sslot->nvalues);

		/*
		 * If the element type is pass-by-reference, we now have a bunch of
		 * Datums that are pointers into the statarray, so we need to keep
		 * that until free_attstatsslot.  Otherwise, all the useful info is in
		 * sslot->values[], so we can free the array object immediately.
		 */
		if (!typeForm->typbyval)
			sslot->values_arr = statarray;
		else
			pfree(statarray);

		ReleaseSysCache(typeTuple);
	}

	if (flags & ATTSTATSSLOT_NUMBERS)
	{
		val = SysCacheGetAttr(STATRELATTINH, statstuple,
							  Anum_pg_statistic_stanumbers1 + i,
							  &isnull);
		if (isnull)
			elog(ERROR, "stanumbers is null");

		/*
		 * Detoast the array if needed, and in any case make a copy that's
		 * under control of this AttStatsSlot.
		 */
		statarray = DatumGetArrayTypePCopy(val);

		narrayelem = ARR_DIMS(statarray)[0];
		if (ARR_NDIM(statarray) != 1 || narrayelem <= 0 ||
			ARR_HASNULL(statarray) ||
			ARR_ELEMTYPE(statarray) != FLOAT4OID)
			elog(ERROR, "stanumbers is not a 1-D float4 array");

		/* Give caller a pointer directly into the statarray */
		sslot->numbers = (float4 *) ARR_DATA_PTR(statarray);
		sslot->nnumbers = narrayelem;

		/* We'll free the statarray in free_attstatsslot */
		sslot->numbers_arr = statarray;
	}

	return true;
}

/*
 * free_attstatsslot
 *		Free data allocated by get_attstatsslot
 */
void
free_attstatsslot(AttStatsSlot *sslot)
{
	/* The values[] array was separately palloc'd by deconstruct_array */
	if (sslot->values)
		pfree(sslot->values);
	/* The numbers[] array points into numbers_arr, do not pfree it */
	/* Free the detoasted array objects, if any */
	if (sslot->values_arr)
		pfree(sslot->values_arr);
	if (sslot->numbers_arr)
		pfree(sslot->numbers_arr);
}

/*
 * get_att_stats
 *		Get attribute statistics. Return a copy of the HeapTuple object, or NULL
 *		if no stats found for attribute
 * 
 */
HeapTuple
get_att_stats(Oid relid, AttrNumber attrnum)
{
	HeapTuple result;

	/*
	 * This is used by ORCA, and ORCA doesn't know that there are two different kinds of stats,
	 * the inherited stats and the non-inherited. Use the inherited stats, i.e. stats that
	 * cover all the child tables, too, if available.
	 */
	result = SearchSysCacheCopy3(STATRELATTINH,
								 ObjectIdGetDatum(relid),
								 Int16GetDatum(attrnum),
								 BoolGetDatum(true));
	if (!result)
		result = SearchSysCacheCopy3(STATRELATTINH,
									 ObjectIdGetDatum(relid),
									 Int16GetDatum(attrnum),
									 BoolGetDatum(false));

	return result;
}

/*				---------- PG_NAMESPACE CACHE ----------				 */

/*
 * get_namespace_name
 *		Returns the name of a given namespace
 *
 * Returns a palloc'd copy of the string, or NULL if no such namespace.
 */
char *
get_namespace_name(Oid nspid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(nspid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_namespace nsptup = (Form_pg_namespace) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(nsptup->nspname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

/*
 * get_namespace_name_or_temp
 *		As above, but if it is this backend's temporary namespace, return
 *		"pg_temp" instead.
 */
char *
get_namespace_name_or_temp(Oid nspid)
{
	if (isTempNamespace(nspid))
		return "pg_temp";
	else
		return get_namespace_name(nspid);
}

/*				---------- PG_RANGE CACHE ----------				 */

/*
 * get_range_subtype
 *		Returns the subtype of a given range type
 *
 * Returns InvalidOid if the type is not a range type.
 */
Oid
get_range_subtype(Oid rangeOid)
{
	HeapTuple	tp;

	tp = SearchSysCache1(RANGETYPE, ObjectIdGetDatum(rangeOid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_range rngtup = (Form_pg_range) GETSTRUCT(tp);
		Oid			result;

		result = rngtup->rngsubtype;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * relation_exists
 *	  Is there a relation with the given oid
 */
bool
relation_exists(Oid oid)
{
	return SearchSysCacheExists(RELOID, oid, 0, 0, 0);
}

/*
 * index_exists
 *	  Is there an index with the given oid
 */
bool
index_exists(Oid oid)
{
	return SearchSysCacheExists(INDEXRELID, oid, 0, 0, 0);
}

/*
 * type_exists
 *	  Is there a type with the given oid
 */
bool
type_exists(Oid oid)
{
	return SearchSysCacheExists(TYPEOID, oid, 0, 0, 0);
}

/*
 * operator_exists
 *	  Is there an operator with the given oid
 */
bool
operator_exists(Oid oid)
{
	return SearchSysCacheExists(OPEROID, oid, 0, 0, 0);
}

/*
 * function_exists
 *	  Is there a function with the given oid
 */
bool
function_exists(Oid oid)
{
	return SearchSysCacheExists(PROCOID, oid, 0, 0, 0);
}

/*
 * aggregate_exists
 *	  Is there an aggregate with the given oid
 */
bool
aggregate_exists(Oid oid)
{
	return SearchSysCacheExists(AGGFNOID, oid, 0, 0, 0);
}

// Get oid of aggregate with given name and argument type
Oid
get_aggregate(const char *aggname, Oid oidType)
{
	CatCList   *catlist;
	int			i;
	Oid			oidResult;

	// lookup pg_proc for functions with the given name and arg type
	catlist = SearchSysCacheList1(PROCNAMEARGSNSP,
								  CStringGetDatum((char *) aggname));

	oidResult = InvalidOid;
	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple htup = &catlist->members[i]->tuple;
		Form_pg_proc proctuple = (Form_pg_proc) GETSTRUCT(htup);
		Oid oidProc = proctuple->oid;

		// skip functions with the wrong number of type of arguments
		if (1 != proctuple->pronargs || oidType != proctuple->proargtypes.values[0])
		{
			continue;
		}

		if (SearchSysCacheExists(AGGFNOID, ObjectIdGetDatum(oidProc), 0, 0, 0))
		{
			oidResult = oidProc;
			break;
		}
	}

	ReleaseSysCacheList(catlist);

	return oidResult;
}

/*
 * trigger_exists
 *	  Is there a trigger with the given oid
 */
bool
trigger_exists(Oid oid)
{
	ScanKeyData	scankey;
	Relation	rel;
	SysScanDesc sscan;
	bool		result;

	ScanKeyInit(&scankey, Anum_pg_trigger_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oid));

	rel = table_open(TriggerRelationId, AccessShareLock);
	sscan = systable_beginscan(rel, TriggerOidIndexId, true,
							   NULL, 1, &scankey);

	result = (systable_getnext(sscan) != NULL);

	systable_endscan(sscan);

	table_close(rel, AccessShareLock);

	return result;
}

/*
 * get_relation_keys
 *	  Return a list of relation keys
 */
List *
get_relation_keys(Oid relid)
{
	List *keys = NIL;

	// lookup unique constraints for relation from the catalog table
	ScanKeyData skey[1];

	Relation rel = table_open(ConstraintRelationId, AccessShareLock);
	SysScanDesc scan;
	HeapTuple	htup;

	ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, relid);
	scan = systable_beginscan(rel, ConstraintRelidTypidNameIndexId, true,
							  NULL, 1, skey);

	while (HeapTupleIsValid(htup = systable_getnext(scan)))
	{
		Form_pg_constraint contuple = (Form_pg_constraint) GETSTRUCT(htup);

		// skip non-unique constraints
		if (CONSTRAINT_UNIQUE != contuple->contype &&
			CONSTRAINT_PRIMARY != contuple->contype)
		{
			continue;
		}
			
		// store key set in an array
		List *key = NIL;
		
		bool null = false;
		Datum dat = 
				heap_getattr(htup, Anum_pg_constraint_conkey, RelationGetDescr(rel), &null);
		
		Datum *dats = NULL;
		int numKeys = 0;

		// extract key elements
		deconstruct_array(DatumGetArrayTypeP(dat), INT2OID, 2, true, 's', &dats, NULL, &numKeys);
			
		for (int i = 0; i < numKeys; i++)
		{
			int16 key_elem =  DatumGetInt16(dats[i]);
			key = lappend_int(key, key_elem);
		}
		
		keys = lappend(keys, key);
	}

	systable_endscan(scan);
	table_close(rel, AccessShareLock);

	return keys;
}

/*
 * check_constraint_exists
 *	  Is there a check constraint with the given oid
 */
bool
check_constraint_exists(Oid oidCheckconstraint)
{
	return SearchSysCacheExists1(CONSTROID, ObjectIdGetDatum(oidCheckconstraint));
}

/*
 * get_check_constraint_relid
 *		Given check constraint id, return the check constraint's relation oid
 */
Oid
get_check_constraint_relid(Oid oidCheckconstraint)
{
	HeapTuple	tp;

	tp = SearchSysCache(CONSTROID,
						ObjectIdGetDatum(oidCheckconstraint),
						0, 0, 0);
	if (HeapTupleIsValid(tp))
	{
		Form_pg_constraint contup = (Form_pg_constraint) GETSTRUCT(tp);
		Oid			result;

		result = contup->conrelid;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

/*
 * get_check_constraint_oids
 *	 Extract all check constraint oid for a given relation.
 */
List *
get_check_constraint_oids(Oid oidRel)
{
	List	   *plConstraints = NIL;
	HeapTuple	htup;
	Relation	conrel;
	ScanKeyData scankey;
	SysScanDesc sscan;

	/*
	 * lookup constraints for relation from the catalog table
	 *
	 * SELECT * FROM pg_constraint WHERE conrelid = :1
	 */
	conrel = table_open(ConstraintRelationId, AccessShareLock);

	ScanKeyInit(&scankey,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oidRel));
	sscan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true,
							   NULL, 1, &scankey);

	while (HeapTupleIsValid(htup = systable_getnext(sscan)))
	{
		Form_pg_constraint contuple = (Form_pg_constraint) GETSTRUCT(htup);

		// only consider check constraints
		if (CONSTRAINT_CHECK != contuple->contype || !contuple->convalidated)
		{
			continue;
		}

		plConstraints = lappend_oid(plConstraints, contuple->oid);
	}

	systable_endscan(sscan);
	table_close(conrel, AccessShareLock);

	return plConstraints;
}

/*
 * get_check_constraint_name
 *        returns the name of the check constraint with the given oidConstraint.
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such constraint.
 */
char *
get_check_constraint_name(Oid oidCheckconstraint)
{
	return get_constraint_name(oidCheckconstraint);
}

/*
 * get_check_constraint_expr_tree
 *        returns the expression node tree representing the check constraint
 *        with the given oidConstraint.
 *
 * Note: returns a palloc'd expression node tree, or NULL if no such constraint.
 */
Node *
get_check_constraint_expr_tree(Oid oidCheckconstraint)
{
	HeapTuple	tp;
	Node	   *result = NULL;

	tp = SearchSysCache(CONSTROID,
						ObjectIdGetDatum(oidCheckconstraint),
						0, 0, 0);
	if (HeapTupleIsValid(tp))
	{
		Datum		conbin;
		bool		isnull;

		conbin = SysCacheGetAttr(CONSTROID, tp,
								 Anum_pg_constraint_conbin, &isnull);
		if (!isnull)
			result = stringToNode(TextDatumGetCString(conbin));

		ReleaseSysCache(tp);
	}
	return result;
}

/*
 * get_cast_func
 *        finds the cast function between the given source and destination type,
 *        and records its oid and properties in the output parameters.
 *        Returns true if a cast exists, false otherwise.
 */
bool
get_cast_func(Oid oidSrc, Oid oidDest, bool *is_binary_coercible, Oid *oidCastFunc, CoercionPathType *pathtype)
{
	if (IsBinaryCoercible(oidSrc, oidDest))
	{
		*is_binary_coercible = true;
		*oidCastFunc = 0;
		return true;
	}
	
	*is_binary_coercible = false;

	*pathtype = find_coercion_pathway(oidDest, oidSrc, COERCION_IMPLICIT, oidCastFunc);
	if (*pathtype == COERCION_PATH_RELABELTYPE)
		*is_binary_coercible = true;
	if (*pathtype != COERCION_PATH_NONE)
		return true;
	return false;
}

/*
 * get_comparison_type
 *      Retrieve comparison type  
 */
CmpType
get_comparison_type(Oid oidOp)
{
	OpBtreeInterpretation *opBti;
	List	   *opBtis;

	opBtis = get_op_btree_interpretation(oidOp);

	if (opBtis == NIL)
	{
		/* The operator does not belong to any B-tree operator family */
		return CmptOther;
	}

	/*
	 * XXX: Arbitrarily use the first found operator family. Usually
	 * there is only one, but e.g. if someone has created a reverse ordering
	 * family that sorts in descending order, it is ambiguous whether a
	 * < operator stands for the less than operator of the ascending opfamily,
	 * or the greater than operator for the descending opfamily.
	 */
	opBti = (OpBtreeInterpretation*)linitial(opBtis);

	switch(opBti->strategy)
	{
		case BTLessStrategyNumber:
			return CmptLT;
		case BTLessEqualStrategyNumber:
			return CmptLEq;
		case BTEqualStrategyNumber:
			return CmptEq;
		case BTGreaterEqualStrategyNumber:
			return CmptGEq;
		case BTGreaterStrategyNumber:
			return CmptGT;
		case ROWCOMPARE_NE:
			return CmptNEq;
		default:
			elog(ERROR, "unknown B-tree strategy: %d", opBti->strategy);
			return CmptOther;
	}
}

/*
 * get_comparison_operator
 *      Retrieve comparison operator between given types  
 */
Oid
get_comparison_operator(Oid oidLeft, Oid oidRight, CmpType cmpt)
{
	int16		opstrat;
	HeapTuple	ht;
	Oid			result = InvalidOid;
	Relation	pg_amop;
	ScanKeyData scankey[4];
	SysScanDesc sscan;

	switch(cmpt)
	{
		case CmptLT:
			opstrat = BTLessStrategyNumber;
			break;
		case CmptLEq:
			opstrat = BTLessEqualStrategyNumber;
			break;
		case CmptEq:
			opstrat = BTEqualStrategyNumber;
			break;
		case CmptGEq:
			opstrat = BTGreaterEqualStrategyNumber;
			break;
		case CmptGT:
			opstrat = BTGreaterStrategyNumber;
			break;
		default:
			return InvalidOid;
	}

	pg_amop = heap_open(AccessMethodOperatorRelationId, AccessShareLock);

	/*
	 * SELECT * FROM pg_amop
	 * WHERE amoplefttype = :1 and amoprighttype = :2 and amopmethod = :3 and amopstrategy = :4
	 */
	ScanKeyInit(&scankey[0],
				Anum_pg_amop_amoplefttype,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oidLeft));
	ScanKeyInit(&scankey[1],
				Anum_pg_amop_amoprighttype,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oidRight));
	ScanKeyInit(&scankey[2],
				Anum_pg_amop_amopmethod,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(BTREE_AM_OID));
	ScanKeyInit(&scankey[3],
				Anum_pg_amop_amopstrategy,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(opstrat));

	/* XXX: There is no index for this, so this is slow! */
	sscan = systable_beginscan(pg_amop, InvalidOid, false,
							   NULL, 4, scankey);

	/* XXX: There can be multiple results. Arbitrarily use the first one */
	while (HeapTupleIsValid(ht = systable_getnext(sscan)))
	{
		Form_pg_amop amoptup = (Form_pg_amop) GETSTRUCT(ht);

		result = amoptup->amopopr;
		break;
	}

	systable_endscan(sscan);
	heap_close(pg_amop, AccessShareLock);

	return result;
}

/*
 * has_subclass_slow
 *
 * Performs the exhaustive check whether a relation has a subclass. This is 
 * different from has_subclass(), in that the latter can return true if a relation.
 * *might* have a subclass. See comments in has_subclass() for more details.
 */
bool
has_subclass_slow(Oid relationId)
{
	ScanKeyData	scankey;
	Relation	rel;
	SysScanDesc sscan;
	bool		result;

	if (!has_subclass(relationId))
	{
		return false;
	}

	rel = heap_open(InheritsRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_inherits_inhparent,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relationId));

	/* no index on inhparent */
	sscan = systable_beginscan(rel, InvalidOid, false,
							   NULL, 1, &scankey);

	result = (systable_getnext(sscan) != NULL);

	systable_endscan(sscan);

	heap_close(rel, AccessShareLock);

	return result;
}

/*
 * get_operator_opfamilies
 *		Get the oid of operator families the given operator belongs to
 *
 * ORCA calls this.
 */
List *
get_operator_opfamilies(Oid opno)
{
	List	   *opfam_oids;
	CatCList   *catlist;
	int			i;

	opfam_oids = NIL;

	/* SELECT * FROM pg_amop WHERE amopopr = :1 */
	catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));
	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	htup = &catlist->members[i]->tuple;
		Form_pg_amop amop_tuple = (Form_pg_amop) GETSTRUCT(htup);

		opfam_oids = lappend_oid(opfam_oids, amop_tuple->amopfamily);
	}

	ReleaseSysCacheList(catlist);

	return opfam_oids;
} 

/*
 * get_index_opfamilies
 *		Get the oid of operator families for the index keys
 */
List *
get_index_opfamilies(Oid oidIndex)
{
	HeapTuple	htup;
	List	   *opfam_oids;
    bool		isnull = false;
	int			indnkeyatts;
	Datum		indclassDatum;
	oidvector  *indclass;

	htup = SearchSysCache1(INDEXRELID,
						   ObjectIdGetDatum(oidIndex));
	if (!HeapTupleIsValid(htup))
		elog(ERROR, "Index %u not found", oidIndex);

    /*
     * use SysCacheGetAttr() to retrieve number of index attributes, and the oid
	 * vector of indclass
     */
	indnkeyatts = DatumGetInt16(SysCacheGetAttr(INDEXRELID, htup, Anum_pg_index_indnkeyatts, &isnull));
	Assert(!isnull);

    indclassDatum = SysCacheGetAttr(INDEXRELID, htup, Anum_pg_index_indclass, &isnull);
    if (isnull)
		return NIL;
    indclass = (oidvector *) DatumGetPointer(indclassDatum);

	opfam_oids = NIL;
	for (int i = 0; i < indnkeyatts; i++)
	{
		Oid			oidOpClass = indclass->values[i];
		Oid 		opfam = get_opclass_family(oidOpClass);

		opfam_oids = lappend_oid(opfam_oids, opfam);
	}

	ReleaseSysCache(htup);
	return opfam_oids;
}

/*
 *  relation_policy
 *  Return the distribution policy of a table. 
 */
GpPolicy *
relation_policy(Relation rel)
{
	Assert(NULL != rel);

	/* not a partitioned table */
	return rel->rd_cdbpolicy;
}

/*
 *  child_distribution_mismatch
 *  Return true if the table is partitioned and one of its children has a
 *  different distribution policy. The only allowed mismatch is for the parent
 *  to be hash distributed, and its child part to be randomly distributed.
 */
/* GPDB_12_MERGE_FIXME */
#if 0
bool
child_distribution_mismatch(Relation rel)
{
	Assert(NULL != rel);
	if (PART_STATUS_NONE == rel_part_status(rel->rd_id))
	{
		/* not a partitioned table */
		return false;
	}

	GpPolicy *rootPolicy = rel->rd_cdbpolicy;
	Assert(NULL != rootPolicy && "Partitioned tables cannot be master-only");

	/* replicated table can't have child */
	Assert(!GpPolicyIsReplicated(rootPolicy));

	if (GpPolicyIsRandomPartitioned(rootPolicy))
	{
		/* root partition policy already marked as Random, no mismatch possible as
		 * all children must be random as well */
		return false;
	}

	List *child_oids = find_all_inheritors(rel->rd_id, NoLock, NULL);
	ListCell *lc;

	foreach (lc, child_oids)
	{
		Oid oidChild = lfirst_oid(lc);
		Relation relChild = RelationIdGetRelation(oidChild);
		Assert(NULL != relChild);

		GpPolicy *childPolicy = relChild->rd_cdbpolicy;

		Assert(!GpPolicyIsReplicated(childPolicy));

		if (GpPolicyIsRandomPartitioned(childPolicy))
		{
			/* child partition is Random, and parent is not */
			RelationClose(relChild);
			return true;
		}

		RelationClose(relChild);
	}

	list_free(child_oids);

	/* all children match the root's distribution policy */
	return false;
}
#endif

/*
 *  child_triggers
 *  Return true if the table is partitioned and any of the child partitions
 *  have a trigger of the given type.
 */
bool
child_triggers(Oid relationId, int32 triggerType)
{
/* GPDB_12_MERGE_FIXME */
	return false;
#if 0
	Assert(InvalidOid != relationId);
	if (PART_STATUS_NONE == rel_part_status(relationId))
	{
		/* not a partitioned table */
		return false;
	}

	List *childOids = find_all_inheritors(relationId, NoLock, NULL);
	ListCell *lc;

	bool found = false;
	foreach (lc, childOids)
	{
		Oid oidChild = lfirst_oid(lc);
		Relation relChild = RelationIdGetRelation(oidChild);
		Assert(NULL != relChild);

		if (relChild->rd_rel->relhastriggers && NULL == relChild->trigdesc)
		{
			RelationBuildTriggers(relChild);
			if (NULL == relChild->trigdesc)
			{
				relChild->rd_rel->relhastriggers = false;
			}
		}

		if (relChild->rd_rel->relhastriggers)
		{
			for (int i = 0; i < relChild->trigdesc->numtriggers && !found; i++)
			{
				Trigger trigger = relChild->trigdesc->triggers[i];
				found = trigger_enabled(trigger.tgoid) &&
						(get_trigger_type(trigger.tgoid) & triggerType) == triggerType;
			}
		}

		RelationClose(relChild);
		if (found)
		{
			break;
		}
	}

	list_free(childOids);
	
	/* no child triggers matching the given type */
	return found;
#endif
}

/*				---------- PG_INDEX CACHE ----------				 */

/*
 * get_index_column_opclass
 *
 *		Given the index OID and column number,
 *		return opclass of the index column
 *			or InvalidOid if the index was not found.
 */
Oid
get_index_column_opclass(Oid index_oid, int attno)
{
	HeapTuple	tuple;
	Form_pg_index rd_index PG_USED_FOR_ASSERTS_ONLY;
	Datum		datum;
	bool		isnull;
	oidvector  *indclass;
	Oid			opclass;

	/* First we need to know the column's opclass. */

	tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(index_oid));
	if (!HeapTupleIsValid(tuple))
		return InvalidOid;

	rd_index = (Form_pg_index) GETSTRUCT(tuple);

	/* caller is supposed to guarantee this */
	Assert(attno > 0 && attno <= rd_index->indnatts);

	datum = SysCacheGetAttr(INDEXRELID, tuple,
							Anum_pg_index_indclass, &isnull);
	Assert(!isnull);

	indclass = ((oidvector *) DatumGetPointer(datum));
	opclass = indclass->values[attno - 1];

	ReleaseSysCache(tuple);

	return opclass;
}

/* GPDB_12_MERGE_FIXME: only used by ORCA. Fix the callers to check
 * Relation->relkind == RELKIND_PARTITIONED_TABLE instead. They should
 * have the relcache entry at hand anyway.
 */
bool
relation_is_partitioned(Oid relid)
{
	HeapTuple   tuple;
	tuple = SearchSysCache1(PARTRELID, ObjectIdGetDatum(relid));

	if (HeapTupleIsValid(tuple))
	{
		ReleaseSysCache(tuple);
		return true;
	}
	else
		return false;
}

List *
relation_get_leaf_partitions(Oid oid)
{
	List *descendants = find_all_inheritors(oid, AccessShareLock, NULL);
	List *leaves = NIL;
	ListCell *lc;
	foreach(lc, descendants)
	{
		const Oid descendant = lfirst_oid(lc);
		if (get_rel_relkind(descendant) != RELKIND_PARTITIONED_TABLE)
			leaves = lappend_oid(leaves, descendant);
	}
	return leaves;
}
