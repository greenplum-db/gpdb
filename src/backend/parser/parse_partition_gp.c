/*-------------------------------------------------------------------------
 *
 * parse_partition_gp.c
 *	  Expand GPDB legacy partition syntax to PostgreSQL commands.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/parser/parse_partition_gp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/table.h"
#include "catalog/partition.h"
#include "catalog/pg_collation.h"
#include "catalog/gp_partition_template.h"
#include "cdb/cdbvars.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "executor/execPartition.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_utilcmd.h"
#include "partitioning/partdesc.h"
#include "partitioning/partbounds.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/timestamp.h"

typedef struct
{
	PartitionKeyData *partkey;
	Datum		endVal;
	bool		isEndValMaxValue;

	ExprState   *plusexprstate;
	ParamListInfo plusexpr_params;
	EState	   *estate;

	Datum		currStart;
	Datum		currEnd;
	bool		called;
	bool		endReached;

	/* for context in error messages */
	ParseState *pstate;
	int			end_location;
	int			every_location;
} PartEveryIterator;

static List *generateRangePartitions(ParseState *pstate,
									 Relation parentrel,
									 GpPartDefElem *elem,
									 PartitionSpec *subPart,
									 partname_comp *partnamecomp,
									 bool *hasImplicitRangeBounds);
static List *generateListPartition(ParseState *pstate,
								   Relation parentrel,
								   GpPartDefElem *elem,
								   PartitionSpec *subPart,
								   partname_comp *partnamecomp);
static List *generateDefaultPartition(ParseState *pstate,
									  Relation parentrel,
									  GpPartDefElem *elem,
									  PartitionSpec *subPart,
									  partname_comp *partnamecomp);

static char *extract_tablename_from_options(List **options);

/*
 * qsort_stmt_cmp
 *
 * Used when sorting CreateStmts across all partitions.
 */
static int32
qsort_stmt_cmp(const void *a, const void *b, void *arg)
{
	int32		cmpval = 0;
	CreateStmt	   *b1cstmt = *(CreateStmt **) a;
	CreateStmt	   *b2cstmt = *(CreateStmt **) b;
	PartitionKey partKey = (PartitionKey) arg;
	PartitionBoundSpec *b1 = b1cstmt->partbound;
	PartitionBoundSpec *b2 = b2cstmt->partbound;
	int partnatts = partKey->partnatts;
	FmgrInfo *partsupfunc = partKey->partsupfunc;
	Oid *partcollation = partKey->partcollation;
	int			i;
	List	   *b1lowerdatums = b1->lowerdatums;
	List	   *b2lowerdatums = b2->lowerdatums;
	List	   *b1upperdatums = b1->upperdatums;
	List	   *b2upperdatums = b2->upperdatums;

	Assert(IsA(b1cstmt, CreateStmt));
	Assert(IsA(b2cstmt, CreateStmt));

	/* Sort DEFAULT partitions last */
	if (b1->is_default != b2->is_default)
	{
		if (b2->is_default)
			return 1;
		else
			return -1;
	}
	else if (b1lowerdatums != NULL && b2lowerdatums != NULL)
	{
		for (i = 0; i < partnatts; i++)
		{
			ListCell *lc;
			Const *n;
			Datum b1lowerdatum;
			Datum b2lowerdatum;

			lc = list_nth_cell(b1lowerdatums, i);
			n = lfirst(lc);
			b1lowerdatum = n->constvalue;

			lc = list_nth_cell(b2lowerdatums, i);
			n = lfirst(lc);
			b2lowerdatum = n->constvalue;

			cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[i],
													 partcollation[i],
													 b1lowerdatum,
													 b2lowerdatum));
			if (cmpval != 0)
				break;
		}
	}
	else if (b1upperdatums != NULL && b2upperdatums != NULL)
	{
		for (i = 0; i < partnatts; i++)
		{
			ListCell *lc;
			Node *n;
			Datum b1upperdatum;
			Datum b2upperdatum;

			lc = list_nth_cell(b1upperdatums, i);
			n = lfirst(lc);
			if (IsA(n, Const))
				b1upperdatum = ((Const *)n)->constvalue;
			else
			{
				Assert(IsA(n, ColumnRef));
				Assert(list_length(((ColumnRef *)n)->fields)== 1);
				Assert(strcmp(strVal(linitial(((ColumnRef *)n)->fields)), "maxvalue") == 0);
				return 1;
			}

			lc = list_nth_cell(b2upperdatums, i);
			n = lfirst(lc);
			if (IsA(n, Const))
				b2upperdatum = ((Const *)n)->constvalue;
			else
			{
				Assert(IsA(n, ColumnRef));
				Assert(list_length(((ColumnRef *)n)->fields)== 1);
				Assert(strcmp(strVal(linitial(((ColumnRef *)n)->fields)), "maxvalue") == 0);
				return -1;
			}

			cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[i],
													 partcollation[i],
													 b1upperdatum,
													 b2upperdatum));
			if (cmpval != 0)
				break;
		}
	}
	else if (b1lowerdatums != NULL && b2upperdatums != NULL)
	{
		for (i = 0; i < partnatts; i++)
		{
			ListCell *lc;
			Node *n;
			Datum b1lowerdatum;
			Datum b2upperdatum;

			lc = list_nth_cell(b1lowerdatums, i);
			n = lfirst(lc);
			Assert(IsA(n, Const));
			b1lowerdatum = ((Const *)n)->constvalue;

			lc = list_nth_cell(b2upperdatums, i);
			n = lfirst(lc);
			if (IsA(n, Const))
				b2upperdatum = ((Const *)n)->constvalue;
			else
			{
				Assert(IsA(n, ColumnRef));
				Assert(list_length(((ColumnRef *)n)->fields)== 1);
				Assert(strcmp(strVal(linitial(((ColumnRef *)n)->fields)), "maxvalue") == 0);
				return -1;
			}

			cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[i],
													 partcollation[i],
													 b1lowerdatum,
													 b2upperdatum));
			if (cmpval != 0)
				break;
		}

		/*
		 * if after comparison, b1 lower and b2 upper are same, we should get
		 * b2 before b1 so that its start can be adjusted properly. Hence,
		 * return b1 is greater than b2 to flip the order.
		 */
		if (cmpval == 0)
			cmpval = 1;
	}
	else if (b1upperdatums != NULL && b2lowerdatums != NULL)
	{
		for (i = 0; i < partnatts; i++)
		{
			ListCell *lc;
			Node *n;
			Datum b1upperdatum;
			Datum b2lowerdatum;

			lc = list_nth_cell(b1upperdatums, i);
			n = lfirst(lc);
			if (IsA(n, Const))
				b1upperdatum = ((Const *)n)->constvalue;
			else
			{
				Assert(IsA(n, ColumnRef));
				Assert(list_length(((ColumnRef *)n)->fields)== 1);
				Assert(strcmp(strVal(linitial(((ColumnRef *)n)->fields)), "maxvalue") == 0);
				return 1;
			}

			lc = list_nth_cell(b2lowerdatums, i);
			n = lfirst(lc);
			Assert(IsA(n, Const));
			b2lowerdatum = ((Const *)n)->constvalue;

			cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[i],
													 partcollation[i],
													 b1upperdatum,
													 b2lowerdatum));
			if (cmpval != 0)
				break;
		}
	}

	return cmpval;
}

/*
 * Convert an array of partition bound Datums to List of Consts.
 *
 * The array of Datums representation is used e.g. in PartitionBoundInfo,
 * whereas the Const List representation is used e.g. in the raw-parse output
 * of PartitionBoundSpec.
 */
static List *
datums_to_consts(PartitionKey partkey, Datum *datums)
{
	List	   *result = NIL;

	for (int i = 0; i < partkey->partnatts; i++)
	{
		Const	   *c;

		c = makeConst(partkey->parttypid[0],
					  partkey->parttypmod[0],
					  partkey->parttypcoll[0],
					  partkey->parttyplen[0],
					  datumCopy(datums[i],
								partkey->parttypbyval[0],
								partkey->parttyplen[0]),
					  false,
					  partkey->parttypbyval[0]);
		result = lappend(result, c);
	}

	return result;
}

static Datum *
consts_to_datums(PartitionKey partkey, List *consts)
{
	Datum	   *datums;
	ListCell   *lc;
	int			i;

	if (partkey->partnatts != list_length(consts))
		elog(ERROR, "wrong number of partition bounds");

	datums = palloc(partkey->partnatts * sizeof(Datum));

	i = 0;
	foreach(lc, consts)
	{
		Const	   *c = (Const *) lfirst(lc);

		if (!IsA(c, Const))
			elog(ERROR, "expected Const in partition bound");

		datums[i] = c->constvalue;
		i++;
	}

	return datums;
}

/*
 * Sort a list of CreateStmts in-place.
 */
static void
list_qsort_arg(List *list, qsort_arg_comparator cmp, void *arg)
{
	int			len = list_length(list);
	ListCell   *cell;
	CreateStmt **create_stmts;
	int			i;

	/* Empty list is easy */
	if (len == 0)
		return;

	/* Flatten list into an array, so we can use qsort */
	create_stmts = (CreateStmt **) palloc(sizeof(CreateStmt *) * len);
	i = 0;
	foreach(cell, list)
		create_stmts[i++] = (CreateStmt *) lfirst(cell);

	qsort_arg(create_stmts, len, sizeof(CreateStmt *), cmp, arg);

	i = 0;
	foreach(cell, list)
		cell->data.ptr_value = create_stmts[i++];

	pfree(create_stmts);
}

/*
 * Sort the list of GpPartitionBoundSpecs based first on START, if START does
 * not exist, use END. After sort, if any stmt contains an implicit START or
 * END, deduce the value and update the corresponding list of CreateStmts.
 */
static void
deduceImplicitRangeBounds(ParseState *pstate, Relation parentrel, List *stmts, bool addpartition)
{
	PartitionKey key = RelationGetPartitionKey(parentrel);
	PartitionDesc desc = RelationGetPartitionDesc(parentrel);

	list_qsort_arg(stmts, qsort_stmt_cmp, key);

	/*
	 * This works slightly differently, depending on whether this is a
	 * CREATE TABLE or ALTER TABLE SET SUBPARTITION TEMPLATE command to create
	 * a whole new set of child partitions of a parent table, or an ALTER TABLE
	 * ADD PARTTION to add to an existing set of sibling partitions.
	 */
	if (!addpartition)
	{
		/*
		 * CREATE TABLE or ALTER TABLE SET SUBPARTITION TEMPLATE. We deduce the
		 * missing START/END bounds based on the other partitions defined in
		 * the same command.
		 */
		CreateStmt *prevstmt = NULL;
		ListCell   *lc;

		foreach(lc, stmts)
		{
			Node	   *n = lfirst(lc);
			CreateStmt *stmt;

			Assert(IsA(n, CreateStmt));
			stmt = (CreateStmt *) n;

			if (stmt->partbound->is_default)
				continue;

			if (!stmt->partbound->lowerdatums)
			{
				if (prevstmt)
				{
					if (prevstmt->partbound->upperdatums)
						stmt->partbound->lowerdatums = prevstmt->partbound->upperdatums;
					else
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("cannot derive starting value of partition based upon ending of previous partition"),
								 parser_errposition(pstate, stmt->partbound->location)));
				}
				else
				{
					ColumnRef  *minvalue = makeNode(ColumnRef);
					minvalue->location = -1;
					minvalue->fields = lcons(makeString("minvalue"), NIL);
					stmt->partbound->lowerdatums = list_make1(minvalue);
				}

			}
			if (!stmt->partbound->upperdatums)
			{
				Node *next = lc->next ? lfirst(lc->next) : NULL;
				if (next)
				{
					CreateStmt *nextstmt = (CreateStmt *)next;
					if (nextstmt->partbound->lowerdatums)
						stmt->partbound->upperdatums = nextstmt->partbound->lowerdatums;
					else
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("cannot derive ending value of partition based upon starting of next partition"),
								 parser_errposition(pstate, stmt->partbound->location)));
					}
				}
				else
				{
					ColumnRef  *maxvalue = makeNode(ColumnRef);
					maxvalue->location = -1;
					maxvalue->fields = lcons(makeString("maxvalue"), NIL);
					stmt->partbound->upperdatums = list_make1(maxvalue);
				}
			}
			prevstmt = stmt;
		}
	}
	else
	{
		/*
		 * This is ALTER TABLE ADD PARTITION. We deduce the missing START/END
		 * bound based on the existing partitions. In principle, we should also
		 * take into account any other partitions defined in the same command,
		 * but in practice it is not necessary, because the ALTER TABLE ADD
		 * PARTITION syntax only allows creating one partition in one command.
		 */
		Assert(desc->nparts != 0);
		if (list_length(stmts) != 1)
			elog(ERROR, "cannot add more than one partition to existing partitioned table in one command");
		CreateStmt *stmt = linitial_node(CreateStmt, stmts);

		if (!stmt->partbound->is_default)
		{
			if (!stmt->partbound->lowerdatums && !stmt->partbound->upperdatums)
				elog(ERROR, "must specify partition bounds"); /* not allowed in syntax */

			if (!stmt->partbound->lowerdatums)
			{
				int			existing_bound_offset;
				Datum	   *upperdatums;
				bool		equal;

				upperdatums = consts_to_datums(key, stmt->partbound->upperdatums);

				/*
				 * Find the highest existing partition that's lower than or equal to the new
				 * upper bound.
				 */
				existing_bound_offset = partition_range_datum_bsearch(key->partsupfunc,
																	  key->partcollation,
																	  desc->boundinfo,
																	  key->partnatts,
																	  upperdatums,
																	  &equal);
				/*
				 * If there is an existing partition with a lower bound that's
				 * equal to the given upper bound, there isn't much we can do.
				 * The operation is doomed to fail. We set the lower bound as
				 * MINVALUE, and let the later stages throw the error about
				 * overlapping partitions.
				 */
				if (existing_bound_offset != -1 && !equal &&
					desc->boundinfo->kind[existing_bound_offset][0] == PARTITION_RANGE_DATUM_VALUE)
				{
					/*
					 * The new partition was defined with no START, and there is an existing
					 * partition before the given END.
					 */
					stmt->partbound->lowerdatums = datums_to_consts(key,
																	desc->boundinfo->datums[existing_bound_offset]);
				}
				else
				{
					ColumnRef  *minvalue = makeNode(ColumnRef);
					minvalue->location = -1;
					minvalue->fields = lcons(makeString("minvalue"), NIL);
					stmt->partbound->lowerdatums = list_make1(minvalue);
				}
			}

			if (!stmt->partbound->upperdatums)
			{
				int			existing_bound_offset;
				Datum	   *lowerdatums;
				bool		equal;

				lowerdatums = consts_to_datums(key, stmt->partbound->lowerdatums);

				/*
				 * Find the smallest existing partition that's greater than
				 * the new lower bound.
				 */
				existing_bound_offset = partition_range_datum_bsearch(key->partsupfunc,
																	  key->partcollation,
																	  desc->boundinfo,
																	  key->partnatts,
																	  lowerdatums,
																	  &equal);
				existing_bound_offset++;

				if (existing_bound_offset < desc->boundinfo->ndatums &&
					desc->boundinfo->kind[existing_bound_offset][0] == PARTITION_RANGE_DATUM_VALUE)
				{
					stmt->partbound->upperdatums = datums_to_consts(key,
																	desc->boundinfo->datums[existing_bound_offset]);
				}
				else
				{
					ColumnRef  *maxvalue = makeNode(ColumnRef);
					maxvalue->location = -1;
					maxvalue->fields = lcons(makeString("maxvalue"), NIL);
					stmt->partbound->upperdatums = list_make1(maxvalue);
				}
			}
		}
	}
}

/*
 * Functions for iterating through all the partition bounds based on
 * transformed START/END/EVERY.
 */
static PartEveryIterator *
initPartEveryIterator(ParseState *pstate, PartitionKeyData *partkey,
					  Node *start, Node *end, Node *every, int end_location)
{
	PartEveryIterator *iter;
	Datum		startVal = 0;
	Datum		endVal = 0;
	Expr 		*plusexpr = NULL;
	bool		isEndValMaxValue = false;

	if (start)
	{
		Assert(IsA(start, Const));
		Assert(!((Const *)start)->constisnull);
		startVal = ((Const *)start)->constvalue;
	}

	if (end)
	{
		Assert(IsA(end, Const));
		if (((Const *)end)->constisnull)
			isEndValMaxValue = true;
		endVal = ((Const *)end)->constvalue;
	}

	iter = palloc0(sizeof(PartEveryIterator));
	iter->partkey = partkey;
	iter->endVal = endVal;
	iter->isEndValMaxValue = isEndValMaxValue;

	if (every)
	{
		plusexpr = (Expr *)every;

		iter->estate = CreateExecutorState();

		iter->plusexpr_params = makeParamList(1);
		iter->plusexpr_params->params[0].value = (Datum) 0;
		iter->plusexpr_params->params[0].isnull = true;
		iter->plusexpr_params->params[0].pflags = 0;
		iter->plusexpr_params->params[0].ptype = partkey->parttypid[0];

		iter->estate->es_param_list_info = iter->plusexpr_params;

		iter->plusexprstate = ExecInitExprWithParams((Expr *) plusexpr, iter->plusexpr_params);
	}

	iter->currEnd = startVal;
	iter->currStart = (Datum) 0;
	iter->called = false;
	iter->endReached = false;

	iter->pstate = pstate;
	iter->end_location = end_location;
	iter->every_location = exprLocation(every);

	return iter;
}

static void
freePartEveryIterator(PartEveryIterator *iter)
{
	if (iter->estate)
		FreeExecutorState(iter->estate);
}

/*
 * Return next partition bound in START/END/EVERY specification.
 */
static bool
nextPartBound(PartEveryIterator *iter)
{
	bool		firstcall;

	firstcall = !iter->called;
	iter->called = true;

	if (iter->plusexprstate)
	{
		/*
		 * Call (previous bound) + EVERY
		 */
		Datum		next;
		int32		cmpval;
		bool		isnull;

		/* If the previous partition reached END, we're done */
		if (iter->endReached)
			return false;

		iter->plusexpr_params->params[0].isnull = false;
		iter->plusexpr_params->params[0].value = iter->currEnd;

		next = ExecEvalExprSwitchContext(iter->plusexprstate,
										 GetPerTupleExprContext(iter->estate),
										 &isnull);
		/*
		 * None of the built-in + operators can return NULL, but a user-defined
		 * operator could.
		 */
		if (isnull)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("could not compute next partition boundary with EVERY, plus-operator returned NULL"),
					 parser_errposition(iter->pstate, iter->every_location)));

		iter->currStart = iter->currEnd;

		/* Is the next bound greater than END? */
		cmpval = DatumGetInt32(FunctionCall2Coll(&iter->partkey->partsupfunc[0],
												 iter->partkey->partcollation[0],
												 next,
												 iter->endVal));
		if (cmpval >= 0)
		{
			iter->endReached = true;
			iter->currEnd = iter->endVal;
		}
		else
		{
			/*
			 * Sanity check that the next bound is > previous bound. This prevents us
			 * from getting into an infinite loop if the + operator is not behaving.
			 */
			cmpval = DatumGetInt32(FunctionCall2Coll(&iter->partkey->partsupfunc[0],
													 iter->partkey->partcollation[0],
													 iter->currEnd,
													 next));
			if (cmpval >= 0)
			{
				if (firstcall)
				{
					/*
					 * Second iteration: parameter hasn't increased the
					 * current end from the old end.
					 */
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("EVERY parameter too small"),
							 parser_errposition(iter->pstate, iter->every_location)));
				}
				else
				{
					/*
					 * We got a smaller value but later than we
					 * thought so it must be an overflow.
					 */
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("END parameter not reached before type overflows"),
							 parser_errposition(iter->pstate, iter->end_location)));
				}
			}

			iter->currEnd = next;
		}

		return true;
	}
	else
	{
		/* Without EVERY, create just one partition that covers the whole range */
		if (!firstcall)
			return false;

		iter->called = true;
		iter->currStart = iter->currEnd;
		iter->currEnd = iter->endVal;
		iter->endReached = true;
		return true;
	}
}

char *
ChoosePartitionName(const char *parentname, int level, Oid naemspaceId,
					const char *partname, int partnum)
{
	char partsubstring[NAMEDATALEN];
	char levelstr[NAMEDATALEN];

	snprintf(levelstr, NAMEDATALEN, "%d", level);

	if (partname)
	{
		snprintf(partsubstring, NAMEDATALEN, "prt_%s", partname);
		return makeObjectName(parentname, levelstr, partsubstring);
	}

	Assert(partnum > 0);
	snprintf(partsubstring, NAMEDATALEN, "prt_%d", partnum);
	return ChooseRelationName(parentname, levelstr, partsubstring, naemspaceId,
							  false);
}

CreateStmt *
makePartitionCreateStmt(Relation parentrel, char *partname, PartitionBoundSpec *boundspec,
						PartitionSpec *subPart, GpPartDefElem *elem,
						partname_comp *partnamecomp)
{
	CreateStmt *childstmt;
	RangeVar   *parentrv;
	RangeVar   *childrv;
	char	   *schemaname;
	const char *final_part_name;

	if (partnamecomp->tablename)
		final_part_name = partnamecomp->tablename;
	else
		final_part_name = ChoosePartitionName(RelationGetRelationName(parentrel),
											  partnamecomp->level,
											  RelationGetNamespace(parentrel),
											  partname,
											  ++partnamecomp->partnum);

	schemaname = get_namespace_name(parentrel->rd_rel->relnamespace);
	parentrv = makeRangeVar(schemaname, pstrdup(RelationGetRelationName(parentrel)), -1);
	parentrv->relpersistence = parentrel->rd_rel->relpersistence;

	childrv = makeRangeVar(schemaname, (char *)final_part_name, -1);
	childrv->relpersistence = parentrel->rd_rel->relpersistence;

	childstmt = makeNode(CreateStmt);
	childstmt->relation = childrv;
	childstmt->tableElts = NIL;
	childstmt->inhRelations = list_make1(parentrv);
	childstmt->partbound = boundspec;
	childstmt->partspec = subPart;
	childstmt->ofTypename = NULL;
	childstmt->constraints = NIL;
	childstmt->options = elem->options ? copyObject(elem->options) : NIL;
	childstmt->oncommit = ONCOMMIT_NOOP;
	childstmt->tablespacename = elem->tablespacename ? pstrdup(elem->tablespacename) : NULL;
	childstmt->accessMethod = elem->accessMethod ? pstrdup(elem->accessMethod) : NULL;
	childstmt->if_not_exists = false;
	childstmt->gp_style_alter_part = true;
	childstmt->distributedBy = make_distributedby_for_rel(parentrel);
	childstmt->partitionBy = NULL;
	childstmt->relKind = 0;
	childstmt->ownerid = parentrel->rd_rel->relowner;
	childstmt->attr_encodings = copyObject(elem->colencs);

	return childstmt;
}

/* Generate partitions for transformed START (..) END (..) EVERY (..) */
static List *
generateRangePartitions(ParseState *pstate,
						Relation parentrel,
						GpPartDefElem *elem,
						PartitionSpec *subPart,
						partname_comp *partnamecomp,
						bool *hasImplicitRangeBounds)
{
	GpPartitionRangeSpec *boundspec;
	List				 *result = NIL;
	PartitionKey		 partkey;
	PartEveryIterator	 *boundIter;
	Node				 *start = NULL;
	Node				 *end = NULL;
	Node				 *every = NULL;
	int 				 end_location = -1;
	int					 i;

	Assert(elem->boundSpec != NULL);
	/*
	 * We should have checked the same in transformGpPartDefElemWithRangeSpec().
	 * However, we need to check again here in case a GpPartDefElem that comes
	 * from a subpartition template with a boundspec that is no longer up to
	 * date with the first child partition's PartitionKey.
	 */
	if (!IsA(elem->boundSpec, GpPartitionRangeSpec))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("invalid boundary specification for RANGE partition"),
				 parser_errposition(pstate, elem->location)));

	boundspec = (GpPartitionRangeSpec *) elem->boundSpec;
	partkey = RelationGetPartitionKey(parentrel);
	/* Syntax doesn't allow expressions in partition key */
	Assert(partkey->partattrs[0] != 0);

	if (partkey->partnatts != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("too many columns for RANGE partition -- only one column is allowed"),
					errhint("To create multi-column range partitioned table, use PostgreSQL's grammar. For example:\n"
							"create table z (a int, b int, c int) partition by range(b, c);\n"
							"create table z1 partition of z for values from (10, 10) TO (20, 20);")));

	if (boundspec->partStart)
	{
		Assert(boundspec->partStart->edge = PART_EDGE_INCLUSIVE);
		start = linitial(boundspec->partStart->val);
	}
	else
		*hasImplicitRangeBounds = true;
	if (boundspec->partEnd)
	{
		Assert(boundspec->partEnd->edge = PART_EDGE_EXCLUSIVE);
		end = linitial(boundspec->partEnd->val);
		end_location = boundspec->partEnd->location;
	}
	else
		*hasImplicitRangeBounds = true;
	/*
	 * Tablename is used by legacy dump and restore ONLY. If tablename is
	 * specified expectation is to ignore the EVERY clause even if
	 * specified. Ideally, dump should never dump the partition CREATE stmts
	 * with EVERY clause, but somehow old code didn't remove EVERY clause from
	 * dump instead ignored the same during restores. Hence, we need to carry
	 * the same hack in new code.
	 */
	if (partnamecomp->tablename == NULL && boundspec->partEvery)
		every = linitial(boundspec->partEvery);

	boundIter = initPartEveryIterator(pstate, partkey, start, end, every, end_location);

	i = 0;
	while (nextPartBound(boundIter))
	{
		PartitionBoundSpec *boundspec;
		CreateStmt *childstmt;
		char	   *partname;
		char partsubstring[NAMEDATALEN];

		boundspec = makeNode(PartitionBoundSpec);
		boundspec->strategy = PARTITION_STRATEGY_RANGE;
		boundspec->is_default = false;
		if (start)
			boundspec->lowerdatums = datums_to_consts(boundIter->partkey,
													  &boundIter->currStart);
		if (end && boundIter->endReached && boundIter->isEndValMaxValue)
		{
			ColumnRef  *maxvalue = makeNode(ColumnRef);
			maxvalue->fields = list_make1(makeString("maxvalue"));
			boundspec->upperdatums = list_make1(maxvalue);
		}
		else if (end)
			boundspec->upperdatums = datums_to_consts(boundIter->partkey,
													  &boundIter->currEnd);
		boundspec->location = elem->location;

		if (every && elem->partName)
		{
			snprintf(partsubstring, NAMEDATALEN, "%s_%d", elem->partName, ++i);
			partname = &partsubstring[0];
		}
		else
			partname = elem->partName;

		childstmt = makePartitionCreateStmt(parentrel, partname, boundspec,
											copyObject(subPart), elem, partnamecomp);
		result = lappend(result, childstmt);
	}

	freePartEveryIterator(boundIter);

	return result;
}

static List *
generateListPartition(ParseState *pstate,
					  Relation parentrel,
					  GpPartDefElem *elem,
					  PartitionSpec *subPart,
					  partname_comp *partnamecomp)
{
	GpPartitionListSpec *gpvaluesspec;
	PartitionBoundSpec  *boundspec;
	CreateStmt			*childstmt;
	ListCell			*lc;
	List				*listdatums;

	Assert(elem->boundSpec != NULL);
	/*
	 * We should have checked the same in transformGpPartDefElemWithListSpec().
	 * However, we need to check again here in case a GpPartDefElem that comes
	 * from a subpartition template with a boundspec that is no longer up to
	 * date with the first child partition's PartitionKey.
	 */
	if (!IsA(elem->boundSpec, GpPartitionListSpec))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("invalid boundary specification for LIST partition"),
				parser_errposition(pstate, elem->location)));

	gpvaluesspec = (GpPartitionListSpec *) elem->boundSpec;

	boundspec = makeNode(PartitionBoundSpec);
	boundspec->strategy = PARTITION_STRATEGY_LIST;
	boundspec->is_default = false;

	listdatums = NIL;
	foreach (lc, gpvaluesspec->partValues)
	{
		List	   *thisvalue = lfirst(lc);

		if (list_length(thisvalue) != 1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("VALUES specification with more than one column not allowed"),
						parser_errposition(pstate, elem->location)));

		listdatums = lappend(listdatums, linitial(thisvalue));
	}

	boundspec->listdatums = listdatums;
	boundspec->location = -1;

	childstmt = makePartitionCreateStmt(parentrel, elem->partName, boundspec, subPart,
										elem, partnamecomp);

	return list_make1(childstmt);
}

static List *
generateDefaultPartition(ParseState *pstate,
						 Relation parentrel,
						 GpPartDefElem *elem,
						 PartitionSpec *subPart,
						 partname_comp *partnamecomp)
{
	PartitionBoundSpec *boundspec;
	CreateStmt *childstmt;

	boundspec = makeNode(PartitionBoundSpec);
	boundspec->is_default = true;
	boundspec->location = -1;

	/* default partition always needs name to be specified */
	Assert(elem->partName != NULL);
	childstmt = makePartitionCreateStmt(parentrel, elem->partName, boundspec, subPart,
										elem, partnamecomp);
	return list_make1(childstmt);
}

static char *
extract_tablename_from_options(List **options)
{
	ListCell *o_lc;
	ListCell *prev_lc = NULL;
	char *tablename = NULL;

	foreach (o_lc, *options)
	{
		DefElem    *pDef = (DefElem *) lfirst(o_lc);

		/*
		 * get the tablename from the WITH, then remove this element
		 * from the list
		 */
		if (0 == strcmp(pDef->defname, "tablename"))
		{
			/* if the string isn't quoted you get a typename ? */
			if (!IsA(pDef->arg, String))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid tablename specification")));

			char *relname_str = defGetString(pDef);
			*options = list_delete_cell(*options, o_lc, prev_lc);
			tablename = pstrdup(relname_str);
			break;
		}
		prev_lc = o_lc;
	}

	return tablename;
}

static void
split_encoding_clauses(List *encs, List **non_def,
					   ColumnReferenceStorageDirective **def)
{
	ListCell   *lc;

	foreach(lc, encs)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		Assert(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
		{
			if (*def)
				elog(ERROR,
					 "DEFAULT COLUMN ENCODING clause specified more than "
					 "once for partition");
			*def = c;
		}
		else
			*non_def = lappend(*non_def, c);
	}
}

static List *
merge_partition_encoding(ParseState *pstate, List *elem_colencs, List *penc)
{
	List	   *elem_nondefs = NIL;
	List	   *part_nondefs = NIL;
	ColumnReferenceStorageDirective *elem_def = NULL;
	ColumnReferenceStorageDirective *part_def = NULL;
	ListCell   *lc;

	if (penc == NULL)
		return elem_colencs;

	/*
	 * If the specific partition has no specific column encoding, just set it
	 * to the partition level default and we're done.
	 */
	if (elem_colencs == NULL)
		return penc;

	/*
	 * Fixup the actual column encoding clauses for this specific partition
	 * element.
	 *
	 * Rules:
	 *
	 * 1. If an element level clause mentions a specific column, do not
	 * override it.
	 *
	 * 2. Clauses at the partition configuration level which mention a column
	 * not already mentioned at the element level, are applied to the element.
	 *
	 * 3. If an element level default clause exists, we're done.
	 *
	 * 4. If a partition configuration level default clause exists, apply it
	 * to the element level.
	 *
	 * 5. We're done.
	 */

	/* Split specific clauses and default clauses from both our lists */
	split_encoding_clauses(elem_colencs, &elem_nondefs, &elem_def);
	split_encoding_clauses(penc, &part_nondefs, &part_def);

	/* Add clauses from part_nondefs if the columns are not already mentioned */
	foreach(lc, part_nondefs)
	{
		ListCell   *lc2;
		ColumnReferenceStorageDirective *pd = lfirst(lc);
		bool		found = false;

		foreach(lc2, elem_nondefs)
		{
			ColumnReferenceStorageDirective *ed = lfirst(lc2);

			if (strcmp(pd->column, ed->column) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
			elem_colencs = lappend(elem_colencs, pd);
	}

	if (elem_def)
		return elem_colencs;

	if (part_def)
		elem_colencs = lappend(elem_colencs, part_def);

	return elem_colencs;
}

/*
 * Convert an exclusive start (or inclusive end) value from the legacy
 * START..EXCLUSIVE (END..INCLUSIVE) syntax into an inclusive start (exclusive
 * end) value. This is required because the range bounds that we store in
 * the catalog (i.e. PartitionBoundSpec->lower/upperdatums) are always in
 * inclusive start and exclusive end format.
 *
 * We only support this for limited data types. For the supported data
 * types, we perform a '+1' operation on the datum, except for the case when
 * the datum is already the maximum value of the data type, in which case we
 * mark constval->constisnull as true and preserve the original
 * constval->constvalue. The caller is responsible for checking
 * constval->constisnull and if that is true constructing an upperdatum of
 * MAXVALUE (or throwing an error if it's START EXCLUSIVE).
 *
 * If 'is_exclusive_start' is true, this is a START EXCLUSIVE value.
 * Otherwise it is an END INCLUSIVE value. That affects the error messages.
 */
void
convert_exclusive_start_inclusive_end(Const *constval, Oid part_col_typid, int32 part_col_typmod,
									  bool is_exclusive_start)
{
	if (part_col_typmod != -1 &&
		(part_col_typid == TIMEOID ||
		 part_col_typid == TIMETZOID ||
		 part_col_typid == TIMESTAMPOID ||
		 part_col_typid == TIMESTAMPTZOID ||
		 part_col_typid == INTERVALOID))
	{
		if (is_exclusive_start)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("START EXCLUSIVE not supported when partition key has precision specification: %s",
							format_type_with_typemod(part_col_typid, part_col_typmod)),
					 errhint("Specify an inclusive START value and remove the EXCLUSIVE keyword")));
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("END INCLUSIVE not supported when partition key has precision specification: %s",
							format_type_with_typemod(part_col_typid, part_col_typmod)),
					 errhint("Specify an exclusive END value and remove the INCLUSIVE keyword")));
		}
	}

	switch (part_col_typid)
	{
		case INT2OID:
		{
			int16 value = DatumGetInt16(constval->constvalue);
			if (value < PG_INT16_MAX)
				constval->constvalue = Int16GetDatum(value + 1);
			else
				constval->constisnull = true;
		}
		break;
		case INT4OID:
		{
			int32 value = DatumGetInt32(constval->constvalue);
			if (value < PG_INT32_MAX)
				constval->constvalue = Int32GetDatum(value + 1);
			else
				constval->constisnull = true;
		}
		break;
		case INT8OID:
		{
			int64 value = DatumGetInt64(constval->constvalue);
			if (value < PG_INT64_MAX)
				constval->constvalue = Int64GetDatum(value + 1);
			else
				constval->constisnull = true;
		}
		break;
		case DATEOID:
		{
			DateADT value = DatumGetDateADT(constval->constvalue);
			if (!DATE_IS_NOEND(value))
				constval->constvalue = DatumGetDateADT(value + 1);
			else
				constval->constisnull = true;
		}
		break;
		case TIMEOID:
		{
			TimeADT value = DatumGetTimeADT(constval->constvalue);
			struct pg_tm tt,
						*tm = &tt;
			fsec_t		fsec;

			time2tm(value, tm, &fsec);
			if (tm->tm_hour != HOURS_PER_DAY)
				constval->constvalue += 1;
			else
				constval->constisnull = true;
		}
		break;
		case TIMETZOID:
		{
			TimeTzADT *valueptr = DatumGetTimeTzADTP(constval->constvalue);
			struct pg_tm tt,
						 *tm = &tt;
			fsec_t		fsec;
			int			tz;

			timetz2tm(valueptr, tm, &fsec, &tz);
			if (tm->tm_hour != HOURS_PER_DAY)
				valueptr->time += 1;
			else
				constval->constisnull = true;
		}
		break;
		case TIMESTAMPOID:
		{
			Timestamp value = DatumGetTimestamp(constval->constvalue);
			if (!TIMESTAMP_IS_NOEND(value))
				constval->constvalue = TimestampGetDatum(value + 1);
			else
				constval->constisnull = true;
		}
		break;
		case TIMESTAMPTZOID:
		{
			TimestampTz value = DatumGetTimestampTz(constval->constvalue);
			if (!TIMESTAMP_IS_NOEND(value))
				constval->constvalue = TimestampTzGetDatum(value + 1);
			else
				constval->constisnull = true;
		}
		break;
		case INTERVALOID:
		{
			Interval *intervalp = DatumGetIntervalP(constval->constvalue);
			if ((intervalp->month == PG_INT32_MAX &&
				intervalp->day == PG_INT32_MAX &&
				intervalp->time == PG_INT64_MAX))
					constval->constisnull = true;
			else if (intervalp->time < PG_INT64_MAX)
				intervalp->time += 1;
			else if (intervalp->day < PG_INT32_MAX)
				intervalp->day += 1;
			else
				intervalp->month += 1;
		}
		break;
		default:
			if (is_exclusive_start)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("START EXCLUSIVE not supported for partition key data type: %s",
								format_type_be(part_col_typid)),
						 errhint("Specify an inclusive START value and remove the EXCLUSIVE keyword")));
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("END INCLUSIVE not supported for partition key data type: %s",
								format_type_be(part_col_typid)),
						 errhint("Specify an exclusive END value and remove the INCLUSIVE keyword")));
			}
			break;
	}
}

/*
 * Transform the GPDB specific GpPartDefElem with LIST spec (VALUES(..))
 *
 * The input GpPartDefElem is modified in-place. Caller is responsible for
 * passing in the copy of the original GpPartDefElem.
 */
static void
transformGpPartDefElemWithListSpec(ParseState *pstate, Relation parentrel, GpPartDefElem *elem)
{
	GpPartitionListSpec *gpvaluesspec;
	GpPartitionListSpec *new_gpvaluesspec;
	PartitionBoundSpec  *boundspec;
	ListCell			*lc;
	List				*listdatums;

	if (elem->boundSpec == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					errmsg("missing boundary specification in partition \"%s\" of type LIST",
						   elem->partName),
					parser_errposition(pstate, elem->location)));

	if (!IsA(elem->boundSpec, GpPartitionListSpec))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					errmsg("invalid boundary specification for LIST partition"),
					parser_errposition(pstate, elem->location)));

	gpvaluesspec = (GpPartitionListSpec *) elem->boundSpec;
	new_gpvaluesspec = makeNode(GpPartitionListSpec);
	new_gpvaluesspec->location = -1;
	new_gpvaluesspec->partValues = NIL;

	boundspec = makeNode(PartitionBoundSpec);
	boundspec->strategy = PARTITION_STRATEGY_LIST;
	boundspec->is_default = false;
	listdatums = NIL;
	foreach (lc, gpvaluesspec->partValues)
	{
		List	   *thisvalue = lfirst(lc);

		if (list_length(thisvalue) != 1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("VALUES specification with more than one column not allowed"),
						parser_errposition(pstate, elem->location)));

		listdatums = lappend(listdatums, linitial(thisvalue));
	}
	boundspec->listdatums = listdatums;
	boundspec->location = -1;
	boundspec = transformPartitionBound(pstate, parentrel, boundspec);

	foreach (lc, boundspec->listdatums)
	{
		Node *value = lfirst(lc);
		/*
		 * GPDB6 and lower used to support multi-column LIST partitioning, for
		 * backward compatibility we keep the partValues list two-dimensional.
		 */
		new_gpvaluesspec->partValues = lappend(new_gpvaluesspec->partValues,
											   list_make1(value));
	}
	elem->boundSpec = (Node *)new_gpvaluesspec;
}

/*
 * Transform the GPDB specific GpPartDefElem with RANGE spec (START(..) END(..) EVERY(..))
 *
 * The transforms include:
 * - Transform START and END into Consts
 * - Transform EVERY into a plus Expr
 * - Convert EXCLUSIVE START to INCLUSIVE END
 * - Convert INCLUSIVE END to EXCLUSIVE START
 *
 * The input GpPartDefElem is modified in-place. Caller is responsible for
 * passing in the copy of the original GpPartDefElem.
 */
static void
transformGpPartDefElemWithRangeSpec(ParseState *pstate, Relation parentrel, GpPartDefElem *elem)
{
	GpPartitionRangeSpec *boundspec;
	GpPartitionRangeSpec *new_boundspec;
	PartitionKey		 partkey;
	const char			 *partcolname;
	Node				 *start = NULL;
	bool				 startExclusive = false;
	Node				 *end = NULL;
	bool				 endInclusive = false;
	Node				 *every = NULL;
	Oid					 part_col_typid;
	int32				 part_col_typmod;
	Oid					 part_col_collation;

	if (elem->boundSpec == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					errmsg("missing boundary specification in partition \"%s\" of type RANGE",
						   elem->partName),
					parser_errposition(pstate, elem->location)));

	if (!IsA(elem->boundSpec, GpPartitionRangeSpec))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					errmsg("invalid boundary specification for RANGE partition"),
					parser_errposition(pstate, elem->location)));

	boundspec = (GpPartitionRangeSpec *) elem->boundSpec;
	partkey = RelationGetPartitionKey(parentrel);

	/*
	 * GPDB_12_MERGE_FEATURE_NOT_SUPPORTED: We currently disabled support for multi-column
	 * range partitioned tables. If user want to define partition table with multi-column
	 * range, can use PostgreSQL's grammar:
	 *
	 * create table z (a int, b int, c int) partition by range(b, c);
	 * create table z1 partition of z for values from (10, 10) TO (20, 20);
	 */
	if (partkey->partnatts != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					errmsg("too many columns for RANGE partition -- only one column is allowed")));

	/* Syntax doesn't allow expressions in partition key */
	Assert(partkey->partattrs[0] != 0);
	partcolname = NameStr(TupleDescAttr(RelationGetDescr(parentrel),
										partkey->partattrs[0] - 1)->attname);

	/* Avoid scribbling on input */
	new_boundspec = copyObject(boundspec);

	if (boundspec->partStart)
	{
		if (list_length(boundspec->partStart->val) != partkey->partnatts)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("number of START values should cover all partition key columns"),
						parser_errposition(pstate, boundspec->partStart->location)));
		start = linitial(boundspec->partStart->val);
		startExclusive = (boundspec->partStart->edge == PART_EDGE_EXCLUSIVE) ? true : false;
	}

	if (boundspec->partEnd)
	{
		if (list_length(boundspec->partEnd->val) != partkey->partnatts)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("number of END values should cover all partition key columns"),
						parser_errposition(pstate, boundspec->partEnd->location)));
		end = linitial(boundspec->partEnd->val);
		endInclusive = (boundspec->partEnd->edge == PART_EDGE_INCLUSIVE) ? true : false;
	}

	if (boundspec->partEvery)
	{
		if (list_length(boundspec->partEvery) != partkey->partnatts)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("number of EVERY values should cover all partition key columns"),
						parser_errposition(pstate, boundspec->location)));
		every = linitial(boundspec->partEvery);
	}
	else if (boundspec->partEvery)
		new_boundspec->partEvery = NIL;

	part_col_typid = get_partition_col_typid(partkey, 0);
	part_col_typmod = get_partition_col_typmod(partkey, 0);
	part_col_collation = get_partition_col_collation(partkey, 0);

	/* Parse the START/END/EVERY clauses */
	if (start)
	{
		Const	   *startConst;

		startConst = transformPartitionBoundValue(pstate,
												  start,
												  partcolname,
												  part_col_typid,
												  part_col_typmod,
												  part_col_collation);
		if (startConst->constisnull)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("cannot use NULL with range partition specification"),
						parser_errposition(pstate, exprLocation(start))));

		if (startExclusive)
			convert_exclusive_start_inclusive_end(startConst,
												  part_col_typid, part_col_typmod,
												  true);
		if (startConst->constisnull)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("START EXCLUSIVE is out of range"),
						parser_errposition(pstate, exprLocation(start))));

		new_boundspec->partStart->val = list_make1(startConst);
		new_boundspec->partStart->edge = PART_EDGE_INCLUSIVE;
	}

	if (end)
	{
		Const	   *endConst;

		endConst = transformPartitionBoundValue(pstate,
												end,
												partcolname,
												part_col_typid,
												part_col_typmod,
												part_col_collation);
		if (endConst->constisnull)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("cannot use NULL with range partition specification"),
						parser_errposition(pstate, exprLocation(end))));

		if (endInclusive)
			convert_exclusive_start_inclusive_end(endConst,
												  part_col_typid, part_col_typmod,
												  false);

		new_boundspec->partEnd->val = list_make1(endConst);
		new_boundspec->partEnd->location = exprLocation(end);
		new_boundspec->partEnd->edge = PART_EDGE_EXCLUSIVE;
	}

	if (every)
	{
		Node		*plusexpr;
		Param		*param;

		if (start == NULL || end == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("EVERY clause requires START and END"),
						parser_errposition(pstate, exprLocation(every))));
		}

		/*
		 * NOTE: We don't use transformPartitionBoundValue() here. We don't want to cast
		 * the EVERY clause to that type; rather, we'll be passing it to the + operator.
		 * For example, if the partition column is a timestamp, the EVERY clause
		 * can be an interval, so don't try to cast it to timestamp.
		 */

		param = makeNode(Param);
		param->paramkind = PARAM_EXTERN;
		param->paramid = 1;
		param->paramtype = part_col_typid;
		param->paramtypmod = part_col_typmod;
		param->paramcollid = part_col_collation;
		param->location = -1;

		/*
		 * Look up the '+' operator in the current searching path (controlled by search_path parameter).
		 * Just like what we do for the 'BETWEEN ... AND ...' clause.
		 */
		plusexpr = (Node *) make_op(pstate,
									list_make1(makeString("+")),
									(Node *) param,
									(Node *) transformExpr(pstate, every, EXPR_KIND_PARTITION_BOUND),
									pstate->p_last_srf,
									exprLocation(every));

		/*
		 * Check that the input expression's collation is compatible with one
		 * specified for the parent's partition key (partcollation).  Don't throw
		 * an error if it's the default collation which we'll replace with the
		 * parent's collation anyway.
		 */
		if (IsA(plusexpr, CollateExpr))
		{
			Oid		exprCollOid = exprCollation(plusexpr);

			if (OidIsValid(exprCollOid) &&
				exprCollOid != DEFAULT_COLLATION_OID &&
				exprCollOid != part_col_collation)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
							errmsg("collation of partition bound value for column \"%s\" does not match partition key collation \"%s\"",
								   partcolname, get_collation_name(part_col_collation))));
		}

		plusexpr = coerce_to_target_type(pstate,
										 plusexpr, exprType(plusexpr),
										 part_col_typid,
										 part_col_typmod,
										 COERCION_ASSIGNMENT,
										 COERCE_IMPLICIT_CAST,
										 -1);

		if (plusexpr == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
						errmsg("specified value cannot be cast to type %s for column \"%s\"",
							   format_type_be(part_col_typid), partcolname)));

		new_boundspec->partEvery = list_make1(plusexpr);
	}

	elem->boundSpec = (Node *)new_boundspec;
}

/*
 * Transform the GPDB specific partition definition.
 *
 * The transforms include:
 * - Split partDefElems and encClauses
 * - Sort partDefElems so that DEFAULT PARTITION always appears first
 * - Transform the partDefElems for RANGE and LIST strategy
 *
 * Returns a transformed GpPartitionDefinition.
 */
GpPartitionDefinition *
transformGpPartitionDefinition(Oid parentrelid, const char *queryString,
							   GpPartitionDefinition *gpPartDef)
{
	GpPartitionDefinition	*result;
	Relation 				parentrel;
	ListCell				*lc;
	ParseState				*pstate;
	List					*partDefElems = NIL;
	List					*encClauses = NIL;
	bool					defaultPartDefElemFound = false;
	PartitionKey 			partkey;

	result = makeNode(GpPartitionDefinition);

	result->type = gpPartDef->type;
	result->isTemplate = gpPartDef->isTemplate;
	result->fromCatalog = gpPartDef->fromCatalog;
	result->location = gpPartDef->location;

	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	parentrel = table_open(parentrelid, NoLock);
	partkey = RelationGetPartitionKey(parentrel);
	Assert(partkey != NULL);
	if (list_length(partkey->partexprs) > 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
					errmsg("expressions in partition key not supported in legacy GPDB partition syntax")));

	/*
	 * If there is a DEFAULT PARTITION, move it to the front of the list.
	 *
	 * This is to keep the partition naming consistent with historic behavior.
	 * In GPDB 6 and below, the default partition is always numbered 1,
	 * regardless of where in the command it is listed. In other words, it is
	 * always given number 1 in the "partcomp" struct . The default partition
	 * itself always has a name, so the partition number isn't used for it,
	 * but it affects the numbering of all the other partitions.
	 *
	 * The main reason we work so hard to keep the naming the same as in
	 * GPDB 6 is to keep the regression tests that refer to partitions by
	 * name after creating them with the legacy partitioning syntax unchanged.
	 * And conceivably there might be users relying on it on real systems,
	 * too.
	 */

	foreach(lc, gpPartDef->partDefElems)
	{
		Node	*n = lfirst(lc);
		Node	*newnode = copyObject(n);

		if (IsA(newnode, GpPartDefElem))
		{
			GpPartDefElem *elem = (GpPartDefElem *) newnode;

			/*
			 * This was not allowed pre-GPDB7, so keeping the same
			 * restriction. Ideally, we can easily support it now based on how
			 * template is stored. I wish to not open up new cases with legacy
			 * syntax than we supported in past, hence keeping the restriction
			 * in-place.
			 */
			if (gpPartDef->isTemplate && elem->colencs)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("partition specific ENCODING clause not supported in SUBPARTITION TEMPLATE"),
							parser_errposition(pstate, elem->location)));

			if (elem->isDefault)
			{
				if (defaultPartDefElemFound)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("multiple default partitions are not allowed"),
							 parser_errposition(pstate, elem->location)));
				defaultPartDefElemFound = true;
				partDefElems = lcons(elem, partDefElems);
			}
			else
			{
				switch (partkey->strategy)
				{
					case PARTITION_STRATEGY_RANGE:
						transformGpPartDefElemWithRangeSpec(pstate, parentrel, elem);
						break;
					case PARTITION_STRATEGY_LIST:
						transformGpPartDefElemWithListSpec(pstate, parentrel, elem);
						break;
					default:
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("Not supported partition strategy")));
				}
				partDefElems = lappend(partDefElems, elem);
			}
		}
		else
		{
			Assert(IsA(newnode, ColumnReferenceStorageDirective));
			encClauses = lappend(encClauses, newnode);
		}
	}

	result->partDefElems = partDefElems;
	result->encClauses = encClauses;

	free_parsestate(pstate);
	table_close(parentrel, NoLock);
	return result;
}

/*
 * Create a list of CreateStmts, to create partitions based on transformed
 * 'gpPartDef' specification.
 */
List *
generatePartitions(Oid parentrelid, GpPartitionDefinition *gpPartSpec,
				   PartitionSpec *subPartSpec, const char *queryString,
				   List *parentoptions, const char *parentaccessmethod,
				   List *parentattenc, bool addpartition)
{
	Relation	parentrel;
	List	   *result = NIL;
	ParseState *pstate;
	ListCell	*lc;
	List	   *ancestors = get_partition_ancestors(parentrelid);
	partname_comp partcomp = {.tablename=NULL, .level=0, .partnum=0};
	bool isSubTemplate = false;
	List       *penc_cls = NIL;
	List       *parent_tblenc = NIL;
	bool		hasImplicitRangeBounds;

	partcomp.level = list_length(ancestors) + 1;

	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	parentrel = table_open(parentrelid, NoLock);

	/* Remove "tablename" cell from parentOptions, if exists */
	extract_tablename_from_options(&parentoptions);

	/* Check subpartition specs */
	if (subPartSpec && subPartSpec->gpPartDef)
	{
		Assert(subPartSpec->gpPartDef->isTemplate);
		isSubTemplate = subPartSpec->gpPartDef->isTemplate;
	}

	foreach(lc, parentattenc)
	{
		Node       *n = lfirst(lc);
		if (IsA(n, ColumnReferenceStorageDirective))
			parent_tblenc = lappend(parent_tblenc, lfirst(lc));
	}

	foreach(lc, gpPartSpec->encClauses)
	{
		Node	   *n PG_USED_FOR_ASSERTS_ONLY = lfirst(lc);

		Assert(IsA(n, ColumnReferenceStorageDirective));
		penc_cls = lappend(penc_cls, lfirst(lc));
	}

	/*
	 * Merge encoding specified for parent table level and partition
	 * configuration level. (Each partition element level encoding will be
	 * merged later to this). For example:
	 *
	 * create table example (i int, j int, DEFAULT COLUMN ENCODING (compresstype=zlib))
	 * with (appendonly = true, orientation=column) distributed by (i)
	 * partition by range(j)
	 * (partition p1 start(1) end(10), partition p2 start(10) end (20),
	 *  COLUMN j ENCODING (compresstype=rle_type));
	 *
	 * merged result will be column i having zlib and column j having
	 * rle_type.
	 */
	penc_cls = merge_partition_encoding(pstate, penc_cls, parent_tblenc);

	hasImplicitRangeBounds = false;
	foreach(lc, gpPartSpec->partDefElems)
	{
		Node			*n = lfirst(lc);
		GpPartDefElem	*elem;
		List			*new_parts;
		PartitionSpec	*tmpSubPartSpec = NULL;

		Assert(IsA(n, GpPartDefElem));
		/* Avoid scribbling on input */
		elem = copyObject((GpPartDefElem *) n);

		if (subPartSpec)
		{
			tmpSubPartSpec = copyObject(subPartSpec);
			if (isSubTemplate)
			{
				if (elem->subSpec)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								errmsg("subpartition configuration conflicts with subpartition template"),
								parser_errposition(pstate, ((GpPartitionDefinition*)elem->subSpec)->location)));
			}
			else
				tmpSubPartSpec->gpPartDef = (GpPartitionDefinition*) elem->subSpec;

			if (tmpSubPartSpec->gpPartDef == NULL)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							errmsg("no partitions specified at depth %d",
								   partcomp.level + 1),
							parser_errposition(pstate, subPartSpec->location)));
			}
		}
		else if (elem->subSpec)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						errmsg("subpartition specification provided but table doesn't have SUBPARTITION BY clause"),
						parser_errposition(pstate, ((GpPartitionDefinition*)elem->subSpec)->location)));

		/* if WITH has "tablename" then it will be used as name for partition */
		partcomp.tablename = extract_tablename_from_options(&elem->options);

		if (elem->options == NIL)
			elem->options = parentoptions ? copyObject(parentoptions) : NIL;
		if (elem->accessMethod == NULL)
			elem->accessMethod = parentaccessmethod ? pstrdup(parentaccessmethod) : NULL;

		if (elem->accessMethod && strcmp(elem->accessMethod, "ao_column") == 0)
			elem->colencs = merge_partition_encoding(pstate, elem->colencs, penc_cls);

		if (elem->isDefault)
			new_parts = generateDefaultPartition(pstate, parentrel, elem, tmpSubPartSpec, &partcomp);
		else
		{
			PartitionKey key = RelationGetPartitionKey(parentrel);
			Assert(key != NULL);
			switch (key->strategy)
			{
				case PARTITION_STRATEGY_RANGE:
				{
					new_parts = generateRangePartitions(pstate, parentrel,
														elem, tmpSubPartSpec, &partcomp, &hasImplicitRangeBounds);
					break;
				}

				case PARTITION_STRATEGY_LIST:
					new_parts = generateListPartition(pstate, parentrel, elem, tmpSubPartSpec, &partcomp);
					break;
				default:
					elog(ERROR, "Not supported partition strategy");
			}
		}

		result = list_concat(result, new_parts);
	}

	/*
	 * GPDB range partition
	 *
	 * Validate and maybe update range partitions bound here instead of in
	 * check_new_partition_bound(), because we need to modify the lower or upper
	 * bounds for implicit START/END.
	 */
	if (hasImplicitRangeBounds)
		deduceImplicitRangeBounds(pstate, parentrel, result, addpartition);

	free_parsestate(pstate);
	table_close(parentrel, NoLock);
	return result;
}
