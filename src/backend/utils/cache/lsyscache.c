/*-------------------------------------------------------------------------
 *
 * lsyscache.c
 *	  Convenience routines for common queries in the system catalog cache.
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/cache/lsyscache.c,v 1.138.2.1 2007/03/19 16:30:40 tgl Exp $
 *
 * NOTES
 *	  Eventually, the index information should go through here, too.
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"

#include "access/genam.h" 
#include "bootstrap/bootstrap.h"
#include "catalog/catquery.h"
#include "catalog/heap.h"                   /* SystemAttributeDefinition() */    
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "cdb/cdbpartition.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_clause.h"			/* for sort_op_can_sort() */
#include "parser/parse_coerce.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/fmgroids.h"
#include "funcapi.h"

static Datum
attstatsslot_getattr(cqContext	*pcqCtx, 
					 TupleDesc	 tupdesc,
					 HeapTuple	 statstuple,
					 AttrNumber	 attnum, bool *isnull);



/*				---------- AMOP CACHES ----------						 */

/*
 * op_in_opclass
 *
 *		Return t iff operator 'opno' is in operator class 'opclass'.
 */
bool
op_in_opclass(Oid opno, Oid opclass)
{
	return (caql_getcount(
					NULL,
					cql("SELECT COUNT(*) FROM pg_amop "
						" WHERE amopopr = :1 "
						" AND amopclaid = :2 ",
						ObjectIdGetDatum(opno),
						ObjectIdGetDatum(opclass))) > 0);
}

/*
 * get_op_opclass_strategy
 *
 *		Get the operator's strategy number within the specified opclass,
 *		or 0 if it's not a member of the opclass.
 */
int
get_op_opclass_strategy(Oid opno, Oid opclass)
{
	HeapTuple	tp;
	Form_pg_amop amop_tup;
	int			result;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_amop "
				" WHERE amopopr = :1 "
				" AND amopclaid = :2 ",
				ObjectIdGetDatum(opno),
				ObjectIdGetDatum(opclass)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		return 0;
	amop_tup = (Form_pg_amop) GETSTRUCT(tp);
	result = amop_tup->amopstrategy;

	caql_endscan(pcqCtx);

	return result;
}

/*
 * get_op_opclass_properties
 *
 *		Get the operator's strategy number, subtype, and recheck (lossy) flag
 *		within the specified opclass.
 *
 * Caller should already have verified that opno is a member of opclass,
 * therefore we raise an error if the tuple is not found.
 */
void
get_op_opclass_properties(Oid opno, Oid opclass,
						  int *strategy, Oid *subtype, bool *recheck)
{
	HeapTuple	tp;
	Form_pg_amop amop_tup;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_amop "
				" WHERE amopopr = :1 "
				" AND amopclaid = :2 ",
				ObjectIdGetDatum(opno),
				ObjectIdGetDatum(opclass)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "operator %u is not a member of opclass %u",
			 opno, opclass);
	amop_tup = (Form_pg_amop) GETSTRUCT(tp);
	*strategy = amop_tup->amopstrategy;
	*subtype = amop_tup->amopsubtype;
	*recheck = amop_tup->amopreqcheck;

	caql_endscan(pcqCtx);
}

/*
 * get_opclass_member
 *		Get the OID of the operator that implements the specified strategy
 *		with the specified subtype for the specified opclass.
 *
 * Returns InvalidOid if there is no pg_amop entry for the given keys.
 */
Oid
get_opclass_member(Oid opclass, Oid subtype, int16 strategy)
{
	Oid			result;
	int			fetchCount;

	result  = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT amopopr FROM pg_amop "
				" WHERE amopclaid = :1 "
				" AND amopsubtype = :2 "
				" AND amopstrategy = :3 ",
				ObjectIdGetDatum(opclass),
				ObjectIdGetDatum(subtype),
				Int16GetDatum(strategy)
					));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_op_hash_function
 *		Get the OID of the datatype-specific hash function associated with
 *		a hashable equality operator.
 *
 * Returns InvalidOid if no hash function can be found.  (This indicates
 * that the operator should not have been marked oprcanhash.)
 */
Oid
get_op_hash_function(Oid opno)
{
	CatCList   *catlist;
	int			i;
	Oid			opclass = InvalidOid;

	/*
	 * Search pg_amop to see if the target operator is registered as the "="
	 * operator of any hash opclass.  If the operator is registered in
	 * multiple opclasses, assume we can use the associated hash function from
	 * any one.
	 */
	catlist = caql_begin_CacheList(
			NULL,
			cql("SELECT * FROM pg_amop "
				" WHERE amopopr = :1 "
				" ORDER BY amopopr, "
				" amopclaid ",
				ObjectIdGetDatum(opno)));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop aform = (Form_pg_amop) GETSTRUCT(tuple);

		if (aform->amopstrategy == HTEqualStrategyNumber &&
			opclass_is_hash(aform->amopclaid))
		{
			opclass = aform->amopclaid;
			break;
		}
	}

	caql_end_CacheList(catlist);

	if (OidIsValid(opclass))
	{
		/* Found a suitable opclass, get its default hash support function */
		return get_opclass_proc(opclass, InvalidOid, HASHPROC);
	}

	/* Didn't find a match... */
	return InvalidOid;
}

/*
 * get_op_btree_interpretation
 *		Given an operator's OID, find out which btree opclasses it belongs to,
 *		and what strategy number it has within each one.  The results are
 *		returned as an OID list and a parallel integer list.
 *
 * In addition to the normal btree operators, we consider a <> operator to be
 * a "member" of an opclass if its negator is the opclass' equality operator.
 * ROWCOMPARE_NE is returned as the strategy number for this case.
 */
void
get_op_btree_interpretation(Oid opno, List **opclasses, List **opstrats)
{
	Oid			lefttype,
				righttype;
	CatCList   *catlist;
	bool		op_negated;
	int			i;

	*opclasses = NIL;
	*opstrats = NIL;

	/*
	 * Get the nominal left-hand input type of the operator; we will ignore
	 * opclasses that don't have that as the expected input datatype.  This is
	 * a kluge to avoid being confused by binary-compatible opclasses (such as
	 * text_ops and varchar_ops, which share the same operators).
	 */
	op_input_types(opno, &lefttype, &righttype);
	Assert(OidIsValid(lefttype));

	/*
	 * Find all the pg_amop entries containing the operator.
	 */

	catlist = caql_begin_CacheList(
			NULL,
			cql("SELECT * FROM pg_amop "
				" WHERE amopopr = :1 "
				" ORDER BY amopopr, "
				" amopclaid ",
				ObjectIdGetDatum(opno)));

	/*
	 * If we can't find any opclass containing the op, perhaps it is a <>
	 * operator.  See if it has a negator that is in an opclass.
	 */
	op_negated = false;
	if (catlist->n_members == 0)
	{
		Oid			op_negator = get_negator(opno);

		if (OidIsValid(op_negator))
		{
			op_negated = true;
			caql_end_CacheList(catlist);

			catlist = caql_begin_CacheList(
					NULL,
					cql("SELECT * FROM pg_amop "
						" WHERE amopopr = :1 "
						" ORDER BY amopopr, "
						" amopclaid ",
						ObjectIdGetDatum(op_negator)));
		}
	}

	/* Now search the opclasses */
	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	op_tuple = &catlist->members[i]->tuple;
		Form_pg_amop op_form = (Form_pg_amop) GETSTRUCT(op_tuple);
		Oid			opclass_id;
		StrategyNumber op_strategy;

		opclass_id = op_form->amopclaid;

		/* must be btree */
		if (!opclass_is_btree(opclass_id))
			continue;

		/* must match operator input type exactly */
		if (get_opclass_input_type(opclass_id) != lefttype)
			continue;

		/* Get the operator's btree strategy number */
		op_strategy = (StrategyNumber) op_form->amopstrategy;
		Assert(op_strategy >= 1 && op_strategy <= 5);

		if (op_negated)
		{
			/* Only consider negators that are = */
			if (op_strategy != BTEqualStrategyNumber)
				continue;
			op_strategy = ROWCOMPARE_NE;
		}

		*opclasses = lappend_oid(*opclasses, opclass_id);
		*opstrats = lappend_int(*opstrats, op_strategy);
	}

	caql_end_CacheList(catlist);
}


/*				---------- AMPROC CACHES ----------						 */

/*
 * get_opclass_proc
 *		Get the OID of the specified support function
 *		for the specified opclass and subtype.
 *
 * Returns InvalidOid if there is no pg_amproc entry for the given keys.
 */
Oid
get_opclass_proc(Oid opclass, Oid subtype, int16 procnum)
{
	Oid			result;
	int			fetchCount;

	result  = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT amproc FROM pg_amproc "
				" WHERE amopclaid = :1 "
				" AND amprocsubtype = :2 "
				" AND amprocnum = :3 ",
				ObjectIdGetDatum(opclass),
				ObjectIdGetDatum(subtype),
				Int16GetDatum(procnum)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}


/*				---------- ATTRIBUTE CACHES ----------					 */

/*
 * get_attname
 *		Given the relation id and the attribute number,
 *		return the "attname" field from the attribute relation.
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such attribute.
 */
char *
get_attname(Oid relid, AttrNumber attnum)
{
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
					NULL,
					&fetchCount,
					NULL,
					cql("SELECT attname FROM pg_attribute "
						" WHERE attrelid = :1 "
						" AND attnum = :2 ",
						ObjectIdGetDatum(relid),
						Int16GetDatum(attnum)));
	
	if (!fetchCount)
		return NULL;

	return result;
}

/*
 * get_relid_attribute_name
 *
 * Same as above routine get_attname(), except that error
 * is handled by elog() instead of returning NULL.
 */
char *
get_relid_attribute_name(Oid relid, AttrNumber attnum)
{
	char	   *attname;

	attname = get_attname(relid, attnum);
	if (attname == NULL)
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	return attname;
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
	return caql_getattnumber(relid, attname);
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
	Oid			result;
	int			fetchCount;

	result  = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT atttypid FROM pg_attribute "
				" WHERE attrelid = :1 "
				" AND attnum = :2 ",
				ObjectIdGetDatum(relid),
				Int16GetDatum(attnum)));

	if (!fetchCount)
		return InvalidOid;

	return result;

}

/*
 * get_atttypmod
 *
 *		Given the relation id and the attribute number,
 *		return the "atttypmod" field from the attribute relation.
 */
int32
get_atttypmod(Oid relid, AttrNumber attnum)
{
	HeapTuple	tp;
	cqContext  *pcqCtx;
	int32		result = -1;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_attribute "
				" WHERE attrelid = :1 "
				" AND attnum = :2 ",
				ObjectIdGetDatum(relid),
				Int16GetDatum(attnum)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_attribute att_tup = (Form_pg_attribute) GETSTRUCT(tp);

		result = att_tup->atttypmod;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_atttypetypmod
 *
 *		A two-fer: given the relation id and the attribute number,
 *		fetch both type OID and atttypmod in a single cache lookup.
 *
 * Unlike the otherwise-similar get_atttype/get_atttypmod, this routine
 * raises an error if it can't obtain the information.
 */
void
get_atttypetypmod(Oid relid, AttrNumber attnum,
				  Oid *typid, int32 *typmod)
{
	HeapTuple	tp;
	Form_pg_attribute att_tup;
	cqContext  *pcqCtx;

    /* CDB: Get type for sysattr even if relid is no good (e.g. SubqueryScan) */
    if (attnum < 0 &&
        attnum > FirstLowInvalidHeapAttributeNumber)
    {
        att_tup = SystemAttributeDefinition(attnum, true);
	    *typid = att_tup->atttypid;
	    *typmod = att_tup->atttypmod;
        return;
    }

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_attribute "
				" WHERE attrelid = :1 "
				" AND attnum = :2 ",
				ObjectIdGetDatum(relid),
				Int16GetDatum(attnum)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	att_tup = (Form_pg_attribute) GETSTRUCT(tp);

	*typid = att_tup->atttypid;
	*typmod = att_tup->atttypmod;

	caql_endscan(pcqCtx);
}

/*				---------- INDEX CACHE ----------						 */

/*		watch this space...
 */

/*				---------- OPCLASS CACHE ----------						 */

/*
 * opclass_is_btree
 *
 *		Returns TRUE iff the specified opclass is associated with the
 *		btree index access method.
 */
bool
opclass_is_btree(Oid opclass)
{
	bool		result;
	Oid			amoid;
	int			fetchCount;

	amoid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT opcamid FROM pg_opclass "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opclass)));

	if (!fetchCount)
		elog(ERROR, "cache lookup failed for opclass %u", opclass);

	result = (amoid == BTREE_AM_OID);

	return result;
}

/*
 * opclass_is_hash
 *
 *		Returns TRUE iff the specified opclass is associated with the
 *		hash index access method.
 */
bool
opclass_is_hash(Oid opclass)
{
	bool		result;
	Oid			amoid;
	int			fetchCount;

	amoid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT opcamid FROM pg_opclass "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opclass)));

	if (!fetchCount)
		elog(ERROR, "cache lookup failed for opclass %u", opclass);

	result = (amoid == HASH_AM_OID);

	return result;
}

/*
 * opclass_is_default
 *
 *		Returns TRUE iff the specified opclass is the default for its
 *		index access method and input data type.
 */
bool
opclass_is_default(Oid opclass)
{
	HeapTuple	tp;
	Form_pg_opclass cla_tup;
	bool		result;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_opclass "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opclass)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);
	cla_tup = (Form_pg_opclass) GETSTRUCT(tp);

	result = cla_tup->opcdefault;

	caql_endscan(pcqCtx);
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
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT opcintype FROM pg_opclass "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opclass)));
	
	if (!fetchCount)
		elog(ERROR, "cache lookup failed for opclass %u", opclass);

	return result;
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
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprcode FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	if (!fetchCount)
		return (RegProcedure) InvalidOid;

	return (RegProcedure) result;
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
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprname FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	if (!fetchCount)
		return NULL;

	return result;
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
	cqContext  *pcqCtx;

	*lefttype = InvalidOid;
	*righttype = InvalidOid;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		optup = (Form_pg_operator) GETSTRUCT(tp);
		*lefttype = optup->oprleft;
		*righttype = optup->oprright;
	}
	caql_endscan(pcqCtx);
}

/*
 * op_mergejoinable
 *
 *		Returns the left and right sort operators corresponding to a
 *		mergejoinable operator, or false if the operator is not mergejoinable.
 */
bool
op_mergejoinable(Oid opno, Oid *leftOp, Oid *rightOp)
{
	HeapTuple	tp;
	bool		result = false;
	cqContext  *pcqCtx;

	if (!sort_op_can_sort(opno, true))
		return result;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);

		if (optup->oprlsortop &&
			optup->oprrsortop)
		{
			*leftOp = optup->oprlsortop;
			*rightOp = optup->oprrsortop;
			result = true;
		}
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * op_mergejoin_crossops
 *
 *		Returns the cross-type comparison operators (ltype "<" rtype and
 *		ltype ">" rtype) for an operator previously determined to be
 *		mergejoinable.	Optionally, fetches the regproc ids of these
 *		operators, as well as their operator OIDs.
 */
void
op_mergejoin_crossops(Oid opno, Oid *ltop, Oid *gtop,
					  RegProcedure *ltproc, RegProcedure *gtproc)
{
	HeapTuple	tp;
	Form_pg_operator optup;
	cqContext  *pcqCtx;

	/*
	 * Get the declared comparison operators of the operator.
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))	/* shouldn't happen */
		elog(ERROR, "cache lookup failed for operator %u", opno);
	optup = (Form_pg_operator) GETSTRUCT(tp);
	*ltop = optup->oprltcmpop;
	*gtop = optup->oprgtcmpop;

	caql_endscan(pcqCtx);

	/* Check < op provided */
	if (!OidIsValid(*ltop))
		elog(ERROR, "mergejoin operator %u has no matching < operator",
			 opno);
	if (ltproc)
		*ltproc = get_opcode(*ltop);

	/* Check > op provided */
	if (!OidIsValid(*gtop))
		elog(ERROR, "mergejoin operator %u has no matching > operator",
			 opno);
	if (gtproc)
		*gtproc = get_opcode(*gtop);
}

/*
 * op_hashjoinable
 *
 * Returns true if the operator is hashjoinable.
 */
bool
op_hashjoinable(Oid opno)
{
	HeapTuple	tp;
	bool		result = false;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_operator optup = (Form_pg_operator) GETSTRUCT(tp);

		result = optup->oprcanhash;
	}

	caql_endscan(pcqCtx);
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
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprcom FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_negator
 *
 *		Returns the corresponding negator of an operator.
 */
Oid
get_negator(Oid opno)
{
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprnegate FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_oprrest
 *
 *		Returns procedure id for computing selectivity of an operator.
 */
RegProcedure
get_oprrest(Oid opno)
{
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprrest FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	if (!fetchCount)
		return (RegProcedure) InvalidOid;

	return (RegProcedure) result;
}

/*
 * get_oprjoin
 *
 *		Returns procedure id for computing selectivity of a join.
 */
RegProcedure
get_oprjoin(Oid opno)
{
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprjoin FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opno)));

	if (!fetchCount)
		return (RegProcedure) InvalidOid;

	return (RegProcedure) result;
}

/*				---------- TRIGGER CACHE ----------					 */

/*
 * get_trigger_name
 *	  returns the name of the trigger with the given triggerid
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such trigger
 */
char *
get_trigger_name(Oid triggerid)
{
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
					NULL,
					&fetchCount,
					NULL,
					cql("SELECT tgname FROM pg_trigger "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(triggerid)));

	if (!fetchCount)
		return NULL;

	return result;
}

/*
 * get_trigger_relid
 *		Given trigger id, return the trigger's relation oid
 */
Oid
get_trigger_relid(Oid triggerid)
{
	Oid			result;
	int			fetchCount;

	result  = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT tgrelid FROM pg_trigger "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(triggerid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_trigger_funcid
 *		Given trigger id, return the trigger's function oid
 */
Oid
get_trigger_funcid(Oid triggerid)
{
	Oid			result;
	int			fetchCount;

	result  = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT tgfoid FROM pg_trigger "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(triggerid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_trigger_type
 *		Given trigger id, return the trigger's type
 */
int32
get_trigger_type(Oid triggerid)
{
	HeapTuple	tp;
	cqContext  *pcqCtx;
	int32		result = -1;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_trigger "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(triggerid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for trigger %u", triggerid);

	result = ((Form_pg_trigger) GETSTRUCT(tp))->tgtype;

	caql_endscan(pcqCtx);
	return result;
}

/*
 * trigger_enabled
 *		Given trigger id, return the trigger's enabled flag
 */
bool
trigger_enabled(Oid triggerid)
{
	HeapTuple	tp;
	bool		result;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_trigger "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(triggerid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for trigger %u", triggerid);

	result = ((Form_pg_trigger) GETSTRUCT(tp))->tgenabled;

	caql_endscan(pcqCtx);
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
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
					NULL,
					&fetchCount,
					NULL,
					cql("SELECT proname FROM pg_proc "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(funcid)));

	if (!fetchCount)
		return NULL;

	return result;
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
       char       *result = NULL;
        int                     fetchCount;

        result = caql_getcstring_plus(
                                        NULL,
                                        &fetchCount,
                                        NULL,
                                        cql("SELECT typname FROM pg_type "
                                                " WHERE oid = :1 ",
                                                ObjectIdGetDatum(oid)));

        if (!fetchCount)
                return NULL;

        return result;
}
 
/*
 * get_func_namespace
 *		Returns the pg_namespace OID associated with a given procedure.
 *
 */
Oid
get_func_namespace(Oid funcid)
{
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT pronamespace FROM pg_proc "
				 " WHERE oid = :1 ",
				 ObjectIdGetDatum(funcid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_func_rettype
 *		Given procedure id, return the function's result type.
 */
Oid
get_func_rettype(Oid funcid)
{
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT prorettype FROM pg_proc "
				 " WHERE oid = :1 ",
				 ObjectIdGetDatum(funcid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_agg_transtype
 *		Given aggregate id, return the aggregate transition function's result type.
 */
Oid
get_agg_transtype(Oid aggid)
{
        Oid                     result;
        int                     fetchCount;

        result = caql_getoid_plus(
                        NULL,
                        &fetchCount,
                        NULL,
                        cql("SELECT aggtranstype FROM pg_aggregate "
                                 " WHERE aggfnoid = :1 ",
                                 ObjectIdGetDatum(aggid)));

        if (!fetchCount)
                elog(ERROR, "cache lookup failed for aggregate %u", aggid);

        return result;
}

/*
 * is_ordered_agg
 *		Given aggregate id, check if it is an ordered aggregate
 */
bool
is_agg_ordered(Oid aggid)
{
	Relation aggRelation = heap_open(AggregateRelationId, AccessShareLock);
	cqContext	cqc;
	HeapTuple aggTuple = caql_getfirst(
			caql_addrel(cqclr(&cqc), aggRelation),
			cql("SELECT * from pg_aggregate"
				" WHERE aggfnoid = :1",
				ObjectIdGetDatum(aggid)));

	if (!HeapTupleIsValid(aggTuple))
	{
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);
	}

	bool isnull = false;
	bool is_ordered = false;
	is_ordered = DatumGetBool(heap_getattr(aggTuple, Anum_pg_aggregate_aggordered, RelationGetDescr(aggRelation), &isnull));

	heap_freetuple(aggTuple);
	heap_close(aggRelation, AccessShareLock);

	Assert(!isnull);

	return is_ordered;
}

/*
 * has_agg_prelimfunc
 *		Given aggregate id, check if it is has a prelim function
 */
bool has_agg_prelimfunc(Oid aggid)
{
	Relation aggRelation = heap_open(AggregateRelationId, AccessShareLock);
	cqContext	cqc;
	HeapTuple aggTuple = caql_getfirst(
			caql_addrel(cqclr(&cqc), aggRelation),
			cql("SELECT * from pg_aggregate"
				" WHERE aggfnoid = :1",
				ObjectIdGetDatum(aggid)));

	if (!HeapTupleIsValid(aggTuple))
	{
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);
	}

	Form_pg_aggregate aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);
	bool has_prelimfunc = (InvalidOid != aggform->aggprelimfn);
	
	heap_freetuple(aggTuple);
	heap_close(aggRelation, AccessShareLock);

	return has_prelimfunc;
}


/*
 * agg_has_prelim_or_invprelim_func
 *		Given aggregate id, check if it is has a prelim or inverse prelim function
 */
bool agg_has_prelim_or_invprelim_func(Oid aggid)
{
	Relation aggRelation = heap_open(AggregateRelationId, AccessShareLock);
	cqContext	cqc;
	HeapTuple aggTuple = caql_getfirst(
			caql_addrel(cqclr(&cqc), aggRelation),
			cql("SELECT * from pg_aggregate"
				" WHERE aggfnoid = :1",
				ObjectIdGetDatum(aggid)));

	if (!HeapTupleIsValid(aggTuple))
	{
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);
	}

	Form_pg_aggregate aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);
	bool has_prelimfunc = (InvalidOid != aggform->aggprelimfn);
	bool has_invprelimfunc = (InvalidOid != aggform->agginvprelimfn);

	heap_freetuple(aggTuple);
	heap_close(aggRelation, AccessShareLock);

	return has_prelimfunc || has_invprelimfunc;
}


/*
 * get_func_nargs
 *		Given procedure id, return the number of arguments.
 */
int
get_func_nargs(Oid funcid)
{
       HeapTuple       tp;
        int                     result;
        cqContext  *pcqCtx;

        pcqCtx = caql_beginscan(
                        NULL,
                        cql("SELECT * FROM pg_proc "
                                " WHERE oid = :1 ",
                                ObjectIdGetDatum(funcid)));

        tp = caql_getnext(pcqCtx);

        if (!HeapTupleIsValid(tp))
                elog(ERROR, "cache lookup failed for function %u", funcid);

        result = ((Form_pg_proc) GETSTRUCT(tp))->pronargs;

        caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(funcid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	procstruct = (Form_pg_proc) GETSTRUCT(tp);

	result = procstruct->prorettype;
	*nargs = (int) procstruct->pronargs;
	Assert(*nargs == procstruct->proargtypes.dim1);
	*argtypes = (Oid *) palloc(*nargs * sizeof(Oid));
	memcpy(*argtypes, procstruct->proargtypes.values, *nargs * sizeof(Oid));

	caql_endscan(pcqCtx);
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
        HeapTuple       tp;
        int             numargs;
        Oid             *argtypes = NULL;
        char    **argnames = NULL;
        char    *argmodes = NULL;
        List    *l_argtypes = NIL;
        int             i;
        cqContext  *pcqCtx;

        pcqCtx = caql_beginscan(
                        NULL,
                        cql("SELECT * FROM pg_proc "
                                " WHERE oid = :1 ",
                                ObjectIdGetDatum(funcid)));

        tp = caql_getnext(pcqCtx);

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
                caql_endscan(pcqCtx);
                return NULL;
        }

        for (i = 0; i < numargs; i++)
        {
                Oid argtype = argtypes[i];
                char argmode = argmodes[i];

                if (PROARGMODE_INOUT == argmode || PROARGMODE_OUT == argmode || PROARGMODE_TABLE == argmode)
                {
                        l_argtypes = lappend_oid(l_argtypes, argtype);
                }
        }

        pfree_ptr_array(argnames, numargs);
        pfree(argtypes);
        pfree(argmodes);

        caql_endscan(pcqCtx);
        return l_argtypes;
}

/*
 * get_func_arg_types
 *		Given procedure id, return all the function's argument types
 */
List *
get_func_arg_types(Oid funcid)
{
	cqContext *pcqCtx = caql_beginscan(
							NULL,
							cql("SELECT * FROM pg_proc "
								" WHERE oid = :1 ",
								ObjectIdGetDatum(funcid)));

	HeapTuple tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	oidvector args = ((Form_pg_proc) GETSTRUCT(tp))->proargtypes;
	List *result = NIL;
	for (int i = 0; i < args.dim1; i++)
	{
		result = lappend_oid(result, args.values[i]);
	}

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(funcid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->proretset;

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(funcid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->proisstrict;

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(funcid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = ((Form_pg_proc) GETSTRUCT(tp))->provolatile;

	caql_endscan(pcqCtx);
	return result;
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
	cqContext  *pcqCtx;
	bool		isnull;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(funcid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for function %u", funcid);

	result = DatumGetChar(
			caql_getattr(pcqCtx, Anum_pg_proc_prodataaccess, &isnull));
	caql_endscan(pcqCtx);

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
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oid FROM pg_class "
				" WHERE relname = :1 "
				" AND relnamespace = :2 ",
				CStringGetDatum((char *) relname),
				ObjectIdGetDatum(relnamespace)));

	if (!fetchCount)
		return InvalidOid;

	return result;
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
	int			result = InvalidAttrNumber;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);

		result = reltup->relnatts;
	}

	caql_endscan(pcqCtx);
	return result;
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
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
					NULL,
					&fetchCount,
					NULL,
					cql("SELECT relname FROM pg_class "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(relid)));

	if (!fetchCount)
		return NULL;

	return result;
}


/*
 * get_rel_name_partition
 *		Returns the name of a given relation plus its parent name, if it is a partition table.
 *		If it not a partition table, it returns the relation name only.
 *
 *	Returns a palloc'd copy of the string, or NULL if no such relation.
 *	The caller is responsible for releasing the palloc'd memory.
 */
char *
get_rel_name_partition(Oid relid)
{
	char *rel_name = get_rel_name(relid);

	if (rel_name == NULL) return NULL;

	if (rel_is_child_partition(relid))
	{
		char *result;

		Oid parent_oid = rel_partition_get_master(relid);
		Assert(parent_oid != InvalidOid);

		char *parent_name = get_rel_name(parent_oid);
		Assert(parent_name);

		char *partition_name = "";

		StringInfo buffer = makeStringInfo();
		Assert(buffer);

		appendStringInfo(buffer, "\"%s\" (partition%s of relation \"%s\")",
							     rel_name, partition_name, parent_name);

		result = pstrdup(buffer->data);

		pfree(rel_name);
		pfree(parent_name);
		pfree(buffer);

		rel_name = NULL;
		parent_name = NULL;
		buffer = NULL;

		return result;
	}
	return rel_name;
}


/*
 * get_rel_namespace
 *
 *		Returns the pg_namespace OID associated with a given relation.
 */
Oid
get_rel_namespace(Oid relid)
{
	Oid			result = InvalidOid;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT relnamespace FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
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
	Oid			result = InvalidOid;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT reltype FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
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
	cqContext  *pcqCtx;
	char		result = '\0';

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);

		result = reltup->relkind;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_rel_reltuples
 *
 *		Returns the estimated number of tuples of a given relation.
 */
float4
get_rel_reltuples(Oid relid)
{
	HeapTuple	tp;
	cqContext  *pcqCtx;
	float4 result = 0;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);

		result = reltup->reltuples;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_rel_relstorage
 *
 *		Returns the relstorage associated with a given relation.
 */
char
get_rel_relstorage(Oid relid)
{
	HeapTuple	tp;
	cqContext  *pcqCtx;
	char		result = '\0';

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);

		result = reltup->relstorage;
	}

	caql_endscan(pcqCtx);
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
	Oid			result = InvalidOid;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT reltablespace FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
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
	bool		result = false;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		result = typtup->typisdefined;
	}

	caql_endscan(pcqCtx);
	return result;
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
	int16		result = 0;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		result = typtup->typlen;
	}

	caql_endscan(pcqCtx);
	return result;
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
	bool		result = false;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		result = typtup->typbyval;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_typlenbyval
 *
 *		A two-fer: given the type OID, return both typlen and typbyval.
 *
 *		Since both pieces of info are needed to know how to copy a Datum,
 *		many places need both.	Might as well get them with one cache lookup
 *		instead of two.  Also, this routine raises an error instead of
 *		returning a bogus value when given a bad type OID.
 */
void
get_typlenbyval(Oid typid, int16 *typlen, bool *typbyval)
{
	HeapTuple	tp;
	Form_pg_type typtup;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typtup = (Form_pg_type) GETSTRUCT(tp);
	*typlen = typtup->typlen;
	*typbyval = typtup->typbyval;

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typtup = (Form_pg_type) GETSTRUCT(tp);
	*typlen = typtup->typlen;
	*typbyval = typtup->typbyval;
	*typalign = typtup->typalign;

	caql_endscan(pcqCtx);
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
	 * own type OID as parameter.  (As of 8.2, domains must get their own OID
	 * even if their base type is an array.)
	 */
	if (typeStruct->typtype == TYPTYPE_BASE && OidIsValid(typeStruct->typelem))
		return typeStruct->typelem;
	else
		return HeapTupleGetOid(typeTuple);
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
	cqContext  *pcqCtx;

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

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	typeTuple = caql_getnext(pcqCtx);

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

	caql_endscan(pcqCtx);
}

#ifdef NOT_USED
char
get_typalign(Oid typid)
{
	HeapTuple	tp;
	cqContext  *pcqCtx;
	char		result = 'i';

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		result = typtup->typalign;
	}

	caql_endscan(pcqCtx);
	return result;
}
#endif

char
get_typstorage(Oid typid)
{
	HeapTuple	tp;
	cqContext  *pcqCtx;
	char		result = 'p';

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		result = typtup->typstorage;
	}

	caql_endscan(pcqCtx);
	return result;
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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	typeTuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", typid);
	type = (Form_pg_type) GETSTRUCT(typeTuple);

	/*
	 * typdefault and typdefaultbin are potentially null, so don't try to
	 * access 'em as struct fields. Must do it the hard way with
	 * caql_getattr.
	 */
	datum = caql_getattr(pcqCtx,
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
		datum = caql_getattr(pcqCtx,
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
			expr = (Node *) makeConst(typid, -1,
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

	caql_endscan(pcqCtx);

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
		cqContext  *pcqCtx;
		
		pcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_type "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(typid)));

		tup = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tup))
			elog(ERROR, "cache lookup failed for type %u", typid);
		typTup = (Form_pg_type) GETSTRUCT(tup);
		if (typTup->typtype != TYPTYPE_DOMAIN)
		{
			/* Not a domain, so done */
			caql_endscan(pcqCtx);
			break;
		}

		Assert(*typmod == -1);
		typid = typTup->typbasetype;
		*typmod = typTup->typtypmod;

		caql_endscan(pcqCtx);
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
	 * Ooops, we have no idea ... wild guess time.
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
	cqContext  *pcqCtx;
	char		result = '\0';

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		result = typtup->typtype;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * type_is_rowtype
 *
 *		Convenience function to determine whether a type OID represents
 *		a "rowtype" type --- either RECORD or a named composite type.
 */
bool
type_is_rowtype(Oid typid)
{
	return (typid == RECORDOID || get_typtype(typid) == TYPTYPE_COMPOSITE);
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
	Oid			result = InvalidOid;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT typrelid FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	if (!fetchCount)
		return InvalidOid;

	return result;
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
	Oid			result = InvalidOid;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);

		if (typtup->typlen == -1)
			result = typtup->typelem;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_array_type
 *
 *		Given the type OID, get the corresponding array type.
 *		Returns InvalidOid if no array type can be found.
 *
 * NB: this only considers varlena arrays to be true arrays.
 */
Oid
get_array_type(Oid typid)
{
	HeapTuple	tp;
	Oid			result = InvalidOid;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typid)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		char	   *array_typename;
		Oid			namespaceId;
		cqContext  *namcqCtx;

		array_typename = makeArrayTypeName(NameStr(typtup->typname));
		namespaceId = typtup->typnamespace;

		namcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_type "
					" WHERE typname = :1 "
					" AND typnamespace = :2 ",
					CStringGetDatum(array_typename),
					ObjectIdGetDatum(namespaceId)));

		tp = caql_getnext(namcqCtx);

		pfree(array_typename);

		if (HeapTupleIsValid(tp))
		{
			typtup = (Form_pg_type) GETSTRUCT(tp);
			if (typtup->typlen == -1 && typtup->typelem == typid)
				result = HeapTupleGetOid(tp);
		}
		caql_endscan(namcqCtx);
	}

	caql_endscan(pcqCtx);
	return result;
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
		cqContext  *pcqCtx;
		
		pcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_type "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(typid)));

		tup = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tup))
		{
			caql_endscan(pcqCtx);
			break;
		}

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

			caql_endscan(pcqCtx);
			return result;
		}

		typid = typTup->typbasetype;
		caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;
		
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(type)));

	typeTuple = caql_getnext(pcqCtx);

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

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;
		
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(type)));

	typeTuple = caql_getnext(pcqCtx);

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

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;
		
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(type)));

	typeTuple = caql_getnext(pcqCtx);

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

	caql_endscan(pcqCtx);
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
	cqContext  *pcqCtx;
		
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(type)));

	typeTuple = caql_getnext(pcqCtx);

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

	caql_endscan(pcqCtx);
}


/*				---------- STATISTICS CACHE ----------					 */

/*
 * get_attavgwidth
 *
 *	  Given the table and attribute number of a column, get the average
 *	  width of entries in the column.  Return zero if no data available.
 */
int32
get_attavgwidth(Oid relid, AttrNumber attnum)
{
	HeapTuple	tp;
	int32		result = 0;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_statistic "
				" WHERE starelid = :1 "
				" AND staattnum = :2 ",
				ObjectIdGetDatum(relid),
				Int16GetDatum(attnum)));

	tp = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tp))
	{
		int32		stawidth = ((Form_pg_statistic) GETSTRUCT(tp))->stawidth;

		if (stawidth > 0)
			result = stawidth;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_attdistinct
 *
 *	  Given the table and attribute number of a column, get the number of
 *	  distinct values in the column.  Return zero if no data available.
 */
float4
get_attdistinct(Oid relid, AttrNumber attnum)
{
	cqContext *pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_statistic "
				" WHERE starelid = :1 "
				" AND staattnum = :2 ",
				ObjectIdGetDatum(relid),
				Int16GetDatum(attnum)));

	HeapTuple tp = caql_getnext(pcqCtx);

	float4 result = 0;
	if (HeapTupleIsValid(tp))
	{
		result = ((Form_pg_statistic) GETSTRUCT(tp))->stadistinct;
	}

	caql_endscan(pcqCtx);
	return result;
}

/*
 * get_attstatsslot
 *
 *		Extract the contents of a "slot" of a pg_statistic tuple.
 *		Returns TRUE if requested slot type was found, else FALSE.
 *
 * Unlike other routines in this file, this takes a pointer to an
 * already-looked-up tuple in the pg_statistic cache.  We do this since
 * most callers will want to extract more than one value from the cache
 * entry, and we don't want to repeat the cache lookup unnecessarily.
 *
 * statstuple: pg_statistics tuple to be examined.
 * atttype: type OID of attribute (can be InvalidOid if values == NULL).
 * atttypmod: typmod of attribute (can be 0 if values == NULL).
 * reqkind: STAKIND code for desired statistics slot kind.
 * reqop: STAOP value wanted, or InvalidOid if don't care.
 * values, nvalues: if not NULL, the slot's stavalues are extracted.
 * numbers, nnumbers: if not NULL, the slot's stanumbers are extracted.
 *
 * If assigned, values and numbers are set to point to palloc'd arrays.
 * If the attribute type is pass-by-reference, the values referenced by
 * the values array are themselves palloc'd.  The palloc'd stuff can be
 * freed by calling free_attstatsslot.
 */
extern bool get_attstatsslot_desc(TupleDesc tupdesc, HeapTuple statstuple,
				 Oid atttype, int32 atttypmod,
				 int reqkind, Oid reqop,
				 Datum **values, int *nvalues,
				 float4 **numbers, int *nnumbers);

bool 
get_attstatsslot(HeapTuple statstuple,
		 Oid atttype, int32 atttypmod,
				 int reqkind, Oid reqop,
				 Datum **values, int *nvalues,
				 float4 **numbers, int *nnumbers)
{
	return get_attstatsslot_desc(NULL, statstuple,
			atttype, atttypmod, reqkind, reqop,
			values, nvalues, numbers, nnumbers
			);
}

/* get an attribute any way you can! */
static Datum
attstatsslot_getattr(cqContext	*pcqCtx, 
					 TupleDesc	 tupdesc,
					 HeapTuple	 statstuple,
					 AttrNumber	 attnum, bool *isnull)
{
	Datum		val;

	if (pcqCtx)
		val = caql_getattr(pcqCtx, attnum, isnull);
	else
	{
		if(tupdesc)
			val = heap_getattr(statstuple, attnum,
							   tupdesc, isnull);
		else
			val = SysCacheGetAttr(STATRELATT, statstuple,
								  attnum,
								  isnull);
	}
		
	return val;
}

bool
get_attstatsslot_desc(TupleDesc tupdesc, HeapTuple statstuple,
				 Oid atttype, int32 atttypmod,
				 int reqkind, Oid reqop,
				 Datum **values, int *nvalues,
				 float4 **numbers, int *nnumbers)
{
	Form_pg_statistic stats = (Form_pg_statistic) GETSTRUCT(statstuple);
	int			i,
				j;
	Datum		val;
	bool		isnull;
	ArrayType  *statarray;
	int			narrayelem;
	HeapTuple	typeTuple;
	Form_pg_type typeForm;

	for (i = 0; i < STATISTIC_NUM_SLOTS; i++)
	{
		if ((&stats->stakind1)[i] == reqkind &&
			(reqop == InvalidOid || (&stats->staop1)[i] == reqop))
			break;
	}
	if (i >= STATISTIC_NUM_SLOTS)
		return false;			/* not there */

	if (values)
	{
		*values = NULL;
		*nvalues = 0;

		val = attstatsslot_getattr(NULL, 
								   tupdesc, 
								   statstuple, 
								   Anum_pg_statistic_stavalues1 + i, 
								   &isnull);

		if (isnull)
			elog(ERROR, "stavalues is null");
		statarray = DatumGetArrayTypeP(val);

		/**
		 * Could be an empty array.
		 */
		if (ARR_NDIM(statarray) > 0)
		{
			cqContext  *typcqCtx;

			/* Need to get info about the array element type */
			typcqCtx = caql_beginscan(
					NULL,
					cql("SELECT * FROM pg_type "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(atttype)));

			typeTuple = caql_getnext(typcqCtx);

			if (!HeapTupleIsValid(typeTuple))
				elog(ERROR, "cache lookup failed for type %u", atttype);
			typeForm = (Form_pg_type) GETSTRUCT(typeTuple);

			/* Deconstruct array into Datum elements; NULLs not expected */
			deconstruct_array(statarray,
					atttype,
					typeForm->typlen,
					typeForm->typbyval,
					typeForm->typalign,
					values, NULL, nvalues);

			/*
			 * If the element type is pass-by-reference, we now have a bunch of
			 * Datums that are pointers into the syscache value.  Copy them to
			 * avoid problems if syscache decides to drop the entry.
			 */
			if (!typeForm->typbyval)
			{
				for (j = 0; j < *nvalues; j++)
				{
					(*values)[j] = datumCopy((*values)[j],
							typeForm->typbyval,
							typeForm->typlen);
				}
			}

			caql_endscan(typcqCtx);
		}
		/*
		 * Free statarray if it's a detoasted copy.
		 */
		if ((Pointer) statarray != DatumGetPointer(val))
			pfree(statarray);
	}

	if (numbers)
	{
		*numbers = NULL;
		*nnumbers = 0;

		val = attstatsslot_getattr(NULL, 
								   tupdesc, 
								   statstuple, 
								   Anum_pg_statistic_stanumbers1 + i, 
								   &isnull);

		if (isnull)
			elog(ERROR, "stanumbers is null");
		statarray = DatumGetArrayTypeP(val);

		/**
		 * Could be an empty array.
		 */
		if (ARR_NDIM(statarray) > 0)
		{
			/*
			 * We expect the array to be a 1-D float4 array; verify that. We don't
			 * need to use deconstruct_array() since the array data is just going
			 * to look like a C array of float4 values.
			 */
			narrayelem = ARR_DIMS(statarray)[0];
			if (ARR_NDIM(statarray) != 1 || narrayelem <= 0 ||
					ARR_HASNULL(statarray) ||
					ARR_ELEMTYPE(statarray) != FLOAT4OID)
				elog(ERROR, "stanumbers is not a 1-D float4 array");
			*numbers = (float4 *) palloc(narrayelem * sizeof(float4));
			*nnumbers = narrayelem;
			memcpy(*numbers, ARR_DATA_PTR(statarray), narrayelem * sizeof(float4));
		}

		/*
		 * Free statarray if it's a detoasted copy.
		 */
		if ((Pointer) statarray != DatumGetPointer(val))
			pfree(statarray);
	}

	return true;
}

/*
 * free_attstatsslot
 *		Free data allocated by get_attstatsslot
 *
 * atttype need be valid only if values != NULL.
 */
void
free_attstatsslot(Oid atttype,
				  Datum *values, int nvalues,
				  float4 *numbers, int nnumbers)
{
	if (values)
	{
		if (!get_typbyval(atttype))
		{
			int			i;

			for (i = 0; i < nvalues; i++)
				pfree(DatumGetPointer(values[i]));
		}
		pfree(values);
	}
	if (numbers)
		pfree(numbers);
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
		return (
			caql_getfirst(
					NULL,
					cql("SELECT * FROM pg_statistic "
						" WHERE starelid = :1 "
						" AND staattnum = :2 ",
						ObjectIdGetDatum(relid),
						Int16GetDatum(attrnum))));
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
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
					NULL,
					&fetchCount,
					NULL,
					cql("SELECT nspname FROM pg_namespace "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(nspid)));

	if (!fetchCount)
		return NULL;

	return result;
}

/*				---------- PG_AUTHID CACHE ----------					 */

/*
 * get_roleid
 *	  Given a role name, look up the role's OID.
 *	  Returns InvalidOid if no such role.
 */
Oid
get_roleid(const char *rolname)
{
	Oid			result;
	int			fetchCount;

	result = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT * FROM pg_authid "
				" WHERE rolname = :1 ",
				CStringGetDatum((char *) rolname)));

	if (!fetchCount)
		return InvalidOid;

	return result;
}

/*
 * get_roleid_checked
 *	  Given a role name, look up the role's OID.
 *	  ereports if no such role.
 */
Oid
get_roleid_checked(const char *rolname)
{
	Oid			roleid;

	roleid = get_roleid(rolname);
	if (!OidIsValid(roleid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role \"%s\" does not exist", rolname)));
	return roleid;
}


/*
 * relation_oids
 *	  Extract all relation oids from the catalog.
 */
List *
relation_oids()
{
	List			*relationOids = NIL;
	Relation		pgclass = NULL;
	HeapScanDesc 	scan = NULL;
	HeapTuple		tuple = NULL;

	pgclass = heap_open(RelationRelationId, AccessShareLock);

	scan = heap_beginscan(pgclass, SnapshotNow, 0 /* key length */, NULL);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Form_pg_class pgclassEntry = (Form_pg_class) GETSTRUCT(tuple);

		switch (pgclassEntry->relstorage)
		{
			case RELSTORAGE_HEAP:
			case RELSTORAGE_AOCOLS:
			case RELSTORAGE_AOROWS:
			case RELSTORAGE_EXTERNAL:
			{
				Oid relOid = HeapTupleGetOid(tuple);
				relationOids = lappend_oid(relationOids, relOid);
				break;
			}
			default:
				break;
		}
	}

	heap_endscan(scan);
	heap_close(pgclass, AccessShareLock);
	return relationOids;
}

/*
 * operator_oids
 *	  Extract all operator oids from the catalog.
 */
List *
operator_oids()
{
	List			*operatorOids = NIL;
	Relation		operatortable = NULL;
	HeapScanDesc 	scan = NULL;
	HeapTuple		tuple = NULL;

	operatortable = heap_open(OperatorRelationId, AccessShareLock);

	scan = heap_beginscan(operatortable, SnapshotNow, 0 /* key length */, NULL);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Oid opOid = HeapTupleGetOid(tuple);
		operatorOids = lappend_oid(operatorOids, opOid);
	}

	heap_endscan(scan);
	heap_close(operatortable, AccessShareLock);
	return operatorOids;
}

/*
 * function_oids
 *	  Extract all function oids from the catalog.
 */
List *
function_oids()
{
	List			*functionOids = NIL;
	Relation		functiontable = NULL;
	HeapScanDesc 	scan = NULL;
	HeapTuple		tuple = NULL;

	functiontable = heap_open(ProcedureRelationId, AccessShareLock);

	scan = heap_beginscan(functiontable, SnapshotNow, 0 /* key length */, NULL);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Oid funcOid = HeapTupleGetOid(tuple);
		functionOids = lappend_oid(functionOids, funcOid);
	}

	heap_endscan(scan);
	heap_close(functiontable, AccessShareLock);
	return functionOids;
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
	HeapTuple htup = NULL;
	
	// lookup pg_proc for functions with the given name and arg type
	cqContext *pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE proname = :1",
				CStringGetDatum((char *) aggname)));

	Oid oidResult = InvalidOid;
	while (HeapTupleIsValid(htup = caql_getnext(pcqCtx)))
	{
		Oid oidProc = HeapTupleGetOid(htup);
		
		Form_pg_proc proctuple = (Form_pg_proc) GETSTRUCT(htup);

		// skip functions with the wrong number of type of arguments
		if (1 != proctuple->pronargs || oidType != proctuple->proargtypes.values[0])
		{
			continue;
		}

		if (caql_getcount(
					NULL,
					cql("SELECT COUNT(*) FROM pg_aggregate "
						" WHERE aggfnoid = :1 ",
						ObjectIdGetDatum(oidProc))) > 0)
		{
			oidResult = oidProc;
			break;
		}
	}
	
	caql_endscan(pcqCtx);

	return oidResult;
}

/*
 * trigger_exists
 *	  Is there a trigger with the given oid
 */
bool
trigger_exists(Oid oid)
{
	return (caql_getcount(
					NULL,
					cql("SELECT COUNT(*) FROM pg_trigger "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(oid))) > 0);
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
	ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, relid);

	Relation rel = heap_open(ConstraintRelationId, AccessShareLock);
	SysScanDesc scan = systable_beginscan
						(
						rel, 
						ConstraintRelidIndexId, 
						true, 
						SnapshotNow, 
						1, 
						skey
						);
	
	HeapTuple	htup = NULL;

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
	heap_close(rel, AccessShareLock);

	return keys;
}

/*
 * check_constraint_exists
 *	  Is there a check constraint with the given oid
 */
bool
check_constraint_exists(Oid oidCheckconstraint)
{
	return (caql_getcount(
					NULL,
					cql("SELECT COUNT(*) FROM pg_constraint "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(oidCheckconstraint))) > 0);
}

/*
 * get_check_constraint_relid
 *		Given check constraint id, return the check constraint's relation oid
 */
Oid
get_check_constraint_relid(Oid oidCheckconstraint)
{
	Oid	result = InvalidOid;
	int	iFetchCount = 0;

	result  = caql_getoid_plus(
			NULL,
			&iFetchCount,
			NULL,
			cql("SELECT conrelid FROM pg_constraint "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(oidCheckconstraint)));

	if (0 == iFetchCount)
	{
		return InvalidOid;
	}

	return result;
}

/*
 * get_check_constraint_oids
 *	 Extract all check constraint oid for a given relation.
 */
List *
get_check_constraint_oids(Oid oidRel)
{
	List *plConstraints = NIL;
	HeapTuple htup = NULL;
	cqContext *pcqCtx = NULL;

	// lookup constraints for relation from the catalog table
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_constraint "
				" WHERE conrelid = :1 ",
				ObjectIdGetDatum(oidRel)));

	while (HeapTupleIsValid(htup = caql_getnext(pcqCtx)))
	{
		Form_pg_constraint contuple = (Form_pg_constraint) GETSTRUCT(htup);

		// only consider check constraints
		if (CONSTRAINT_CHECK != contuple->contype)
		{
			continue;
		}

		plConstraints = lappend_oid(plConstraints, HeapTupleGetOid(htup));
	}

	caql_endscan(pcqCtx);

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
	char *szResult = NULL;
	int iFetchCount = 0;
	szResult = caql_getcstring_plus
				(
				NULL,
				&iFetchCount,
				NULL,
				cql("SELECT conname FROM pg_constraint "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(oidCheckconstraint)
					)
				);

	if (0 == iFetchCount)
	{
		return NULL;
	}

	return szResult;
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
	char *szResult = NULL;
	int iFetchCount = 0;
	szResult = caql_getcstring_plus
				(
				NULL,
				&iFetchCount,
				NULL,
				cql("SELECT conbin FROM pg_constraint "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(oidCheckconstraint)
					)
				);

	if (0 == iFetchCount)
	{
		return NULL;
	}

	return stringToNode(szResult);
}

/*
 * get_cast_func
 *        finds the cast function between the given source and destination type,
 *        and records its oid and properties in the output parameters.
 *        Returns true if a cast exists, false otherwise.
 */
bool
get_cast_func(Oid oidSrc, Oid oidDest, bool *is_binary_coercible, Oid *oidCastFunc)
{
	if (IsBinaryCoercible(oidSrc, oidDest))
	{
		*is_binary_coercible = true;
		*oidCastFunc = 0;
		return true;
	}
	
	*is_binary_coercible = false;
	
	return find_coercion_pathway(oidDest, oidSrc, COERCION_IMPLICIT, oidCastFunc);
}

/*
 * get_comparison_type
 *      Retrieve comparison type  
 */
CmpType
get_comparison_type
	(
	Oid oidOp,
	Oid oidLeft,
	Oid oidRight
	)
{
	// find the equality comparison between the operands
	
	cqContext *pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_operator "
				" WHERE oprleft = :1 and oprright = :2",
				ObjectIdGetDatum(oidLeft),
				ObjectIdGetDatum(oidRight)));

	Oid	oidEq = InvalidOid;
	
	HeapTuple ht = NULL;
	Form_pg_operator optup = NULL;
	
	while (HeapTupleIsValid(ht = caql_getnext(pcqCtx)))
	{		
		optup = (Form_pg_operator) GETSTRUCT(ht);

		if (optup->oprrest == F_EQSEL)
		{
			oidEq = HeapTupleGetOid(ht);
			break;
		}
	}
	
	if (InvalidOid == oidEq)
	{
		// no equality found between specified types, hence not a special comparison operator
		caql_endscan(pcqCtx);
		return CmptOther;
	}
	
	int cmp_oids[][2] = 
	{
	{oidEq, CmptEq},
	{get_negator(oidEq), CmptNEq},
	{optup->oprltcmpop, CmptLT},
	{optup->oprgtcmpop, CmptGT},
	{get_negator(optup->oprltcmpop), CmptGEq},
	{get_negator(optup->oprgtcmpop), CmptLEq}
	};
	
	for (int ul = 0; ul < 6; ul++)
	{
		int *map_elem = cmp_oids[ul];
		if (map_elem[0] == oidOp)
		{
			caql_endscan(pcqCtx);
			return map_elem[1];
		}
	}

	caql_endscan(pcqCtx);
	return CmptOther;
}

/*
 * get_comparison_operator
 *      Retrieve comparison operator between given types  
 */
Oid
get_comparison_operator
	(
	Oid oidLeft,
	Oid oidRight,
	CmpType cmpt
	)
{
	// find the equality comparison between the operands
	cqContext *pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_operator "
				" WHERE oprleft = :1 and oprright = :2",
				ObjectIdGetDatum(oidLeft),
				ObjectIdGetDatum(oidRight)));

	Oid	oidEq = InvalidOid;
	
	HeapTuple ht = NULL;
	Form_pg_operator optup = NULL;
	
	while (HeapTupleIsValid(ht = caql_getnext(pcqCtx)))
	{		
		optup = (Form_pg_operator) GETSTRUCT(ht);

		if (optup->oprrest == F_EQSEL)
		{
			oidEq = HeapTupleGetOid(ht);
			break;
		}
	}
	
	if (InvalidOid == oidEq)
	{
		// no equality found between specified types, hence not a special comparison operator
		caql_endscan(pcqCtx);
		return CmptOther;
	}
	
	int cmp_oids[][2] = 
	{
	{oidEq, CmptEq},
	{get_negator(oidEq), CmptNEq},
	{optup->oprltcmpop, CmptLT},
	{optup->oprgtcmpop, CmptGT},
	{get_negator(optup->oprltcmpop), CmptGEq},
	{get_negator(optup->oprgtcmpop), CmptLEq}
	};
	
	for (int ul = 0; ul < 6; ul++)
	{
		int *map_elem = cmp_oids[ul];
		if (map_elem[1] == cmpt)
		{
			caql_endscan(pcqCtx);
			return map_elem[0];
		}
	}

	caql_endscan(pcqCtx);
	
	return InvalidOid;
}

/*
 * has_subclass_fast
 *
 * In the current implementation, has_subclass returns whether a
 * particular class *might* have a subclass. It will not return the
 * correct result if a class had a subclass which was later dropped.
 * This is because relhassubclass in pg_class is not updated when a
 * subclass is dropped, primarily because of concurrency concerns.
 *
 * Currently has_subclass is only used as an efficiency hack to skip
 * unnecessary inheritance searches, so this is OK.
 */
bool
has_subclass_fast(Oid relationId)
{
	HeapTuple	tuple;
	bool		result;
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relationId)));
	
	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relationId);

	result = ((Form_pg_class) GETSTRUCT(tuple))->relhassubclass;

	caql_endscan(pcqCtx);
	return result;
}

/*
 * has_subclass
 *
 * Performs the exhaustive check whether a relation has a subclass. This is 
 * different from has_subclass_fast, in that the latter can return true if a relation.
 * *might* have a subclass. See comments in has_subclass_fast for more details.
 * 
 */
bool
has_subclass(Oid relationId)
{
	if (!has_subclass_fast(relationId))
	{
		return false;
	}
	
	return (0 < caql_getcount(
					NULL,
					cql("SELECT COUNT(*) FROM pg_inherits "
						" WHERE inhparent = :1 ",
						ObjectIdGetDatum(relationId))));
}

/*
 * get_operator_opclasses
 *		Get the oid of classes the given operator belongs to
 */
List *
get_operator_opclasses(Oid opno)
{
	cqContext *pcqCtx = caql_beginscan(
							NULL,
							cql("SELECT * FROM pg_amop "
								" WHERE amopopr = :1 ",
								ObjectIdGetDatum(opno)));

	HeapTuple htup = NULL;
	List *opclass_oids = NIL;
	while (HeapTupleIsValid(htup = caql_getnext(pcqCtx)))
	{		
		Form_pg_amop amop_tuple = (Form_pg_amop) GETSTRUCT(htup);

		opclass_oids = lappend_oid(opclass_oids, amop_tuple->amopclaid);
	}
	
	caql_endscan(pcqCtx);
	
	return opclass_oids;
} 

/*
 * get_index_opclasses
 *		Get the oid of classes for the index keys
 */
List *
get_index_opclasses(Oid oidIndex)
{
	cqContext *pcqCtx = caql_beginscan(
							NULL,
							cql("SELECT * FROM pg_index "
								" WHERE indexrelid = :1 ",
								ObjectIdGetDatum(oidIndex)));

	HeapTuple htup = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(htup))
	{
		elog(ERROR, "Index %u not found", oidIndex);
	}
	
	List *opclass_oids = NIL;
    /*
     * use caql_getattr() to retrieve number of index attributes, and the oid vector of indclass
     */
    bool isnull = false;
    int indnatts = DatumGetInt16(caql_getattr(pcqCtx, Anum_pg_index_indnatts, &isnull));
	Assert(!isnull);

    Datum indclassDatum = caql_getattr(pcqCtx, Anum_pg_index_indclass, &isnull);
    if (isnull)
    {
            return opclass_oids;
    }
    oidvector *indclass = (oidvector *) DatumGetPointer(indclassDatum);


	for (int i = 0; i < indnatts; i++)
	{
		Oid oidOpClass = indclass->values[i];
		opclass_oids = lappend_oid(opclass_oids, oidOpClass);
	}
	
	caql_endscan(pcqCtx);
	return opclass_oids;
}

/*
 *  has_parquet_children
 *  Check if relation has a Parquet child relation
 */
bool
has_parquet_children(Oid relationId)
{
	Assert(InvalidOid != relationId);
	List *child_oids = find_all_inheritors(relationId);
	ListCell *lc = NULL;
	
	foreach (lc, child_oids)
	{
		Oid oidChild = lfirst_oid(lc);
		Relation rel = RelationIdGetRelation(oidChild);
		Assert(NULL != rel);
		if (RELSTORAGE_PARQUET == rel->rd_rel->relstorage)
		{
			list_free(child_oids);
			RelationClose(rel);
			return true;
		}

		RelationClose(rel);
	}
	list_free(child_oids);
	return false;
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

	if (POLICYTYPE_PARTITIONED == rootPolicy->ptype && 0 == rootPolicy->nattrs)
	{
		/* root partition policy already marked as Random, no mismatch possible as
		 * all children must be random as well */
		return false;
	}

	List *child_oids = find_all_inheritors(rel->rd_id);
	ListCell *lc = NULL;

	foreach (lc, child_oids)
	{
		Oid oidChild = lfirst_oid(lc);
		Relation relChild = RelationIdGetRelation(oidChild);
		Assert(NULL != relChild);

		GpPolicy *childPolicy = relChild->rd_cdbpolicy;
		if (POLICYTYPE_PARTITIONED == childPolicy->ptype && 0 == childPolicy->nattrs)
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

/*
 *  child_triggers
 *  Return true if the table is partitioned and any of the child partitions
 *  have a trigger of the given type.
 */
bool
child_triggers(Oid relationId, int32 triggerType)
{
	Assert(InvalidOid != relationId);
	if (PART_STATUS_NONE == rel_part_status(relationId))
	{
		/* not a partitioned table */
		return false;
	}

	List *childOids = find_all_inheritors(relationId);
	ListCell *lc = NULL;

	bool found = false;
	foreach (lc, childOids)
	{
		Oid oidChild = lfirst_oid(lc);
		Relation relChild = RelationIdGetRelation(oidChild);
		Assert(NULL != relChild);

		if (0 < relChild->rd_rel->reltriggers && NULL == relChild->trigdesc)
		{
			RelationBuildTriggers(relChild);
			if (NULL == relChild->trigdesc)
			{
				relChild->rd_rel->reltriggers = 0;
			}
		}

		for (int i = 0; i < relChild->rd_rel->reltriggers && !found; i++)
		{
			Trigger trigger = relChild->trigdesc->triggers[i];
			found = trigger_enabled(trigger.tgoid) &&
					(get_trigger_type(trigger.tgoid) & triggerType) == triggerType;
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
}
