/*-------------------------------------------------------------------------
 *
 * predtest_valueset.c
 *	  Routines to prove possible values of variable based on predicates.
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/predtest_valueset.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "catalog/pg_type.h"
#include "optimizer/clauses.h"
#include "optimizer/predtest_valueset.h"
#include "utils/lsyscache.h"
#include "nodes/makefuncs.h"
#include "utils/datum.h"


#define INT16MAX (32767)
#define INT16MIN (-32768)
#define INT32MAX (2147483647)
#define INT32MIN (-2147483648)

static HTAB *CreateNodeSetHashTable();
static void AddValue(PossibleValueSet *pvs, Const *valueToCopy);
static void RemoveValue(PossibleValueSet *pvs, Const *value);
static bool ContainsValue(PossibleValueSet *pvs, Const *value);

static bool TryProcessOpExprForPossibleValues(OpExpr *expr, Node *variable, Oid opfamily, PossibleValueSet *resultOut);
static bool TryProcessNullTestForPossibleValues(NullTest *expr, Node *variable, PossibleValueSet *resultOut);

typedef struct ConstHashValue
{
	Const * c;
} ConstHashValue;

/*
 * Compute a hash value for Const.
 *
 * We use equal() to check for equality, which just comparse the raw bytes.
 * Therefore, we don't need datatype-aware hashing either, we just hash the
 * raw bytes.
 */
static uint32
ConstHashTableHash(const void *keyPtr, Size keysize)
{
	Const	   *c = *((Const **) keyPtr);

	if (c->constisnull)
		return 0;
	else if (c->constbyval)
	{
		/*
		 * Like in datumIsEqual, we use all bits of the Datum, even if the
		 * length is smaller. This assumes that the datatype is consistent
		 * about how it fills the extra bits.
		 */
		return hash_any((unsigned char *) &c->constvalue, sizeof(Datum));
	}
	else
	{
		Size		realSize;

		realSize = datumGetSize(c->constvalue, c->constbyval, c->constlen);

		return hash_any((unsigned char *) DatumGetPointer(c->constvalue), realSize);
	}
}

static int
ConstHashTableMatch(const void*keyPtr1, const void *keyPtr2, Size keysize)
{
	Node	   *left = *((Node **) keyPtr1);
	Node	   *right = *((Node **) keyPtr2);

	return equal(left, right) ? 0 : 1;
}

/**
 * returns a hashtable that can be used to map from a node to itself
 */
static HTAB*
CreateNodeSetHashTable(MemoryContext memoryContext)
{
	HASHCTL	hash_ctl;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));

	hash_ctl.keysize = sizeof(Const **);
	hash_ctl.entrysize = sizeof(ConstHashValue);
	hash_ctl.hash = ConstHashTableHash;
	hash_ctl.match = ConstHashTableMatch;
	hash_ctl.hcxt = memoryContext;

	return hash_create("ConstantSet", 16, &hash_ctl,
					   HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);
}

/**
 * basic operation on PossibleValueSet:  initialize to "any value possible"
 */
void
InitPossibleValueSetData(PossibleValueSet *pvs)
{
	pvs->memoryContext = NULL;
	pvs->set = NULL;
	pvs->isAnyValuePossible = true;
}

/**
 * Take the values from the given PossibleValueSet and return them as an allocated array.
 *
 * @param pvs the set to turn into an array
 * @param numValuesOut receives the length of the returned array
 * @return the array of Node objects
 */
Node **
GetPossibleValuesAsArray(PossibleValueSet *pvs, int *numValuesOut)
{
	HASH_SEQ_STATUS status;
	ConstHashValue *value;
	List	   *list = NIL;
	Node	  **result;
	int			numValues,
				i;
	ListCell   *lc;

	if (pvs->set == NULL)
	{
		*numValuesOut = 0;
		return NULL;
	}

	hash_seq_init(&status, pvs->set);
	while ((value = (ConstHashValue *) hash_seq_search(&status)) != NULL)
	{
		list = lappend(list, copyObject(value->c));
	}

	numValues = list_length(list);
	result = palloc(sizeof(Node *) * numValues);
	foreach_with_count(lc, list, i)
	{
		result[i] = (Node *) lfirst(lc);
	}

	*numValuesOut = numValues;
	return result;
}

/**
 * basic operation on PossibleValueSet:  cleanup
 */
void
DeletePossibleValueSetData(PossibleValueSet *pvs)
{
	if (pvs->set != NULL)
	{
		Assert(pvs->memoryContext != NULL);

		MemoryContextDelete(pvs->memoryContext);
		pvs->memoryContext = NULL;
		pvs->set = NULL;
	}
	pvs->isAnyValuePossible = true;
}

/**
 * basic operation on PossibleValueSet:  add a value to the set field of PossibleValueSet
 *
 * The caller must verify that the valueToCopy is greenplum hashable
 */
static void
AddValue(PossibleValueSet *pvs, Const *valueToCopy)
{
	if (pvs->set == NULL)
	{
		Assert(pvs->memoryContext == NULL);

		pvs->memoryContext = AllocSetContextCreate(CurrentMemoryContext,
												   "PossibleValueSet",
												   ALLOCSET_DEFAULT_MINSIZE,
												   ALLOCSET_DEFAULT_INITSIZE,
												   ALLOCSET_DEFAULT_MAXSIZE);
		pvs->set = CreateNodeSetHashTable(pvs->memoryContext);
	}

	if (!ContainsValue(pvs, valueToCopy))
	{
		bool		found; /* unused but needed in call */
		MemoryContext oldContext = MemoryContextSwitchTo(pvs->memoryContext);

		Const	   *key = copyObject(valueToCopy);
		ConstHashValue *entry = hash_search(pvs->set, &key, HASH_ENTER, &found);

		entry->c = key;

		MemoryContextSwitchTo(oldContext);
	}
}

static void
SetToNoValuesPossible(PossibleValueSet *pvs)
{
	if (pvs->memoryContext)
	{
		MemoryContextDelete(pvs->memoryContext);
	}
	pvs->memoryContext = AllocSetContextCreate(CurrentMemoryContext,
												   "PossibleValueSet",
												   ALLOCSET_DEFAULT_MINSIZE,
												   ALLOCSET_DEFAULT_INITSIZE,
												   ALLOCSET_DEFAULT_MAXSIZE);
	pvs->set = CreateNodeSetHashTable(pvs->memoryContext);
	pvs->isAnyValuePossible = false;
}

/**
 * basic operation on PossibleValueSet:  remove a value from the set field of PossibleValueSet
 */
static void
RemoveValue(PossibleValueSet *pvs, Const *value)
{
	bool		found; /* unused, needed in call */

	Assert( pvs->set != NULL);
	hash_search(pvs->set, &value, HASH_REMOVE, &found);
}

/**
 * basic operation on PossibleValueSet:  determine if a value is contained in the set field of PossibleValueSet
 */
static bool
ContainsValue(PossibleValueSet *pvs, Const *value)
{
	bool		found = false;

	Assert(!pvs->isAnyValuePossible);
	if (pvs->set != NULL)
		hash_search(pvs->set, &value, HASH_FIND, &found);
	return found;
}

/**
 * in-place union operation
 */
void
AddUnmatchingValues( PossibleValueSet *pvs, PossibleValueSet *toCheck )
{
	HASH_SEQ_STATUS status;
	ConstHashValue *value;

	Assert(!pvs->isAnyValuePossible);
	Assert(!toCheck->isAnyValuePossible);

	hash_seq_init(&status, toCheck->set);
	while ((value = (ConstHashValue *) hash_seq_search(&status)) != NULL)
	{
		AddValue(pvs, value->c);
	}
}

/**
 * in-place intersection operation
 */
void
RemoveUnmatchingValues(PossibleValueSet *pvs, PossibleValueSet *toCheck)
{
	List	   *toRemove = NIL;
	ListCell   *lc;
	HASH_SEQ_STATUS status;
	ConstHashValue *value;

	Assert(!pvs->isAnyValuePossible);
	Assert(!toCheck->isAnyValuePossible);

	hash_seq_init(&status, pvs->set);
	while ((value = (ConstHashValue*) hash_seq_search(&status)) != NULL)
	{
		if (!ContainsValue(toCheck, value->c))
		{
			toRemove = lappend(toRemove, value->c);
		}
	}

	/* remove after so we don't mod hashtable underneath iteration */
	foreach(lc, toRemove)
	{
		Const	   *value = (Const*) lfirst(lc);

		RemoveValue(pvs, value);
	}
	list_free(toRemove);
}

/**
 * Check to see if the given expression is a valid equality between the listed variable and a constant.
 *
 * @param expr the expression to check for being a valid quality
 * @param variable the variable to look for
 * @param resultOut will be updated with the modified values
 */
bool
TryProcessExprForPossibleValues(Node *expr, Node *variable, Oid opfamily, PossibleValueSet *resultOut)
{
	if (IsA(expr, OpExpr))
	{
		return TryProcessOpExprForPossibleValues((OpExpr *) expr, variable, opfamily, resultOut);
	}
	else if (IsA(expr, NullTest))
	{
		return TryProcessNullTestForPossibleValues((NullTest *) expr, variable, resultOut);
	}
	else
		return false;
}

static bool
TryProcessOpExprForPossibleValues(OpExpr *expr, Node *variable, Oid opfamily, PossibleValueSet *resultOut)
{
	Node	   *leftop,
			   *rightop,
			   *varExpr;
	Const	   *constExpr;
	bool		constOnRight;
	List	   *clause_op_infos;
	ListCell   *lc;

	InitPossibleValueSetData(resultOut);

	leftop = get_leftop((Expr *) expr);
	rightop = get_rightop((Expr *) expr);
	if (!leftop || !rightop)
		return false;

	/* check if one operand is a constant */
	if (IsA(rightop, Const))
	{
		varExpr = leftop;
		constExpr = (Const *) rightop;
		constOnRight = true;
	}
	else if (IsA(leftop, Const))
	{
		constExpr = (Const *) leftop;
		varExpr = rightop;
		constOnRight = false;
	}
	else
	{
		/** not a constant?  Learned nothing */
		return false;
	}

	if (constExpr->constisnull)
	{
		/* null doesn't help us */
		return false;
	}

	if (IsA(varExpr, RelabelType))
	{
		RelabelType *rt = (RelabelType *) varExpr;

		varExpr = (Node *) rt->arg;
	}

	if (!equal(varExpr, variable))
	{
		/**
		 * Not talking about our variable?  Learned nothing
		 */
		return false;
	}

	/*
	 * if expr->opno does not belong to opfamily of distributed key,
	 * do not use direct dispatch to avoid wrong results.
	 * more details please refer to https://github.com/greenplum-db/gpdb/issues/14887
	 */
	if (!op_in_opfamily(expr->opno, opfamily))
	{
		return false;
	}

	clause_op_infos = get_op_btree_interpretation(expr->opno);

	/* check if it's equality operation */
	bool		is_equality = false;
	OpBtreeInterpretation *clause_op_info = NULL;

	foreach (lc, clause_op_infos)
	{
		clause_op_info = lfirst(lc);

		if (clause_op_info->strategy == BTEqualStrategyNumber)
		{
			is_equality = true;
			break;
		}
	}

	if (!is_equality)
		return false;

	if (clause_op_info->oplefttype == clause_op_info->oprighttype)
	{
		/*
		 * Both sides have the same datatype. This case is simple, we can
		 * add the constant directly as a possible value.
		 */
		resultOut->isAnyValuePossible = false;
		AddValue(resultOut, constExpr);
		return true;
	}
	else
	{
		/*
		 * Cross-datatype equality. this is more complicated, we need to
		 * construct a const with constant's value, but using the variable
		 * side's datatype. it would be tempting to cast the value, but that
		 * fails e.g. if the constant is an int4, with a large value, and
		 * the variable is int2. there will be no match in that case, but
		 * if we tried to simply cast the value, we'd get an error. So we
		 * have hard-coded knowledge of the few simple cases, namely
		 * differently-sized integer types
		 *
		 * Expand the constant to int64, then assign it back to a Datum
		 * of the variable's datatype, checking for overflow.
		 */
		Oid			const_type;
		Oid			var_type;
		int64		constvalue;

		bool		isOverflow;
		Oid			new_const_type;
		int32		new_const_len;
		Datum		new_const_datum;

		if (constOnRight)
		{
			const_type = clause_op_info->oprighttype;
			var_type = clause_op_info->oplefttype;
		}
		else
		{
			const_type = clause_op_info->oplefttype;
			var_type = clause_op_info->oprighttype;
		}

		switch (const_type)
		{
			case INT2OID:
				constvalue = (int64) DatumGetInt16(constExpr->constvalue);
				break;
			case INT4OID:
				constvalue = (int64) DatumGetInt32(constExpr->constvalue);
				break;
			case INT8OID:
				constvalue = DatumGetInt64(constExpr->constvalue);
				break;
			default:
				return false; /* we only know how to handle integer types */
		}

		/* Assign to the target type, checking for overflow */
		isOverflow = false;
		switch (var_type)
		{
			case INT2OID:
				if (constvalue > INT16MAX || constvalue < INT16MIN)
					isOverflow = true;
				new_const_type = INT2OID;
				new_const_len = sizeof(int16);
				new_const_datum = Int16GetDatum((int16) constvalue);
				break;
			case INT4OID:
				if (constvalue > INT32MAX || constvalue < INT32MIN)
					isOverflow = true;
				new_const_type = INT4OID;
				new_const_len = sizeof(int32);
				new_const_datum = Int32GetDatum((int32) constvalue);
				break;
			case INT8OID:
				new_const_type = INT8OID;
				new_const_len = sizeof(int64);
				new_const_datum = Int64GetDatum(constvalue);
				break;

			default:
				return false; /* we only know how to handle integer types */
		}

		if (isOverflow)
		{
			SetToNoValuesPossible(resultOut);
		}
		else
		{
			/* okay, got a new constant value .. set it and done! */
			Const	   *newConst;

			newConst = makeConst(new_const_type,
								 /* consttypmod */ 0,
								 /* constcollid */ InvalidOid,
								 new_const_len,
								 new_const_datum,
								 /* constisnull */ false,
								 /* constbyval */ true);

			resultOut->isAnyValuePossible = false;
			AddValue(resultOut, newConst);

			pfree(newConst);
		}
		return true;
	}
}

static bool
TryProcessNullTestForPossibleValues(NullTest *expr, Node *variable, PossibleValueSet *resultOut)
{
	Node	   *varExpr;

	InitPossibleValueSetData(resultOut);

	varExpr = (Node *) expr->arg;

	if (IsA(varExpr, RelabelType))
	{
		RelabelType *rt = (RelabelType *) varExpr;

		varExpr = (Node *) rt->arg;
	}

	if (!equal(varExpr, variable))
	{
		/**
		 * Not talking about our variable?  Learned nothing
		 */
		return false;
	}

	if (expr->nulltesttype == IS_NULL)
	{
		resultOut->isAnyValuePossible = false;
		AddValue(resultOut, makeNullConst(exprType(varExpr),
										  -1,
										  exprCollation(varExpr)));
		return true;
	}
	else
	{
		/* can't do anything with IS NOT NULL */
		return false;
	}
}
