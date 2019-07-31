/*-------------------------------------------------------------------------
 *
 * parse_partition.c
 *	  handle partition clauses in parser
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_partition.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_operator.h"
#include "catalog/gp_policy.h"
#include "catalog/namespace.h"
#include "commands/defrem.h"
#include "commands/tablespace.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/walkers.h"
#include "parser/analyze.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_node.h"
#include "parser/parse_oper.h"
#include "parser/parse_partition.h"
#include "parser/parse_utilcmd.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"


typedef struct
{
	ParseState *pstate;
	List	   *cols;
} part_col_cxt;


/* state structures for validating parsed partition specifications */
typedef struct
{
	CreateStmtContext *cxt;
	CreateStmt *stmt;
	PartitionBy *pBy;
	PartitionElem *pElem;
	PartitionElem *prevElem;
	Node	   *spec;
	int			partNumber;
	char	   *namBuf;
	char	   *at_depth;
	bool		prevHadName;
	int			prevStartEnd;
	List	   *allRangeVals;
	List	   *allListVals;
} partValidationState;

typedef struct
{
	ParseState *pstate;
	int			location;
}	range_partition_ctx;

static void make_child_node(CreateStmtContext *cxt, CreateStmt *stmt, char *relname,
				PartitionBy *curPby, Node *newSub,
				Node *pRuleCatalog, Node *pConstraint,
				Node *pStoreAttr, char *prtstr, bool bQuiet,
				List *stenc);
static char *deparse_partition_rule(Node *pNode, ParseState *pstate, Node *parent);
static int partition_range_compare(CreateStmtContext *cxt, CreateStmt *stmt,
						PartitionBy *pBy,
						char *at_depth,
						int partNumber,
						char *compare_op,
						PartitionRangeItem *pRI1,
						PartitionRangeItem *pRI2);
static int partition_range_every(ParseState *pstate,
					  PartitionBy *partitionBy,
					  List *coltypes,
					  char *at_depth,
					  partValidationState *vstate);
static Datum partition_arg_get_val(Node *node, bool *isnull);
static Datum eval_basic_opexpr(ParseState *pstate, List *oprname,
				  Node *leftarg, Node *rightarg,
				  bool *typbyval, int16 *typlen,
				  Oid *restypid,
				  int location);
static void preprocess_range_spec(partValidationState *vstate);
static void validate_range_partition(partValidationState *vstate);
static void validate_list_partition(partValidationState *vstate);
static List *transformPartitionStorageEncodingClauses(List *enc);
static void merge_part_column_encodings(CreateStmt *cs, List *stenc);
static void merge_partition_encoding(ParseState *pstate, PartitionElem *elem, List *penc);
static List *make_partition_rules(CreateStmtContext *cxt, CreateStmt *stmt,
					 Node *partitionBy, PartitionElem *pElem,
					 char *at_depth, char *child_name_str,
					 int partNumId, int maxPartNum,
					 int everyOffset, int maxEveryOffset,
					 ListCell **pp_lc_anp);
static bool range_partition_walker(Node *node, void *context);

/*----------
 * transformPartitionBy() - transform a partitioning clause attached to a CREATE
 * TABLE statement.
 *
 * An example clause:
 *
 * PARTITION BY col1
 *	 SUBPTN  BY col2 SUBTEMPLATE (Spec2),
 *	 SUBPTN  BY col3 SUBTEMPLATE (Spec3)
 * (Spec1)
 *
 * becomes a chain of PartitionBy structs, each with a PartitionSpec:
 *
 *	 pBy -> (col1, spec1, NULL subspec)
 *	  |
 *	  v
 *	 pBy -> (col2, spec2, NULL subspec)
 *	  |
 *	  v
 *	 pBy -> (col3, spec3, NULL subspec)
 *
 * This struct is easy to process recursively.  However, for the syntax:
 *
 * PARTITION BY col1
 *	 SUBPTN  BY col2
 *	 SUBPTN  BY col3
 * (
 * PTN AA (SUB CCC (SUB EE, SUB FF), SUB DDD (SUB EE, SUB FF))
 * PTN BB (SUB CCC (SUB EE, SUB FF), SUB DDD (SUB EE, SUB FF))
 * )
 *
 * the struct is more like:
 *
 *	 pBy -> (col1, spec1, spec2->spec3)
 *	  |
 *	  v
 *	 pBy -> (col2, NULL spec, NULL subspec)
 *	  |
 *	  v
 *	 pBy -> (col3, NULL spec, NULL subspec)
 *
 *
 * We need to move the subpartition specifications to the correct spots.
 *
 *----------
 */
void
transformPartitionBy(CreateStmtContext *cxt,
					 CreateStmt *stmt, Node *partitionBy)
{
	Oid			snamespaceid;
	char	   *snamespace;
	int			partDepth;		/* depth (starting at zero, but display at 1) */
	char		depthstr[NAMEDATALEN];
	char	   *at_depth = "";
	char		at_buf[NAMEDATALEN];
	int			partNumber = -1;
	int			partno = 1;
	int			everyno = 0;
	List	   *partElts = NIL;
	ListCell   *lc = NULL;
	PartitionBy *pBy;			/* the current partitioning clause */
	PartitionBy *psubBy = NULL; /* child of current */
	char		prtstr[NAMEDATALEN];
	Oid			accessMethodId = InvalidOid;
	bool		lookup_opclass;
	Node	   *prevEvery;
	ListCell   *lc_anp = NULL;
	List	   *key_attnums = NIL;
	List	   *key_attnames = NIL;
	List	   *stenc = NIL;

	if (NULL == partitionBy)
		return;

	pBy = (PartitionBy *) partitionBy;
	partDepth = pBy->partDepth;
	partDepth++;				/* increment depth for subpartition */

	if (0 < gp_max_partition_level && partDepth > gp_max_partition_level)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Exceeded the maximum allowed level of partitioning of %d", gp_max_partition_level)));
	}

	snprintf(depthstr, sizeof(depthstr), "%d", partDepth);

	if (pBy->partDepth != 0)
	{
		/*
		 * Only mention the depth 2 and greater to aid in debugging
		 * subpartitions
		 */
		snprintf(at_buf, sizeof(at_buf),
				 " (at depth %d)", partDepth);
		at_depth = at_buf;
	}
	else
		pBy->parentRel = copyObject(stmt->relation);

	/* set the depth for the immediate subpartition */
	if (pBy->subPart)
	{
		psubBy = (PartitionBy *) pBy->subPart;
		psubBy->partDepth = partDepth;
		if (((PartitionBy *) pBy->subPart)->parentRel == NULL)
			((PartitionBy *) pBy->subPart)->parentRel =
				copyObject(pBy->parentRel);
	}

	/*
	 * The recursive nature of this code means that if we're processing a
	 * subpartition rule, the opclass may have been looked up already.
	 */
	lookup_opclass = list_length(pBy->keyopclass) == 0;

	if ((list_length(pBy->keys) > 1) && (pBy->partType == PARTTYP_RANGE))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("too many columns for RANGE partition%s -- "
						"only one column is allowed.",
						at_depth),
				 parser_errposition(cxt->pstate, pBy->location)));
	}

	/* validate keys */
	foreach(lc, pBy->keys)
	{
		Value	   *colval = lfirst(lc);
		char	   *colname = strVal(colval);
		ListCell   *columns;
		bool		found = false;
		Oid			typeid = InvalidOid;
		Oid			opclass = InvalidOid;
		int			i = 0;

		foreach(columns, cxt->columns)
		{
			ColumnDef  *column = (ColumnDef *) lfirst(columns);

			Assert(IsA(column, ColumnDef));

			i++;
			if (strcmp(column->colname, colname) == 0)
			{
				found = true;

				if (list_member_int(key_attnums, i))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("column \"%s\" specified more than once "
									"in partitioning key",
									colname),
							 parser_errposition(cxt->pstate, pBy->location)));


				key_attnums = lappend_int(key_attnums, i);
				Insist(IsA(colval, String));
				key_attnames = lappend(key_attnames, colval);

				if (lookup_opclass)
				{
					typeid = column->typeName->typeOid;

					if (!OidIsValid(typeid))
					{
						int32		typmod;

						typenameTypeIdAndMod(cxt->pstate, column->typeName, &typeid, &typmod);

						column->typeName->typeOid = typeid;
						column->typeName->typemod = typmod;
					}
				}
				break;
			}
		}

		if (!found)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" does not exist in relation \"%s\"",
							colname, cxt->relation->relname),
					 parser_errposition(cxt->pstate, pBy->location)));
		}

		if (lookup_opclass)
		{
			/* get access method ID for this partition type */
			switch (pBy->partType)
			{
				case PARTTYP_RANGE:
				case PARTTYP_LIST:
					accessMethodId = BTREE_AM_OID;
					break;
				default:
					elog(ERROR, "unknown partitioning type %i",
						 pBy->partType);
					break;
			}

			opclass = GetDefaultOpClass(typeid, accessMethodId);

			if (!OidIsValid(opclass))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("data type %s has no default operator class",
								format_type_be(typeid))));
			pBy->keyopclass = lappend_oid(pBy->keyopclass, opclass);
		}
	}

	/* Have partitioning keys; check for violating unique constraints */
	foreach(lc, cxt->ixconstraints)
	{
		ListCell   *ilc = NULL;

		Constraint *ucon = (Constraint *) lfirst(lc);

		Insist(ucon->keys != NIL);

		foreach(ilc, key_attnames)
		{
			Value	   *partkeyname = (Value *) lfirst(ilc);

			if (!list_member(ucon->keys, partkeyname))
			{
				char	   *what = NULL;

				switch (ucon->contype)
				{
					case CONSTR_PRIMARY:
						what = "PRIMARY KEY";
						break;
					case CONSTR_UNIQUE:
						what = "UNIQUE";
						break;
					default:
						elog(ERROR, "unexpected constraint type in internal transformation");
						break;
				}
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					  errmsg("%s constraint must contain all columns in the "
							 "partition key",
							 what),
						 errhint("Include column \"%s\" in the %s constraint or create "
								 "a part-wise UNIQUE index after creating the table instead.",
								 strVal(partkeyname), what)));
			}
		}

	}

	/* No further use for key_attnames, so clean up. */
	if (key_attnames)
	{
		list_free(key_attnames);
	}
	key_attnames = NIL;

	if (pBy->partSpec)
	{
		partNumber = validate_partition_spec(cxt, stmt, pBy, at_depth,
											 partNumber);
		stenc = ((PartitionSpec *) pBy->partSpec)->enc_clauses;
	}

	/*
	 * We'd better have some partitions to look at by now
	 */
	if (partNumber < 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("no partitions specified at depth %d",
						partDepth),
				 parser_errposition(cxt->pstate, pBy->location)));
	}

	/*
	 * Determine namespace and name to use for the child table.
	 */
	snamespaceid = RangeVarGetCreationNamespace(cxt->relation);
	snamespace = get_namespace_name(snamespaceid);

	/* set up the partition specification element list if it exists */
	if (pBy->partSpec)
	{
		PartitionSpec *pSpec = (PartitionSpec *) pBy->partSpec;
		PartitionSpec *subSpec = (PartitionSpec *) pSpec->subSpec;

		/*
		 * If the current specification has a subSpec, then that is the
		 * specification for the child, so we'd better have a SUBPARTITION BY
		 * clause for the child. The sub specification cannot contain a
		 * partition specification of its own.
		 */
		if (subSpec)
		{
			if (NULL == psubBy)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("missing SUBPARTITION BY clause for "
								"subpartition specification%s",
								at_depth),
						 parser_errposition(cxt->pstate, pBy->location)));
			}
			if (psubBy && psubBy->partSpec)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("subpartition specification conflict%s",
								at_depth),
						 parser_errposition(cxt->pstate, psubBy->location)));
			}
			psubBy->partSpec = (Node *) subSpec;
		}
		partElts = pSpec->partElem;
	}

	Assert(partNumber > 0);

	/*
	 * We iterated through the elements in this way to support HASH
	 * partitioning, which likely had no partitioning elements.
	 * FIXME: can we refactor this code now that HASH partitioning
	 * is removed?
	 */
	lc = list_head(partElts);

	prevEvery = NULL;
	/* Iterate through each partition element */
	for (partno = 0; partno < partNumber; partno++)
	{
		PartitionBy *newSub = NULL;
		PartitionElem *pElem = lc ? (PartitionElem *) lfirst(lc) : NULL;

		Node	   *pRuleCatalog = NULL;
		Node	   *pConstraint = NULL;
		Node	   *pStoreAttr = NULL;
		char	   *relname = NULL;
		PartitionBoundSpec *pBSpec = NULL;
		Node	   *every = NULL;
		PartitionBy *curPby = NULL;
		bool		bQuiet = false;
		char	   *pWithTname = NULL;
		List	   *colencs = NIL;

		/*
		 * silence partition name messages if really generating a subpartition
		 * template
		 */
		if (pBy->partQuiet == PART_VERBO_NOPARTNAME)
			bQuiet = true;

		if (pElem)
		{
			pStoreAttr = pElem->storeAttr;
			colencs = pElem->colencs;

			if (pElem->boundSpec && IsA(pElem->boundSpec, PartitionBoundSpec))
			{
				pBSpec = (PartitionBoundSpec *) pElem->boundSpec;
				every = pBSpec->partEvery;
				pWithTname = pBSpec->pWithTnameStr;

				if (prevEvery && every)
				{
					if (equal(prevEvery, every))
						everyno++;
					else
					{
						everyno = 1;	/* reset */
						lc_anp = list_head(pBSpec->everyGenList);
					}
				}
				else if (every)
				{
					everyno++;
					if (!lc_anp)
						lc_anp = list_head(pBSpec->everyGenList);
				}
				else
				{
					everyno = 0;
					lc_anp = NULL;
				}
			}

			if (pElem->partName)
			{
				/*
				 * Use the partition name as part of the child table name
				 */
				snprintf(prtstr, sizeof(prtstr), "prt_%s", pElem->partName);
			}
			else
			{
				/*
				 * For the case where we don't have a partition name, don't
				 * worry about EVERY.
				 */
				if (pElem->rrand)
					/* make a random name for ALTER TABLE ... ADD PARTITION */
					snprintf(prtstr, sizeof(prtstr), "prt_r%lu",
							 pElem->rrand + partno + 1);
				else
					snprintf(prtstr, sizeof(prtstr), "prt_%d", partno + 1);
			}

			if (pElem->subSpec)
			{
				/*
				 * If the current partition element has a subspec, then that
				 * is the spec for the child.  So we'd better have a
				 * SUBPARTITION BY clause for the child, and that clause
				 * cannot contain a SUBPARTITION TEMPLATE (which is a spec).
				 * Note that we might have just updated the subBy partition
				 * spec with the subspec of the current spec, which would be a
				 * conflict.  If the psubBy has a NULL spec, we make a copy of
				 * the node and update it -- the subSpec becomes the
				 * specification for the child.  And if the subspec has a
				 * subspec it gets handled at the top of this loop as the
				 * subspec of the current spec when transformPartitionBy is
				 * re-invoked for the child table.
				 */
				if (NULL == psubBy)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("missing SUBPARTITION BY clause "
									"for subpartition specification%s",
									at_depth),
							 parser_errposition(cxt->pstate, pElem->location)));
				}

				if (psubBy && psubBy->partSpec)
				{
					Assert(((PartitionSpec *) psubBy->partSpec)->istemplate);

					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("subpartition configuration conflicts "
									"with subpartition template"),
							 parser_errposition(cxt->pstate, psubBy->location)));
				}

				if (!((PartitionSpec *) pElem->subSpec)->istemplate && !gp_allow_non_uniform_partitioning_ddl)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("Multi-level partitioned tables without templates are not supported")));
				}

				newSub = makeNode(PartitionBy);

				newSub->partType = psubBy->partType;
				newSub->keys = psubBy->keys;
				newSub->subPart = psubBy->subPart;
				newSub->partSpec = pElem->subSpec;		/* use the subspec */
				newSub->partDepth = psubBy->partDepth;
				newSub->partQuiet = pBy->partQuiet;

				if (pElem->subSpec)		/* get a good location for error msg */
				{
					/* use subspec location */
					newSub->location =
						((PartitionSpec *) pElem->subSpec)->location;
				}
				else
				{
					newSub->location = pElem->location;
				}
			}
		}
		else
			snprintf(prtstr, sizeof(prtstr), "prt_%d", partno + 1);

		/*
		 * MPP-6297: check for WITH (tablename=name) clause [only for
		 * dump/restore, set deep in the guts of partition_range_every...]
		 */
		if (pWithTname)
		{
			relname = pWithTname;
			prtstr[0] = '\0';
		}
		else if (pStoreAttr && ((AlterPartitionCmd *) pStoreAttr)->arg1)
		{
			/*
			 * MPP-6297, MPP-7661, MPP-7514: check for WITH (tablename=name)
			 * clause (AGAIN!) because the pWithTname is only set for an EVERY
			 * clause, and we might not have an EVERY clause.
			 */

			ListCell   *prev_lc = NULL;
			ListCell   *def_lc = NULL;
			List	   *pWithList = (List *)
			(((AlterPartitionCmd *) pStoreAttr)->arg1);

			foreach(def_lc, pWithList)
			{
				DefElem    *pDef = (DefElem *) lfirst(def_lc);

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

					char	   *relname_str = defGetString(pDef);

					relname = pstrdup(relname_str);

					prtstr[0] = '\0';
					pWithList = list_delete_cell(pWithList, def_lc, prev_lc);
					((AlterPartitionCmd *) pStoreAttr)->arg1 =
						(Node *) pWithList;
					break;
				}
				prev_lc = def_lc;
			}					/* end foreach */
		}

		if (strlen(prtstr))
			relname = ChooseRelationName(cxt->relation->relname,
										 depthstr,		/* depth */
										 prtstr,		/* part spec */
										 snamespaceid);

		/* XXX: temporarily add rule creation code for debugging */

		/* now that we have the child table name, make the rule */
		if (!(pElem && pElem->isDefault))
		{
			ListCell   *lc_rule = NULL;
			int			everycount = every ?
			((PartitionRangeItem *) every)->everycount : 0;
			List	   *allRules =
			make_partition_rules(cxt, stmt,
								 partitionBy, pElem, at_depth,
								 relname, partno + 1, partNumber,
								 everyno, everycount,
								 &lc_anp);

			if (allRules)
				lc_rule = list_head(allRules);

			if (lc_rule)
			{
				pConstraint = lfirst(lc_rule);

				if (pConstraint)
				{
					Constraint *pCon = makeNode(Constraint);

					pCon->contype = CONSTR_CHECK;
					pCon->conname = psprintf("%s_check", relname);
					pCon->raw_expr = pConstraint;
					pCon->cooked_expr = NULL;
					pCon->indexspace = NULL;

					pConstraint = (Node *) pCon;
				}
			}
		}

		curPby = makeNode(PartitionBy);

		{
			PartitionSpec *tmppspec = makeNode(PartitionSpec);

			/* selectively copy pBy */
			curPby->partType = pBy->partType;

			curPby->keys = key_attnums;

			curPby->keyopclass = copyObject(pBy->keyopclass);

			if (pElem)
			{
				PartitionSpec *_spec = (PartitionSpec *) pBy->partSpec;

				tmppspec->partElem = list_make1(copyObject(pElem));
				tmppspec->istemplate = _spec->istemplate;
				tmppspec->enc_clauses = copyObject(stenc);
			}

			curPby->partSpec = (Node *) tmppspec;
			curPby->partDepth = pBy->partDepth;
			curPby->partQuiet = pBy->partQuiet;
			curPby->parentRel = copyObject(pBy->parentRel);
		}

		if (!newSub)
			newSub = copyObject(psubBy);

		if (newSub)
			newSub->parentRel = copyObject(pBy->parentRel);

		stmt->is_part_parent = true;
		make_child_node(cxt, stmt, relname, curPby, (Node *) newSub,
						pRuleCatalog, pConstraint, pStoreAttr,
						prtstr, bQuiet, colencs);

		if (pBSpec)
			prevEvery = pBSpec->partEvery;

		if (lc)
			lc = lnext(lc);
	}

	/*
	 * nefarious: we need to keep the "top" partition by statement because
	 * analyze.c:do_parse_analyze needs to find it to re-order the ALTER
	 * statements. (see cdbpartition.c:atpxPart_validate_spec)
	 */
	if ((pBy->partDepth > 0) && (pBy->bKeepMe != true))
	{
		/* we don't need this any more */
		stmt->partitionBy = NULL;
		stmt->is_part_child = true;
	}

}	/* end transformPartitionBy */

/*
 * Add or override any column reference storage directives inherited from the
 * parent table.
 */
static void
merge_part_column_encodings(CreateStmt *cs, List *stenc)
{
	ListCell   *lc;
	List	   *parentencs = NIL;
	List	   *others = NIL;
	List	   *finalencs = NIL;

	/* Don't waste time unnecessarily */
	if (!stenc)
		return;

	/*
	 * First, split the table elements into column reference storage
	 * directives and everything else.
	 */
	foreach(lc, cs->tableElts)
	{
		Node	   *n = lfirst(lc);

		if (IsA(n, ColumnReferenceStorageDirective))
			parentencs = lappend(parentencs, n);
		else
			others = lappend(others, n);
	}

	/*
	 * Now build a final set of storage directives for this partition. Start
	 * with stenc and then add everything from parentencs which doesn't appear
	 * in that. This means we prefer partition level directives over those
	 * directives specified for the parent.
	 *
	 * We must copy stenc, since list_concat may modify it destructively.
	 */
	finalencs = list_copy(stenc);

	foreach(lc, parentencs)
	{
		bool		found = false;
		ListCell   *lc2;
		ColumnReferenceStorageDirective *p = lfirst(lc);

		if (p->deflt)
			continue;

		foreach(lc2, finalencs)
		{
			ColumnReferenceStorageDirective *f = lfirst(lc2);

			if (f->deflt)
				continue;

			if (strcmp(f->column, p->column) == 0)
			{
				found = true;
				break;
			}
		}
		if (!found)
			finalencs = lappend(finalencs, p);
	}

	/*
	 * Finally, make sure we don't propagate any conflicting clauses in the
	 * ColumnDef.
	 */
	foreach(lc, others)
	{
		Node	   *n = lfirst(lc);

		if (IsA(n, ColumnDef))
		{
			ColumnDef  *c = (ColumnDef *) n;
			ListCell   *lc2;

			foreach(lc2, finalencs)
			{
				ColumnReferenceStorageDirective *r = lfirst(lc2);

				if (r->deflt)
					continue;

				if (strcmp(r->column, c->colname) == 0)
				{
					c->encoding = NIL;
					break;
				}
			}

		}
	}

	cs->tableElts = list_concat(others, finalencs);
}

static void
make_child_node(CreateStmtContext *cxt, CreateStmt *stmt, char *relname,
				PartitionBy *curPby, Node *newSub,
				Node *pRuleCatalog, Node *pConstraint,
				Node *pStoreAttr, char *prtstr, bool bQuiet,
				List *stenc)
{
	RangeVar   *parent_tab_name = makeNode(RangeVar);
	RangeVar   *child_tab_name = makeNode(RangeVar);
	CreateStmt *child_tab_stmt = makeNode(CreateStmt);
	AlterTableStmt *ats;
	List	   *childstmts;

	parent_tab_name->catalogname = cxt->relation->catalogname;
	parent_tab_name->schemaname = cxt->relation->schemaname;
	parent_tab_name->relname = cxt->relation->relname;
	parent_tab_name->location = -1;

	child_tab_name->catalogname = cxt->relation->catalogname;
	child_tab_name->schemaname = cxt->relation->schemaname;
	child_tab_name->relpersistence = cxt->relation->relpersistence;
	child_tab_name->relname = relname;
	child_tab_name->location = -1;

	child_tab_stmt->relation = child_tab_name;
	child_tab_stmt->is_part_child = true;
	child_tab_stmt->is_add_part = stmt->is_add_part;

	if (!bQuiet)
		ereport(NOTICE,
				(errmsg("%s will create partition \"%s\" for "
						"table \"%s\"",
						cxt->stmtType, child_tab_name->relname,
						cxt->relation->relname)));

	/*
	 * Deep copy the parent's table elements.
	 *
	 * XXX The copy may be unnecessary, but it is safe.
	 *
	 * Previously, some following code updated constraint names in the
	 * tableElts to assure uniqueness and handle issues with FKs (?).  This
	 * required a copy.
	 *
	 * However, forcing a name change at this level overrides any
	 * user-specified constraint names, so we don't do one here any more.
	 */
	child_tab_stmt->tableElts = copyObject(stmt->tableElts);

	merge_part_column_encodings(child_tab_stmt, stenc);

	if (pConstraint)
		child_tab_stmt->tableElts = lappend(child_tab_stmt->tableElts,
											pConstraint);

	/*
	 * XXX XXX: inheriting the parent causes a headache in
	 * transformDistributedBy, since it assumes the parent exists already.
	 * Just add an "ALTER TABLE...INHERIT parent" after the create child table
	 */
	/* child_tab_stmt->inhRelations = list_make1(parent_tab_name); */
	child_tab_stmt->inhRelations = list_copy(stmt->inhRelations);

	child_tab_stmt->constraints = copyObject(stmt->constraints);
	child_tab_stmt->options = stmt->options;

	/* allow WITH clause for appendonly tables */
	if (pStoreAttr)
	{
		AlterPartitionCmd *psa_apc = (AlterPartitionCmd *) pStoreAttr;

		/* Options */
		if (psa_apc->arg1)
			child_tab_stmt->options = (List *) psa_apc->arg1;
		/* Tablespace from parent (input CreateStmt)... */
		if (psa_apc->arg2 && *strVal(psa_apc->arg2))
			child_tab_stmt->tablespacename = strVal(psa_apc->arg2);
	}
	/* ...or tablespace from root. */
	if (!child_tab_stmt->tablespacename && stmt->tablespacename)
		child_tab_stmt->tablespacename = stmt->tablespacename;

	child_tab_stmt->oncommit = stmt->oncommit;
	child_tab_stmt->distributedBy = stmt->distributedBy;

	/*
	 * use the newSub as the partitionBy if the current partition elem had an
	 * inline subpartition declaration
	 */
	child_tab_stmt->partitionBy = (Node *) newSub;

	child_tab_stmt->relKind = RELKIND_RELATION;

	/*
	 * Adjust tablespace name for the CREATE TABLE via ADD PARTITION.
	 * (MPP-8047)
	 *
	 * The way we traverse the hierarchy, parents are visited before children,
	 * so we can usually pick up tablespace from the parent relation.  If the
	 * child is a top-level branch, though, we take the tablespace from the
	 * root. Ultimately, we take the tablespace as specified in the command,
	 * or, if none was specified, the one from the root partitioned table.
	 */
	if (!child_tab_stmt->tablespacename)
	{
		Oid			poid = RangeVarGetRelid(cxt->relation, NoLock, true);		/* parent branch */

		if (!poid)
		{
			poid = RangeVarGetRelid(stmt->relation, NoLock, true);		/* whole partitioned
																 * table */
		}
		if (poid)
		{
			Relation	prel = RelationIdGetRelation(poid);

			child_tab_stmt->tablespacename = get_tablespace_name(prel->rd_rel->reltablespace);
			RelationClose(prel);
		}
	}

	childstmts = transformCreateStmt(child_tab_stmt, cxt->pstate->p_sourcetext, true);

	/*
	 * Attach the child partition to the parent, by creating an ALTER TABLE
	 * statement. The order of the commands is important: we want the CREATE
	 * TABLE command for the partition to be executed first, then the ALTER
	 * TABLE to make it inherit the parent, and any additional commands to
	 * create subpartitions after that.
	 */
	{
		AlterTableCmd *atc;
		InheritPartitionCmd *ipc;

		ipc = makeNode(InheritPartitionCmd);
		ipc->parent = parent_tab_name;

		/* alter table child inherits parent */
		atc = makeNode(AlterTableCmd);
		atc->subtype = AT_AddInherit;
		atc->def = (Node *) ipc;

		ats = makeNode(AlterTableStmt);
		ats->relation = child_tab_name;
		ats->cmds = list_make1((Node *) atc);
		ats->relkind = OBJECT_TABLE;

		/* this is the deepest we're going, add the partition rules */
		{
			AlterTableCmd *atc2 = makeNode(AlterTableCmd);

			/* alter table add child to partition set */
			atc2->subtype = AT_PartAddInternal;
			atc2->def = (Node *) curPby;
			ats->cmds = lappend(ats->cmds, atc2);
		}
	}

	/* CREATE TABLE command for the partition */
	cxt->alist = lappend(cxt->alist, linitial(childstmts));
	childstmts = list_delete_first(childstmts);
	/* ALTER TABLE goes next */
	cxt->alist = lappend(cxt->alist, ats);
	/* And then any additional commands generated from the CREATE TABLE */
	cxt->alist = list_concat(cxt->alist, childstmts);
}

static List *
make_partition_rules(CreateStmtContext *cxt, CreateStmt *stmt,
					 Node *partitionBy, PartitionElem *pElem,
					 char *at_depth, char *child_name_str,
					 int partNumId, int maxPartNum,
					 int everyOffset, int maxEveryOffset,
					 ListCell **pp_lc_anp
)
{
	PartitionBy *pBy = (PartitionBy *) partitionBy;
	List	   *allRules = NULL;

	Assert(pElem);

	if (pBy->partType == PARTTYP_LIST)
	{
		Node	   *pIndAND = NULL;
		Node	   *pIndOR = NULL;
		List	   *colElts = pBy->keys;
		ListCell   *lc = NULL;
		List	   *valElts = NULL;
		ListCell   *lc_val = NULL;
		ListCell   *lc_valent = NULL;
		char	   *exprStr;
		StringInfoData ANDBuf;
		StringInfoData ORBuf;
		PartitionValuesSpec *spec = (PartitionValuesSpec *) pElem->boundSpec;

		initStringInfo(&ANDBuf);
		initStringInfo(&ORBuf);

		valElts = spec->partValues;
		lc_valent = list_head(valElts);

		/* number of values is a multiple of the number of columns */
		if (lc_valent)
			lc_val = list_head((List *) lfirst(lc_valent));
		for (; lc_val;)			/* for all vals */
		{
			lc = list_head(colElts);
			pIndAND = NULL;

			for (; lc; lc = lnext(lc))	/* for all cols */
			{
				Node	   *pCol = lfirst(lc);
				Node	   *pEq = NULL;
				Value	   *pColConst;
				A_Const    *pValConst;
				ColumnRef  *pCRef;
				bool		isnull = false;

				pCRef = makeNode(ColumnRef);	/* need columnref for WHERE */

				if (NULL == lc_val)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("mismatched columns for VALUES%s",
									at_depth),
							 parser_errposition(cxt->pstate, spec->location)));
				}

				pColConst = (Value *) pCol;
				pValConst = (A_Const *) lfirst(lc_val);

				Assert(IsA(pColConst, String));

				pCRef->location = -1;
				pCRef->fields = list_make1(pColConst);

				if (!(IsA(pValConst, A_Const)))
				{
					Const	   *c = (Const *) pValConst;
					Type		typ;
					Form_pg_type pgtype;
					Datum		dat;
					A_Const    *aconst;
					Value	   *val;

					Assert(IsA(c, Const));

					if (c->constisnull)
					{
						isnull = true;
						aconst = NULL;
					}
					else
					{
						aconst = makeNode(A_Const);
						typ = typeidType(c->consttype);
						pgtype = (Form_pg_type) GETSTRUCT(typ);
						dat = OidFunctionCall1(pgtype->typoutput, c->constvalue);

						ReleaseSysCache(typ);
						val = makeString(DatumGetCString(dat));
						aconst->val = *val;
						aconst->location = -1;
					}

					pValConst = aconst;
				}

				if (isnull)
				{
					NullTest   *nt = makeNode(NullTest);

					nt->arg = (Expr *) pCRef;
					nt->nulltesttype = IS_NULL;
					pEq = (Node *) nt;
				}
				else
					/* equality expression: column = value */
					pEq = (Node *) makeSimpleA_Expr(AEXPR_OP,
													"=",
													(Node *) pCRef,
													(Node *) pValConst,
													-1 /* position */ );

				exprStr = deparse_partition_rule((Node *) pEq, cxt->pstate, (Node *) spec);

				/*
				 * for multiple cols - AND the matches eg: (col = value) AND
				 * (col = value)
				 */
				if (pIndAND)
				{
					char	   *pfoo = pstrdup(ANDBuf.data);

					pIndAND =
						(Node *) makeA_Expr(AEXPR_AND, NIL,
											pIndAND,
											pEq,
											-1 /* position */ );

					resetStringInfo(&ANDBuf);
					appendStringInfo(&ANDBuf, "((%s) and %s)",
									 exprStr, pfoo);

					pfree(pfoo);
				}
				else
				{
					pIndAND = pEq;
					resetStringInfo(&ANDBuf);
					appendStringInfo(&ANDBuf, "(%s)", exprStr);
				}

				lc_val = lnext(lc_val);
			}					/* end for all cols */

			/*----------
			 * if more VALUES than columns, then multiple matching
			 * conditions, so OR them eg:
			 *
			 * ((col = value) AND (col = value)) OR
			 * ((col = value) AND (col = value)) OR
			 * ((col = value) AND (col = value))
			 *----------
			 */
			if (pIndOR)
			{
				char	   *pfoo = pstrdup(ORBuf.data);

				pIndOR =
					(Node *) makeA_Expr(AEXPR_OR, NIL,
										pIndOR,
										pIndAND,
										-1 /* position */ );

				resetStringInfo(&ORBuf);
				appendStringInfo(&ORBuf, "((%s) OR %s)",
								 ANDBuf.data, pfoo);

				pfree(pfoo);

			}
			else
			{
				pIndOR = pIndAND;
				resetStringInfo(&ORBuf);
				appendStringInfo(&ORBuf, "(%s)", ANDBuf.data);
			}
			if (lc_val == NULL)
			{
				lc_valent = lnext(lc_valent);
				if (lc_valent)
					lc_val = list_head((List *) lfirst(lc_valent));
			}

		}						/* end for all vals */

		/*
		 * first the CHECK constraint, then the INSERT statement, then the
		 * RULE statement
		 */
		allRules = list_make1(pIndOR);
	}							/* end if LIST */

	/*
	 * For RANGE partitions with an EVERY clause, the following fields are
	 * defined: int everyOffset - 0 for no EVERY, else 1 to maxEveryOffset int
	 * maxEveryOffset - 1 for no EVERY, else >1 ListCell   **pp_lc_anp - the
	 * pointer to the pointer to the ListCell of [A]ll[N]ew[P]artitions, a
	 * list of lists of stringified values (as opposed to A_Const's).
	 */

	if (pBy->partType == PARTTYP_RANGE)
	{
		Node	   *pIndAND = NULL;
		Node	   *pIndOR = NULL;
		List	   *colElts = pBy->keys;
		ListCell   *lc = NULL;
		List	   *valElts = NULL;
		ListCell   *lc_val = NULL;
		PartitionRangeItem *pRI = NULL;
		char	   *exprStr;
		StringInfoData ANDBuf;
		StringInfoData ORBuf;
		int			range_idx;
		PartitionBoundSpec *pBSpec = NULL;

		ListCell   *lc_every_val = NULL;
		List	   *allNewCols = NIL;

		if (pElem)
		{
			pBSpec = (PartitionBoundSpec *) pElem->boundSpec;
		}

		initStringInfo(&ANDBuf);
		initStringInfo(&ORBuf);

		pRI = (PartitionRangeItem *) (pBSpec->partStart);

		if (pRI)
			valElts = pRI->partRangeVal;
		else
			valElts = NULL;

		if (maxEveryOffset > 1) /* if have an EVERY clause */
		{
			ListCell   *lc_anp = NULL;

			/* check the list of "all new partitions" */
			if (pp_lc_anp)
			{
				lc_anp = *pp_lc_anp;
				if (lc_anp)
				{
					allNewCols = lfirst(lc_anp);

					/* find the list of columns for a new partition */
					if (allNewCols)
					{
						lc_every_val = list_head(allNewCols);
					}
				}
			}
		}						/* end if every */

		/* number of values must equal of the number of columns */

		for (range_idx = 0; range_idx < 2; range_idx++)
		{
			char	   *expr_op = ">";

			if (everyOffset > 1)	/* for generated START for EVERY */
				expr_op = ">="; /* always be inclusive			 */
			else
				/* only inclusive set that way */
			if (pRI && (PART_EDGE_INCLUSIVE == pRI->partedge))
				expr_op = ">=";

			if (range_idx)		/* Only do it for the upper bound iteration */
			{
				pRI = (PartitionRangeItem *) (pBSpec->partEnd);

				if (pRI)
					valElts = pRI->partRangeVal;
				else
					valElts = NULL;

				expr_op = "<";

				/* for generated END for EVERY always be exclusive */
				if ((everyOffset + 1) < maxEveryOffset)
					expr_op = "<";
				else
				{
					/* only be inclusive if set that way */
					if (pRI && (PART_EDGE_INCLUSIVE == pRI->partedge))
						expr_op = "<=";
				}

				/* If have EVERY, and not the very first START or last END */
				if ((0 != everyOffset) && (everyOffset + 1 <= maxEveryOffset))
				{
					if (*pp_lc_anp)
					{
						if (everyOffset != 1)
							*pp_lc_anp = lnext(*pp_lc_anp);

						if (*pp_lc_anp)
						{
							allNewCols = lfirst(*pp_lc_anp);

							if (allNewCols)
								lc_every_val = list_head(allNewCols);
						}
					}
				}
			}					/* end if range_idx != 0 */

			lc_val = list_head(valElts);

			for (; lc_val;)		/* for all vals */
			{
				lc = list_head(colElts);
				pIndAND = NULL;

				for (; lc; lc = lnext(lc))		/* for all cols */
				{
					Node	   *pCol = lfirst(lc);
					Node	   *pEq = NULL;
					Value	   *pColConst;
					A_Const    *pValConst;
					ColumnRef  *pCRef;

					pCRef = makeNode(ColumnRef);		/* need columnref for
														 * WHERE */

					if (NULL == lc_val)
					{
						char	   *st_end = "START";

						if (range_idx)
						{
							st_end = "END";
						}

						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							  errmsg("too few columns in %s specification%s",
									 st_end,
									 at_depth),
							  parser_errposition(cxt->pstate, pBSpec->location)));
					}

					pColConst = (Value *) pCol;
					pValConst = (A_Const *) lfirst(lc_val);

					Assert(IsA(pColConst, String));

					pCRef->location = -1;
					pCRef->fields = list_make1(pColConst);

					if (!(IsA(pValConst, A_Const)))
					{
						Const	   *c = (Const *) pValConst;
						Type		typ;
						Form_pg_type pgtype;
						Datum		dat;
						A_Const    *aconst = makeNode(A_Const);
						Value	   *val;

						Assert(IsA(c, Const));

						typ = typeidType(c->consttype);
						pgtype = (Form_pg_type) GETSTRUCT(typ);
						dat = OidFunctionCall1(pgtype->typoutput, c->constvalue);

						val = makeString(DatumGetCString(dat));
						aconst->val = *val;
						aconst->location = -1;
						ReleaseSysCache(typ);

						pValConst = aconst;
					}

					pEq = (Node *) makeSimpleA_Expr(AEXPR_OP,
													expr_op,
													(Node *) pCRef,
													(Node *) pValConst,
													-1 /* position */ );

					exprStr = deparse_partition_rule((Node *) pEq, cxt->pstate, (Node *) pBSpec);

					/*
					 * for multiple cols - AND the matches eg: (col = value)
					 * AND (col = value)
					 */
					if (pIndAND)
					{
						char	   *pfoo = pstrdup(ANDBuf.data);

						pIndAND =
							(Node *) makeA_Expr(AEXPR_AND, NIL,
												pIndAND,
												pEq,
												-1 /* position */ );

						resetStringInfo(&ANDBuf);
						appendStringInfo(&ANDBuf, "((%s) and %s)",
										 exprStr, pfoo);

						pfree(pfoo);
					}
					else
					{
						pIndAND = pEq;
						resetStringInfo(&ANDBuf);
						appendStringInfo(&ANDBuf, "(%s)", exprStr);
					}

					lc_val = lnext(lc_val);

					if (((0 == range_idx)
						 && (everyOffset > 1))
						|| ((1 == range_idx)
							&& (everyOffset + 1 < maxEveryOffset))
						)
					{

						if (lc_every_val)
						{
							lc_every_val = lnext(lc_every_val);
						}
					}
				}				/* end for all cols */

				/*
				 * if more VALUES than columns, then complain
				 */
				if (lc_val)
				{
					char	   *st_end = "START";

					if (range_idx)
					{
						st_end = "END";
					}

					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("too many columns in %s specification%s",
									st_end,
									at_depth),
							 parser_errposition(cxt->pstate, pBSpec->location)));
				}

				if (pIndOR)
				{
					char	   *pfoo = pstrdup(ORBuf.data);

					/*
					 * XXX XXX build an AND for now.  But later we split this
					 * to distinguish START and END conditions
					 */

					pIndOR =
						(Node *) makeA_Expr(AEXPR_AND, NIL,
											pIndOR,
											pIndAND,
											-1 /* position */ );

					resetStringInfo(&ORBuf);
					appendStringInfo(&ORBuf, "((%s) AND %s)",
									 ANDBuf.data, pfoo);

					pfree(pfoo);

				}
				else
				{
					pIndOR = pIndAND;
					resetStringInfo(&ORBuf);
					appendStringInfo(&ORBuf, "(%s)", ANDBuf.data);
				}

			}					/* end for all vals */

		}						/* end for range_idx */

		/* first the CHECK constraint, then the INSERT statement */
		allRules = list_make1(pIndOR);
	}							/* end if RANGE */

	return allRules;
}	/* end make_partition_rules */


static char *
deparse_partition_rule(Node *pNode, ParseState *pstate, Node *parent)
{
	if (!pNode)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("unknown type or operator in partitioning rule"),
				 parser_errposition(pstate, exprLocation(parent))));

	switch (nodeTag(pNode))
	{
		case T_NullTest:
			{
				NullTest   *nt = (NullTest *) pNode;
				char	   *left;

				left = deparse_partition_rule((Node *) nt->arg, pstate, (Node *) nt);

				return psprintf("%s %s",
								left,
								nt->nulltesttype == IS_NULL ? "ISNULL" : "IS NOT NULL");
			}
			break;
		case T_Value:

			/* XXX XXX XXX ??? */

			break;
		case T_ColumnRef:
			{
				ColumnRef  *pCRef = (ColumnRef *) pNode;
				List	   *coldefs = pCRef->fields;
				ListCell   *lc;
				StringInfoData sid;
				int			colcnt = 0;

				initStringInfo(&sid);

				lc = list_head(coldefs);

				for (; lc; lc = lnext(lc))		/* for all cols */
				{
					Node	   *pCol = lfirst(lc);
					char	   *left;

					left = deparse_partition_rule(pCol, pstate, parent);
					if (colcnt)
						appendStringInfo(&sid, ".");

					appendStringInfo(&sid, "%s", left);

					colcnt++;
				}				/* end for all cols */

				return sid.data;
			}
			break;
		case T_String:
			return pstrdup(strVal(pNode));
			break;
		case T_Integer:
			return psprintf("%ld", intVal(pNode));
		case T_Float:
			return psprintf("%f", floatVal(pNode));
		case T_A_Const:
			{
				A_Const    *acs = (A_Const *) pNode;

				if (acs->val.type == T_String)
				{
					return psprintf("\'%s\'", acs->val.val.str);
				}
				return deparse_partition_rule((Node *) &(acs->val), pstate, parent);
			}
			break;
		case T_A_Expr:
			{
				A_Expr	   *ax = (A_Expr *) pNode;
				char	   *left;
				char	   *right;
				char	   *infix_op;

				switch (ax->kind)
				{
					case AEXPR_OP:		/* normal operator */
					case AEXPR_AND:		/* booleans - name field is unused */
					case AEXPR_OR:
						break;
					default:
						return 0;
				}
				left = deparse_partition_rule(ax->lexpr, pstate, parent);
				right = deparse_partition_rule(ax->rexpr, pstate, parent);

				switch (ax->kind)
				{
					case AEXPR_OP:		/* normal operator */
						infix_op = strVal(lfirst(list_head(ax->name)));
						break;
					case AEXPR_AND:		/* booleans - name field is unused */
						infix_op = "AND";
						break;
					case AEXPR_OR:
						infix_op = "OR";
						break;
					default:
						return 0;
				}
				return psprintf("(%s %s %s)",
								left, infix_op, right);
			}
			break;
		default:
			break;
	}

	elog(ERROR, "unexpected node type %u in partitioning rule", nodeTag(pNode));
	return NULL;
}

/* XXX: major cleanup required. Get rid of gotos at least */
static int
partition_range_compare(CreateStmtContext *cxt,
						CreateStmt *stmt, PartitionBy *pBy,
						char *at_depth, int partNumber,
						char *compare_op,		/* =, <, > only */
						PartitionRangeItem *pRI1,
						PartitionRangeItem *pRI2)
{
	int			rc = -1;
	ListCell   *lc1 = NULL;
	ListCell   *lc2 = NULL;
	List	   *cop = lappend(NIL, makeString(compare_op));

	if (!pRI1 || !pRI2)			/* error */
		return rc;

	lc1 = list_head(pRI1->partRangeVal);
	lc2 = list_head(pRI2->partRangeVal);
L_redoLoop:
	for (; lc1 && lc2;)
	{
		Node	   *n1 = lfirst(lc1);
		Node	   *n2 = lfirst(lc2);

		if (!equal(n1, n2))
			break;

		lc1 = lnext(lc1);
		lc2 = lnext(lc2);
	}

	if (!lc1 && !lc2)			/* both empty, so all values are equal */
	{
		if (strcmp("=", compare_op) == 0)
			return 1;
		else
			return 0;
	}
	else if (lc1 && lc2)		/* both not empty, so last value was different */
	{
		Datum		res;
		Node	   *n1 = lfirst(lc1);
		Node	   *n2 = lfirst(lc2);

		/*
		 * XXX XXX: can't trust equal() code on "typed" data like date types
		 * (because it compares things like "location") so do a real compare
		 * using eval .
		 */
		res = eval_basic_opexpr(cxt->pstate, cop, n1, n2, NULL, NULL, NULL, -1);

		if (DatumGetBool(res) && strcmp("=", compare_op) == 0)
		{
			/* surprise! they were equal after all.  So keep going... */
			lc1 = lnext(lc1);
			lc2 = lnext(lc2);

			goto L_redoLoop;
		}

		return DatumGetBool(res);
	}
	else if (lc1 || lc2)
	{
		/* lists of different lengths */
		if (strcmp("=", compare_op) == 0)
			return 0;

		/* longer list is bigger? */

		if (strcmp("<", compare_op) == 0)
		{
			if (lc1)
				return 0;
			else
				return 1;
		}
		if (strcmp(">", compare_op) == 0)
		{
			if (lc1)
				return 1;
			else
				return 0;
		}
	}

	return rc;
}	/* end partition_range_compare */

/* Compare two Consts of the same type, using a less than (<) operator. */
static int
cmp_consts(RegProcedure lt_proc, Const *c1, Const *c2)
{
	FmgrInfo	flinfo;
	FunctionCallInfoData fcinfo;

	fmgr_info(lt_proc, &flinfo);

	if (flinfo.fn_strict)
	{
		if (!c1->constisnull && c2->constisnull)
			return -1;
		if (c1->constisnull && !c2->constisnull)
			return 1;
		if (c1->constisnull && c2->constisnull)
			return 0;
	}

	/* Is c1 < c2 ? */
	InitFunctionCallInfoData(fcinfo, &flinfo, 2, c1->constcollid, NULL, NULL);
	fcinfo.arg[0] = c1->constvalue;
	fcinfo.argnull[0] = c1->constisnull;
	fcinfo.arg[1] = c2->constvalue;
	fcinfo.argnull[1] = c2->constisnull;
	if (DatumGetBool(FunctionCallInvoke(&fcinfo)))
		return -1;

	/* Is c2 < c1 ? */
	InitFunctionCallInfoData(fcinfo, &flinfo, 2, c1->constcollid, NULL, NULL);
	fcinfo.arg[0] = c2->constvalue;
	fcinfo.argnull[0] = c2->constisnull;
	fcinfo.arg[1] = c1->constvalue;
	fcinfo.argnull[1] = c1->constisnull;
	if (DatumGetBool(FunctionCallInvoke(&fcinfo)))
		return 1;

	/* must be equal, then */
	return 0;
}

static int
part_el_cmp(void *a, void *b, void *arg)
{
	RegProcedure *lt_funcs = (RegProcedure *) arg;
	PartitionElem *el1 = (PartitionElem *) a;
	PartitionElem *el2 = (PartitionElem *) b;
	PartitionBoundSpec *bs1;
	PartitionBoundSpec *bs2;
	int			i;
	List	   *start1 = NIL,
			   *start2 = NIL;
	ListCell   *lc1,
			   *lc2;

	/*
	 * We call this function from all over the place so don't assume that
	 * things are valid.
	 */

	if (!el1)
		return -1;
	else if (!el2)
		return 1;

	if (el1->isDefault)
		return -1;

	if (el2->isDefault)
		return 1;

	bs1 = (PartitionBoundSpec *) el1->boundSpec;
	bs2 = (PartitionBoundSpec *) el2->boundSpec;

	if (!bs1)
		return -1;
	if (!bs2)
		return 1;

	if (bs1->partStart)
		start1 = ((PartitionRangeItem *) bs1->partStart)->partRangeVal;
	if (bs2->partStart)
		start2 = ((PartitionRangeItem *) bs2->partStart)->partRangeVal;

	if (!start1 && !start2)
	{
		/* these cases are syntax errors but they'll be picked up later */
		if (!bs1->partEnd && !bs2->partEnd)
			return 0;
		else if (!bs1->partEnd)
			return 1;
		else if (!bs2->partEnd)
			return -1;
		else
		{
			List	   *end1 = ((PartitionRangeItem *) bs1->partEnd)->partRangeVal;
			List	   *end2 = ((PartitionRangeItem *) bs2->partEnd)->partRangeVal;

			i = 0;
			forboth(lc1, end1, lc2, end2)
			{
				Const	   *c1 = lfirst(lc1);
				Const	   *c2 = lfirst(lc2);
				int			cmp;

				cmp = cmp_consts(lt_funcs[i], c1, c2);
				if (cmp != 0)
					return cmp;
				i++;
			}
			/* equal */
			return 0;
		}
	}
	else if (!start1 && start2)
	{
		/*
		 * compare end1 to start2. Whether the end and start are inclusive or
		 * exclusive is very important here. For example, if end1 is exclusive
		 * and start2 is inclusive, their being equal means that end1 finishes
		 * *before* start2.
		 */

		PartitionRangeItem *pri1 = (PartitionRangeItem *) bs1->partEnd;
		PartitionRangeItem *pri2 = (PartitionRangeItem *) bs2->partStart;
		List	   *end1 = pri1->partRangeVal;
		PartitionEdgeBounding pe1 = pri1->partedge;
		PartitionEdgeBounding pe2 = pri2->partedge;

		Assert(pe1 != PART_EDGE_UNSPECIFIED);
		Assert(pe2 != PART_EDGE_UNSPECIFIED);

		i = 0;
		forboth(lc1, end1, lc2, start2)
		{
			Const	   *c1 = lfirst(lc1);
			Const	   *c2 = lfirst(lc2);
			int			cmp;

			/* try < first */
			cmp = cmp_consts(lt_funcs[i], c1, c2);

			if (cmp != 0)
				return cmp;
			else
			{
				/* equal, but that might actually mean < */
				if (pe1 == PART_EDGE_EXCLUSIVE)
					return -1;	/* it's less than */
				else if (pe1 == PART_EDGE_INCLUSIVE &&
						 pe2 == PART_EDGE_EXCLUSIVE)
					return -1;	/* it's less than */

				/* otherwise, they're equal */
			}

			i++;
		}
		return 0;
	}
	else if (start1 && !start2)
	{
		/* opposite of above */
		return -part_el_cmp(b, a, arg);
	}
	else
	{
		/* we have both starts */
		PartitionRangeItem *pri1 = (PartitionRangeItem *) bs1->partStart;
		PartitionRangeItem *pri2 = (PartitionRangeItem *) bs2->partStart;
		PartitionEdgeBounding pe1 = pri1->partedge;
		PartitionEdgeBounding pe2 = pri2->partedge;

		i = 0;
		forboth(lc1, start1, lc2, start2)
		{
			Const	   *c1 = lfirst(lc1);
			Const	   *c2 = lfirst(lc2);
			int			cmp;

			cmp = cmp_consts(lt_funcs[i], c1, c2);
			if (cmp != 0)
				return cmp;
			else
			{
				/* equal, but that might actually mean something else */
				if (pe1 == PART_EDGE_INCLUSIVE &&
					pe2 == PART_EDGE_EXCLUSIVE)
					return -1;	/* actually, it was < */
				else if (pe1 == PART_EDGE_EXCLUSIVE &&
						 pe2 == PART_EDGE_INCLUSIVE)
					return 1;
			}

			i++;
		}
	}

	/* all equal */
	return 0;
}

static List *
sort_range_elems(List *opclasses, List *elems)
{
	ListCell   *lc;
	PartitionElem *clauses;
	RegProcedure *lt_funcs;
	int			i;
	List	   *newelems = NIL;

	/* Get the < operator for each operator class */
	lt_funcs = palloc(list_length(opclasses) * sizeof(RegProcedure));
	i = 0;
	foreach(lc, opclasses)
	{
		Oid			opclass = lfirst_oid(lc);
		Oid			intype = get_opclass_input_type(opclass);
		Oid			opfam = get_opclass_family(opclass);
		Oid			opoid;

		opoid = get_opfamily_member(opfam, intype, intype, BTLessStrategyNumber);
		lt_funcs[i] = get_opcode(opoid);
		i++;
	}

	i = 0;
	clauses = palloc(sizeof(PartitionElem) * list_length(elems));
	foreach(lc, elems)
		clauses[i++] = *(PartitionElem *) lfirst(lc);

	qsort_arg(clauses, list_length(elems), sizeof(PartitionElem),
			  (qsort_arg_comparator) part_el_cmp, (void *) lt_funcs);

	for (i = 0; i < list_length(elems); i++)
		newelems = lappend(newelems, &clauses[i]);

	return newelems;
}

int
validate_partition_spec(CreateStmtContext *cxt,
						CreateStmt *stmt, PartitionBy *pBy,
						char *at_depth, int partNumber)
{
	PartitionSpec *pSpec;
	char		namBuf[NAMEDATALEN];
	List	   *partElts;
	ListCell   *lc = NULL;
	List	   *allPartNames = NIL;
	partValidationState *vstate;
	PartitionElem *pDefaultElem = NULL;
	int			partno = 0;
	List	   *enc_cls = NIL;

	vstate = palloc0(sizeof(partValidationState));

	vstate->cxt = cxt;
	vstate->stmt = stmt;
	vstate->pBy = pBy;
	vstate->at_depth = at_depth;
	vstate->partNumber = partNumber;

	Assert(pBy);
	pSpec = (PartitionSpec *) pBy->partSpec;

	/*
	 * Find number of partitions in the specification, and determine the
	 * subpartition specifications.  Perform basic error checking on boundary
	 * specifications.
	 */

	/*
	 * NOTE: a top-level PartitionSpec never has a subSpec, but a
	 * PartitionSpec derived from an inline SubPartition specification might
	 * have one
	 */


	/*----------
	 * track previous boundary spec to check for this subtle problem:
	 *
	 *	(
	 *	partition aa start (2007,1) end (2008,2),
	 *	partition bb start (2008,2) end (2009,3)
	 *	);
	 *	This declaration would create four partitions:
	 *	1. named aa, starting at 2007, 1
	 *	2. unnamed, ending at 2008, 2
	 *	3. named bb, starting at 2008, 2
	 *	4. unnamed, ending at 2009, 3
	 *
	 *	Warn user if they do this, since they probably wanted
	 *	1. named aa, starting at 2007, 1 and ending at 2008, 2
	 *	2. named bb, starting at 2008, 2 and ending at 2009, 3
	 *
	 *	The extra comma between the start and end is the problem.
	 *----------
	 */

	if (pBy->partType == PARTTYP_RANGE)
		preprocess_range_spec(vstate);

	partElts = pSpec->partElem;

	/*
	 * If this is a RANGE partition, we might have gleaned some encoding
	 * clauses already, because preprocess_range_spec() looks at partition
	 * elements.
	 */
	enc_cls = pSpec->enc_clauses;

	foreach(lc, partElts)
	{
		PartitionElem *pElem = (PartitionElem *) lfirst(lc);
		PartitionBoundSpec *pBSpec = NULL;

		if (!IsA(pElem, PartitionElem))
		{
			Insist(IsA(pElem, ColumnReferenceStorageDirective));
			enc_cls = lappend(enc_cls, lfirst(lc));
			continue;
		}
		vstate->pElem = pElem;

		if (pSpec->istemplate && pElem->colencs)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("partition specific ENCODING clause not supported in SUBPARTITION TEMPLATE"),
					 parser_errposition(cxt->pstate, pElem->location)));


		/*
		 * We've done all possible expansions so now number the partition
		 * elements so that we can set the position when adding the
		 * configuration to the catalog.
		 */
		pElem->partno = ++partno;

		if (pElem)
		{
			pBSpec = (PartitionBoundSpec *) pElem->boundSpec;
			vstate->spec = (Node *) pBSpec;

			/*
			 * handle all default partition cases:
			 *
			 * - Can only have a single DEFAULT partition.
			 * - Default partitions cannot have a boundary specification.
			 */
			if (pElem->isDefault)
			{
				Assert(pElem->partName);		/* default partn must have a
												 * name */
				snprintf(namBuf, sizeof(namBuf), " \"%s\"",
						 pElem->partName);

				if (pDefaultElem)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("multiple default partitions are not "
									"allowed"),
							 parser_errposition(cxt->pstate, pElem->location)));

				}

				pDefaultElem = pElem;	/* save the default */

				if (pBSpec)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("invalid use of boundary specification "
									"for DEFAULT partition%s%s",
									namBuf,
									at_depth),
							 parser_errposition(cxt->pstate, pElem->location)));

				}

			}					/* end if is default */

			if (pElem->partName)
			{
				bool		doit = true;

				snprintf(namBuf, sizeof(namBuf), " \"%s\"",
						 pElem->partName);

				/*
				 * We might have expanded an EVERY clause here. If so, just
				 * add the first partition name.
				 */
				if (doit)
				{
					ListCell *alc;

					foreach(alc, allPartNames)
					{
						if (strcmp((char *) lfirst(alc), pElem->partName) == 0)
							ereport(ERROR,
									(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
									 errmsg("duplicate partition name for partition%s%s",
											namBuf,
											at_depth),
									 parser_errposition(cxt->pstate, pElem->location)));
					}
					allPartNames = lappend(allPartNames, pElem->partName);
				}
			}
			else
			{
				if (pElem->AddPartDesc)
					snprintf(namBuf, sizeof(namBuf), "%s", pElem->AddPartDesc);
				else
					snprintf(namBuf, sizeof(namBuf), " number %d", partno);
			}
		}
		else
		{
			if (pElem->AddPartDesc)
				snprintf(namBuf, sizeof(namBuf), "%s", pElem->AddPartDesc);
			else
				snprintf(namBuf, sizeof(namBuf), " number %d", partno);
		}

		/* don't have to validate default partition boundary specs */
		if (pElem->isDefault)
			continue;

		vstate->namBuf = namBuf;

		switch (pBy->partType)
		{
			case PARTTYP_RANGE:
				validate_range_partition(vstate);
				break;

			case PARTTYP_LIST:
				validate_list_partition(vstate);
				break;

			default:
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("unknown partition type %d %s",
								pBy->partType, at_depth),
						 parser_errposition(cxt->pstate, pBy->location)));
				break;

		}
	}							/* end foreach(lc, partElts) */

	pSpec->enc_clauses = transformPartitionStorageEncodingClauses(enc_cls);

	foreach(lc, partElts)
	{
		PartitionElem *elem = (PartitionElem *) lfirst(lc);

		if (!IsA(elem, PartitionElem))
			continue;

		merge_partition_encoding(cxt->pstate, elem, pSpec->enc_clauses);
	}

	/* validate_range_partition might have changed some boundaries */
	if (pBy->partType == PARTTYP_RANGE)
		pSpec->partElem = sort_range_elems(pBy->keyopclass, partElts);

	if (partNumber > -1 && partno != partNumber)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("PARTITIONS \"%d\" must match \"%d\" elements "
						"in specification%s",
						partNumber, partno, vstate->at_depth),
				 parser_errposition(cxt->pstate, pBy->location)));

	if (vstate->allRangeVals)
		list_free(vstate->allRangeVals);

	if (allPartNames)
		list_free(allPartNames);

	return partno;
}	/* end validate_partition_spec */

static Const *
flatten_partition_val(Node *node, Oid target_type)
{
	if (IsA(node, Const))
		return (Const *) node;
	else
	{
		Datum		res;
		Oid			curtyp;
		Const	   *c;
		Type		typ = typeidType(target_type);
		int32		typmod = ((Form_pg_type) GETSTRUCT(typ))->typtypmod;
		int32		typcollation = ((Form_pg_type) GETSTRUCT(typ))->typcollation;
		int16		typlen = ((Form_pg_type) GETSTRUCT(typ))->typlen;
		bool		typbyval = ((Form_pg_type) GETSTRUCT(typ))->typbyval;
		bool		isnull;

		ReleaseSysCache(typ);

		curtyp = exprType(node);
		Assert(OidIsValid(curtyp));

		if (curtyp != target_type && OidIsValid(target_type))
		{
			node = coerce_type(NULL, node, curtyp, target_type, typmod,
							   COERCION_EXPLICIT,
							   COERCE_IMPLICIT_CAST,
							   -1);

			if (!PointerIsValid(node))
				elog(ERROR, "could not coerce partitioning parameter");
		}

		res = partition_arg_get_val(node, &isnull);
		c = makeConst(target_type, typmod, typcollation, typlen, res, isnull, typbyval);

		return c;
	}

}

/*
 * Get the actual value from the expression. There are only a limited range
 * of cases we must cover because the parser guarantees constant input.
 */
static Datum
partition_arg_get_val(Node *node, bool *isnull)
{
	Const	   *c;

	c = (Const *) evaluate_expr((Expr *) node,
								exprType(node),
								exprTypmod(node),
								exprCollation(node));
	if (!IsA(c, Const))
		elog(ERROR, "partition parameter is not constant");

	*isnull = c->constisnull;
	return c->constvalue;
}


static void
preprocess_range_spec(partValidationState *vstate)
{
	PartitionSpec *spec = (PartitionSpec *) vstate->pBy->partSpec;
	ParseState *pstate = vstate->cxt->pstate;
	ListCell   *lc;
	List	   *plusop = list_make2(makeString("pg_catalog"), makeString("+"));
	List	   *ltop = list_make2(makeString("pg_catalog"), makeString("<"));
	List	   *coltypes = NIL;
	List	   *stenc;
	List	   *newelts = NIL;

	foreach(lc, vstate->pBy->keys)
	{
		char	   *colname = strVal(lfirst(lc));
		bool		found = false;
		ListCell   *lc2;
		TypeName   *typname = NULL;

		foreach(lc2, vstate->cxt->columns)
		{
			ColumnDef  *column = lfirst(lc2);

			if (strcmp(column->colname, colname) == 0)
			{
				found = true;
				if (!OidIsValid(column->typeName->typeOid))
				{
					typenameTypeIdAndMod(pstate, column->typeName,
										 &column->typeName->typeOid,
										 &column->typeName->typemod);
				}
				typname = column->typeName;
				break;
			}
		}
		Assert(found);
		coltypes = lappend(coltypes, typname);
	}

	partition_range_every(pstate, vstate->pBy, coltypes, vstate->at_depth,
						  vstate);

	/*
	 * Must happen after partition_range_every(), since that gathers up
	 * encoding clauses for us.
	 */
	stenc = spec->enc_clauses;

	/*
	 * Iterate over the elements in a given partition specification and expand
	 * any EVERY clauses into raw elements.
	 */
	foreach(lc, spec->partElem)
	{
		PartitionElem *el = lfirst(lc);
		PartitionBoundSpec *pbs = (PartitionBoundSpec *) el->boundSpec;

		Node	   *pStoreAttr = NULL;
		ListCell   *lc2;
		bool		bTablename = false;

		if (IsA(el, ColumnReferenceStorageDirective))
		{
			stenc = lappend(stenc, el);
			continue;
		}

		/* we might not have a boundary spec if user just specified DEFAULT */
		if (!pbs)
		{
			newelts = lappend(newelts, el);
			continue;
		}

		pbs = (PartitionBoundSpec *) transformExpr(pstate, (Node *) pbs,
												   EXPR_KIND_PARTITION_EXPRESSION);

		pStoreAttr = el->storeAttr;

		/*
		 * MPP-6297: check for WITH (tablename=name) clause [only for
		 * dump/restore, set deep in the guts of partition_range_every...]
		 */
		bTablename = (NULL != pbs->pWithTnameStr);

		if (!bTablename && pbs->partEvery)
		{
			/*
			 * the start expression might come from a previous end expression
			 */
			List	   *start = NIL;
			List	   *curstart = NIL;
			List	   *end = ((PartitionRangeItem *) pbs->partEnd)->partRangeVal;
			List	   *newend = NIL;
			List	   *every =
			(List *) ((PartitionRangeItem *) pbs->partEvery)->partRangeVal;
			List	   *everyinc;
			bool		lessthan = true;
			int			everycount = 0;
			List	   *everyelts = NIL;
			bool		first = true;
			List	   *everytypes = NIL;
			int			i;

			Assert(pbs->partStart);
			Assert(pbs->partEnd);
			Assert(IsA(end, List));
			Assert(IsA(every, List));

			/* we modify every, so copy it */
			everyinc = copyObject(every);

			while (lessthan)
			{
				Datum		res;
				Oid			restypid;
				PartitionElem *newel;
				PartitionBoundSpec *newbs;
				PartitionRangeItem *newri;
				ListCell   *lctypes,
						   *lceveryinc;
				ListCell   *lcstart,
						   *lcend,
						   *lcevery,
						   *lccol,
						   *lclastend;
				List	   *lastend = NIL;

				/*
				 * Other parts of the parser want to know how many clauses we
				 * expanded here.
				 */
				everycount++;

				if (start == NIL)
				{
					start = ((PartitionRangeItem *) pbs->partStart)->partRangeVal;

					lcend = list_head(end);
					/* coerce to target type */
					forboth(lcstart, start, lccol, coltypes)
					{
						TypeName   *typ = lfirst(lccol);
						Node	   *newnode;
						int32		typmod;
						Oid			typid;

						typenameTypeIdAndMod(pstate, typ, &typid, &typmod);

						newnode = coerce_partition_value(pstate,
														 lfirst(lcstart),
														 typid, typmod,
														 PARTTYP_RANGE);

						lfirst(lcstart) = newnode;

						/* be sure we coerce the end value */
						newnode = coerce_partition_value(pstate,
														 lfirst(lcend),
														 typid,
														 typmod,
														 PARTTYP_RANGE);

						assign_expr_collations(pstate, newnode);

						lfirst(lcend) = newnode;
						lcend = lnext(lcend);
					}

					curstart = copyObject(start);
					forboth(lccol, coltypes, lcevery, every)
					{
						TypeName   *typ = lfirst(lccol);
						HeapTuple	optup;
						List	   *opname = list_make2(makeString("pg_catalog"),
														makeString("+"));
						Node	   *e = lfirst(lcevery);
						Oid			rtypeId = exprType(e);
						Oid			newrtypeId;

						/* first, make sure we can build up an operator */
						optup = oper(pstate, opname, typ->typeOid, rtypeId,
									 true, -1);
						if (!HeapTupleIsValid(optup))
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("could not identify operator for partitioning operation between type \"%s\" and type \"%s\"",
											format_type_be(typ->typeOid),
											format_type_be(rtypeId)),
									 errhint("Add an explicit cast to the partitioning parameters")));


						newrtypeId = ((Form_pg_operator) GETSTRUCT(optup))->oprright;
						ReleaseSysCache(optup);

						if (rtypeId != newrtypeId)
						{
							Type		newetyp = typeidType(newrtypeId);
							int32		typmod =
							((Form_pg_type) GETSTRUCT(newetyp))->typtypmod;

							ReleaseSysCache(newetyp);

							/* we need to coerce */
							e = coerce_partition_value(pstate,
													   e,
													   newrtypeId,
													   typmod,
													   PARTTYP_RANGE);

							lfirst(lcevery) = e;
							rtypeId = newrtypeId;
						}

						assign_expr_collations(pstate, e);

						everytypes = lappend_oid(everytypes, rtypeId);
					}
				}
				else
				{
					curstart = newend;
					lastend = newend;
					newend = NIL;
				}

				lcend = list_head(end);
				lcevery = list_head(everyinc);
				lclastend = list_head(lastend);

				forboth(lcstart, start, lccol, coltypes)
				{
					Const	   *mystart = lfirst(lcstart);
					TypeName   *type = lfirst(lccol);
					Const	   *myend;
					Const	   *clauseend;
					Const	   *clauseevery;
					int32		typmod;
					Oid			typid;
					int16		len;
					bool		typbyval;
					Oid			typcollation;

					typenameTypeIdAndMod(pstate, type, &typid, &typmod);
					get_typlenbyval(typid, &len, &typbyval);
					typcollation = get_typcollation(typid);
					
					Assert(lcevery);
					Assert(lcend);

					clauseevery = lfirst(lcevery);
					clauseend = lfirst(lcend);

					/* add the every value to the start */
					res = eval_basic_opexpr(pstate, plusop, (Node *) mystart,
											(Node *) clauseevery,
											&typbyval, &len, &typid,
											-1);

					myend = makeConst(typid, typmod,
									  typcollation,
									  len,
									  datumCopy(res, typbyval, len),
									  false, typbyval);

					/* make sure res is bigger than the last value */
					if (lclastend)
					{
						Oid			typ = InvalidOid;
						Const	   *prevval = (Const *) lfirst(lclastend);
						Datum		is_lt = eval_basic_opexpr(pstate, ltop,
															(Node *) prevval,
															  (Node *) myend,
															NULL, NULL, &typ,
															  -1);

						if (!DatumGetBool(is_lt))
							elog(ERROR, "every interval too small");
					}
					restypid = InvalidOid;
					res = eval_basic_opexpr(pstate, ltop, (Node *) myend,
											(Node *) clauseend, NULL, NULL,
											&restypid, -1);
					if (!DatumGetBool(res))
					{
						newend = end;
						lessthan = false;
						break;
					}
					newend = lappend(newend, myend);

					lcend = lnext(lcend);
					lcevery = lnext(lcevery);
					if (lclastend)
						lclastend = lnext(lclastend);
				}

				lctypes = list_head(everytypes);
				forboth(lcevery, every, lceveryinc, everyinc)
				{
					Oid			typid = lfirst_oid(lctypes);
					bool		byval = get_typbyval(typid);
					int16		typlen = get_typlen(typid);
					Oid			typcollation = get_typcollation(typid);
					Const	   *c;

					/* increment every */
					res = eval_basic_opexpr(pstate, plusop,
											(Node *) lfirst(lcevery),
											(Node *) lfirst(lceveryinc),
											NULL, NULL,
											&typid, -1);

					c = makeConst(typid,
								  -1,
								  typcollation,
								  typlen,
								  res, false, byval);
					pfree(lfirst(lceveryinc));
					lfirst(lceveryinc) = c;
				}

				newel = makeNode(PartitionElem);
				newel->subSpec = copyObject(el->subSpec);
				newel->storeAttr = copyObject(el->storeAttr);
				newel->AddPartDesc = copyObject(el->AddPartDesc);
				newel->location = el->location;

				newbs = makeNode(PartitionBoundSpec);

				newel->boundSpec = (Node *) newbs;

				/* start */
				newri = makeNode(PartitionRangeItem);

				/* modifier only relevant on the first iteration */
				if (everycount == 1)
					newri->partedge =
						((PartitionRangeItem *) pbs->partStart)->partedge;
				else
					newri->partedge = PART_EDGE_INCLUSIVE;

				newri->location =
					((PartitionRangeItem *) pbs->partStart)->location;
				newri->partRangeVal = curstart;
				newbs->partStart = (Node *) newri;

				/* end */
				newri = makeNode(PartitionRangeItem);
				newri->partedge = PART_EDGE_EXCLUSIVE;
				newri->location =
					((PartitionRangeItem *) pbs->partEnd)->location;
				newri->partRangeVal = newend;
				newbs->partEnd = (Node *) newri;

				/* every */
				newbs->partEvery = (Node *) copyObject(pbs->partEvery);
				newbs->location = pbs->location;
				newbs->everyGenList = pbs->everyGenList;

				everyelts = lappend(everyelts, newel);

				if (first)
					first = false;
			}

			/*
			 * Update the final PartitionElem's partEnd modifier if it isn't
			 * the default
			 */
			if (((PartitionRangeItem *) pbs->partEnd)->partedge !=
				PART_EDGE_EXCLUSIVE)
			{
				PartitionElem *elem = lfirst(list_tail(everyelts));
				PartitionBoundSpec *s = (PartitionBoundSpec *) elem->boundSpec;
				PartitionRangeItem *ri = (PartitionRangeItem *) s->partEnd;

				ri->partedge = ((PartitionRangeItem *) pbs->partEnd)->partedge;
			}

			/* add everycount to each EVERY clause */
			i = 0;
			foreach(lc2, everyelts)
			{
				PartitionElem *el2 = lfirst(lc2);
				PartitionBoundSpec *pbs = (PartitionBoundSpec *) el2->boundSpec;
				PartitionRangeItem *ri = (PartitionRangeItem *) pbs->partEvery;

				ri->everycount = everycount;

				/*
				 * Generate the new name, which is parname + every count but
				 * only if every expands to more than one partition.
				 */
				if (el->partName)
				{
					char		newname[sizeof(NameData) + 10];

					if (list_length(everyelts) > 1)
						snprintf(newname, sizeof(newname),
								 "%s_%u", el->partName,
								 ++i);
					else
						snprintf(newname, sizeof(newname),
								 "%s", el->partName);

					if (strlen(newname) > NAMEDATALEN)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("partition name \"%s\" too long",
										el->partName),
						  parser_errposition(pstate, el->location)));

					el2->partName = pstrdup(newname);
				}
			}

			newelts = list_concat(newelts, everyelts);
		}
		else
		{
			if (pbs->partStart)
			{
				ListCell   *lccol;
				ListCell   *lcstart;
				List	   *start =
				((PartitionRangeItem *) pbs->partStart)->partRangeVal;

				/* coerce to target type */
				forboth(lcstart, start, lccol, coltypes)
				{
					Node	   *mystart = lfirst(lcstart);
					TypeName   *typ = lfirst(lccol);
					Node	   *newnode;
					Oid			typid;
					int32		typmod;

					typenameTypeIdAndMod(pstate, typ, &typid, &typmod);

					newnode = coerce_partition_value(pstate,
													 mystart,
													 typid, typmod,
													 PARTTYP_RANGE);

					assign_expr_collations(pstate, newnode);

					lfirst(lcstart) = newnode;
				}
			}

			if (pbs->partEnd)
			{
				List	   *end = ((PartitionRangeItem *) pbs->partEnd)->partRangeVal;
				ListCell   *lccol;
				ListCell   *lcend;

				/* coerce to target type */
				forboth(lcend, end, lccol, coltypes)
				{
					Node	   *myend = lfirst(lcend);
					TypeName   *typ = lfirst(lccol);
					Node	   *newnode;
					Oid			typid;
					int32		typmod;

					typenameTypeIdAndMod(pstate, typ, &typid, &typmod);

					newnode = coerce_partition_value(pstate,
													 myend,
													 typid, typmod,
													 PARTTYP_RANGE);

					assign_expr_collations(pstate, newnode);

					lfirst(lcend) = newnode;
				}
			}
			newelts = lappend(newelts, el);
		}
	}

	/* do an initial sort */
	spec->partElem = sort_range_elems(vstate->pBy->keyopclass, newelts);
	spec->enc_clauses = stenc;
}

/*
 * XXX: this is awful, rewrite
 */
static int
partition_range_every(ParseState *pstate, PartitionBy *pBy, List *coltypes,
					  char *at_depth, partValidationState *vstate)
{
	PartitionSpec *pSpec;
	char		namBuf[NAMEDATALEN];
	int			numElts = 0;
	List	   *partElts;
	ListCell   *lc = NULL;
	ListCell   *lc_prev = NULL;
	List	   *stenc = NIL;

	Assert(pBy);
	Assert(pBy->partType == PARTTYP_RANGE);

	pSpec = (PartitionSpec *) pBy->partSpec;

	if (pSpec)					/* no bound spec for default partition */
	{
		partElts = pSpec->partElem;
		stenc = pSpec->enc_clauses;

		lc = list_head(partElts);
	}

	for (; lc;)					/* foreach */
	{
		PartitionElem *pElem = (PartitionElem *) lfirst(lc);

		PartitionBoundSpec *pBSpec = NULL;
		int			numCols = 0;
		int			colCnt = 0;
		Const	   *everyCnt;
		PartitionRangeItem *pRI_Start = NULL;
		PartitionRangeItem *pRI_End = NULL;
		PartitionRangeItem *pRI_Every = NULL;
		ListCell   *lc_Start = NULL;
		ListCell   *lc_End = NULL;
		ListCell   *lc_Every = NULL;
		Node	   *pStoreAttr = NULL;
		List	   *allNewCols;
		List	   *allNewPartns = NIL;
		List	   *lastval = NIL;
		bool		do_every_param_test = true;

		pBSpec = NULL;

		if (pElem && IsA(pElem, ColumnReferenceStorageDirective))
		{
			stenc = lappend(stenc, pElem);
			goto l_next_iteration;
		}

		numElts++;

		if (!pElem)
			goto l_EveryLoopEnd;
		else
		{
			pBSpec = (PartitionBoundSpec *) pElem->boundSpec;

			if (!pBSpec)
				goto l_EveryLoopEnd;

			pStoreAttr = pElem->storeAttr;

			if (pElem->partName)
			{
				snprintf(namBuf, sizeof(namBuf), " \"%s\"",
						 pElem->partName);
			}
			else
			{
				snprintf(namBuf, sizeof(namBuf), " number %d",
						 numElts);
			}

			if (pElem->isDefault)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("invalid use of boundary specification "
								"for DEFAULT partition%s%s",
								namBuf,
								at_depth),
						 parser_errposition(pstate, pElem->location)));
			}					/* end if is default */

			/* MPP-3541: invalid use of LIST spec */
			if (!IsA(pBSpec, PartitionBoundSpec))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("invalid use of LIST boundary specification "
								"in partition%s of type RANGE%s",
								namBuf,
								at_depth),
				/* MPP-4249: use value spec location if have one */
						 ((IsA(pBSpec, PartitionValuesSpec)) ?
						  parser_errposition(pstate,
								((PartitionValuesSpec *) pBSpec)->location) :
				/* else use invalid parsestate/position */
						  parser_errposition(NULL, 0)
						  )
						 ));
			}

			Assert(IsA(pBSpec, PartitionBoundSpec));

		}

		/* have a valid range bound spec */

		if (!pBSpec->partEvery)
			goto l_EveryLoopEnd;

		/* valid EVERY needs a START and END */
		if (!(pBSpec->partStart &&
			  pBSpec->partEnd))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("EVERY clause in partition%s "
							"requires START and END%s",
							namBuf,
							at_depth),
					 parser_errposition(pstate, pBSpec->location)));
		}

		/*
		 * MPP-6297: check for WITH (tablename=name) clause [magic to make
		 * dump/restore work by ignoring EVERY]
		 */
		if (pStoreAttr && ((AlterPartitionCmd *) pStoreAttr)->arg1)
		{
			ListCell   *prev_lc = NULL;
			ListCell   *def_lc = NULL;
			List	   *pWithList = (List *)
			(((AlterPartitionCmd *) pStoreAttr)->arg1);
			bool		bTablename = false;

			foreach(def_lc, pWithList)
			{
				DefElem    *pDef = (DefElem *) lfirst(def_lc);

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

					bTablename = true;
					char	   *widthname_str = defGetString(pDef);

					pBSpec->pWithTnameStr = pstrdup(widthname_str);

					pWithList = list_delete_cell(pWithList, def_lc, prev_lc);
					((AlterPartitionCmd *) pStoreAttr)->arg1 =
						(Node *) pWithList;
					break;
				}
				prev_lc = def_lc;
			}					/* end foreach */

			if (bTablename)
				goto l_EveryLoopEnd;
		}

		everyCnt = make_const(pstate, makeInteger(1), -1);

		pRI_Start = (PartitionRangeItem *) pBSpec->partStart;
		pRI_End = (PartitionRangeItem *) pBSpec->partEnd;
		pRI_Every = (PartitionRangeItem *) pBSpec->partEvery;

		numCols = list_length(pRI_Every->partRangeVal);

		if ((numCols != list_length(pRI_Start->partRangeVal))
			|| (numCols != list_length(pRI_End->partRangeVal)))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("mismatch between EVERY, START and END "
							"in partition%s%s",
							namBuf,
							at_depth),
					 parser_errposition(pstate, pBSpec->location)));

		}

		/*
		 * XXX XXX: need to save prev value of every_num * (every_cnt * n3t)
		 * for first column only
		 */

		for (;;)				/* loop until exceed end */
		{
			ListCell   *coltype = list_head(coltypes);
			ListCell   *lclastval = list_head(lastval);
			List	   *curval = NIL;

			int			sqlRc = 0;

			lc_Start = list_head(pRI_Start->partRangeVal);
			lc_End = list_head(pRI_End->partRangeVal);
			lc_Every = list_head(pRI_Every->partRangeVal);
			colCnt = numCols;
			allNewCols = NIL;

			for (; lc_Start && lc_End && lc_Every;)		/* for all cols */
			{
				Node	   *n1 = lfirst(lc_Start);
				Node	   *n2 = lfirst(lc_End);
				Node	   *n3 = lfirst(lc_Every);
				TypeName   *type = lfirst(coltype);
				char	   *compare_op = (1 == colCnt) ? "<" : "<=";
				Node	   *n1t,
						   *n2t,
						   *n3t;
				Datum		res;
				Const	   *c;
				Const	   *newend;
				List	   *oprmul,
						   *oprplus,
						   *oprcompare,
						   *ltop;
				Oid			restypid;
				Type		typ;
				char	   *outputstr;
				int32		coltypmod;
				Oid			coltypid;

				typenameTypeIdAndMod(pstate, type, &coltypid, &coltypmod);

				oprmul = lappend(NIL, makeString("*"));
				oprplus = lappend(NIL, makeString("+"));
				oprcompare = lappend(NIL, makeString(compare_op));
				ltop = lappend(NIL, makeString("<"));

				n1t = transformExpr(pstate, n1,
									EXPR_KIND_PARTITION_EXPRESSION);
				n1t = coerce_type(NULL, n1t, exprType(n1t), coltypid,
								  coltypmod,
								  COERCION_EXPLICIT, COERCE_IMPLICIT_CAST,
								  -1);
				n1t = (Node *) flatten_partition_val(n1t, coltypid);

				n2t = transformExpr(pstate, n2,
									EXPR_KIND_PARTITION_EXPRESSION);
				n2t = coerce_type(NULL, n2t, exprType(n2t), coltypid,
								  coltypmod,
								  COERCION_EXPLICIT, COERCE_IMPLICIT_CAST,
								  -1);
				n2t = (Node *) flatten_partition_val(n2t, coltypid);

				n3t = transformExpr(pstate, n3,
									EXPR_KIND_PARTITION_EXPRESSION);
				n3t = (Node *) flatten_partition_val(n3t, exprType(n3t));

				Assert(IsA(n3t, Const));
				Assert(IsA(n2t, Const));
				Assert(IsA(n1t, Const));

				/* formula is n1t + (every_cnt * every_num * n3t) [<|<=] n2t */

				/* every_cnt * n3t */
				restypid = InvalidOid;
				res = eval_basic_opexpr(pstate, oprmul, (Node *) everyCnt, n3t,
										NULL, NULL, &restypid,
										exprLocation(n3));
				typ = typeidType(restypid);
				c = makeConst(restypid, -1, InvalidOid, typeLen(typ), res, false,
							  typeByVal(typ));
				ReleaseSysCache(typ);

				/*
				 * n1t + (...) NOTE: order is important because several useful
				 * operators, like date + interval, only have a built in
				 * function when the order is this way (the reverse is
				 * implemented using an SQL function which we cannot evaluate
				 * at the moment).
				 */
				restypid = exprType(n1t);
				res = eval_basic_opexpr(pstate, oprplus, n1t, (Node *) c,
										NULL, NULL,
										&restypid,
										exprLocation(n1));
				typ = typeidType(restypid);
				newend = makeConst(restypid, -1, InvalidOid, typeLen(typ), res, false,
								   typeByVal(typ));
				ReleaseSysCache(typ);

				/*----------
				 * Now we must detect a few conditions.
				 *
				 * We must reject every() parameters which do not significantly
				 * increment the starting value. For example:
				 *
				 * 1: start ('2008-01-01') end ('2010-01-01') every('0 days')
				 *
				 * We'll detect this case on the second iteration by observing
				 * that the current partition end is no greater than the
				 * previous.
				 *
				 * We must also consider:
				 *
				 * 2: start (1) end (10) every(0.5)
				 * 3: start (1) end (10) every(1.5)
				 *
				 * The problem here is that 1.5 and 2.5 will be rounded to 2 and
				 * 3 respectively. This means they look sane to the code but
				 * they aren't. So, we must test that:
				 *
				 *	 cast(1 + 0.5 as int) != (1 + 0.5)::numeric
				 *
				 * If we make it past this point, we still might see a current
				 * value less than the previous value. That would be caused by
				 * an overflow of the type which is undetected by the type
				 * itself. Most types detect overflow but time and timetz do
				 * not. So, if we're beyond the second iteration and detect and
				 * overflow, error out.
				 *----------
				 */
				/* do it on the first iteration */
				if (!lclastval)
				{
					List	   *opreq = lappend(NIL, makeString("="));
					Datum		uncast;
					Datum		iseq;
					Const	   *tmpconst;
					Oid			test_typid = InvalidOid;

					uncast = eval_basic_opexpr(pstate, oprplus, n1t,
											   (Node *) c, NULL, NULL,
											   &test_typid,
											   exprLocation(n1));

					typ = typeidType(test_typid);
					tmpconst = makeConst(test_typid, -1, InvalidOid, typeLen(typ), uncast,
										 false, typeByVal(typ));
					ReleaseSysCache(typ);

					iseq = eval_basic_opexpr(NULL, opreq, (Node *) tmpconst,
											 (Node *) newend, NULL, NULL,
											 NULL, -1);

					if (!DatumGetBool(iseq))
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("EVERY parameter produces ambiguous partition rule"),
								 parser_errposition(pstate,
													exprLocation(n3))));

				}

				if (lclastval)
				{
					Datum		res2;
					Oid			tmptyp = InvalidOid;

					/*
					 * now test for case 2 and 3 above: ensure that the cast
					 * value is equal to the uncast value.
					 */

					res2 = eval_basic_opexpr(pstate, ltop,
											 (Node *) lfirst(lclastval),
											 (Node *) newend,
											 NULL, NULL,
											 &tmptyp,
											 exprLocation(n3));

					if (!DatumGetBool(res2))
					{
						/*
						 * Second iteration: parameter hasn't increased the
						 * current end from the old end.
						 */
						if (do_every_param_test)
						{
							ereport(ERROR,
									(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
									 errmsg("EVERY parameter too small"),
									 parser_errposition(pstate,
														exprLocation(n3))));
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
									 parser_errposition(pstate,
														exprLocation(n2))));
						}
					}
				}

				curval = lappend(curval, newend);

				/* get the string for the calculated type */
				{
					Oid			ooutput;
					bool		isvarlena;
					FmgrInfo	finfo;

					getTypeOutputInfo(restypid, &ooutput, &isvarlena);
					fmgr_info(ooutput, &finfo);

					if (isvarlena)
						res = PointerGetDatum(PG_DETOAST_DATUM(res));

					outputstr = OutputFunctionCall(&finfo, res);
				}

				/* the comparison */
				restypid = InvalidOid;
				res = eval_basic_opexpr(pstate, oprcompare, (Node *) newend, n2t,
										NULL, NULL,
										&restypid,
										exprLocation(n2));

				/*
				 * XXX XXX: also check stop flag. and free up prev if stop is
				 * true or current > end.
				 */

				if (!DatumGetBool(res))
				{
					if (outputstr)
						pfree(outputstr);

					sqlRc = 0;
					break;
				}
				else
				{
					allNewCols = lappend(allNewCols, pstrdup(outputstr));

					if (outputstr)
						pfree(outputstr);

					sqlRc = 1;
				}
				lc_Start = lnext(lc_Start);
				lc_End = lnext(lc_End);
				lc_Every = lnext(lc_Every);
				coltype = lnext(coltype);
				if (lclastval)
					lclastval = lnext(lclastval);

				colCnt--;
			}					/* end for all cols */

			if (lc_Start || lc_End || lc_Every)
			{					/* allNewCols is incomplete - don't use */
				if (allNewCols)
					list_free_deep(allNewCols);
			}
			else
			{
				allNewPartns = lappend(allNewPartns, allNewCols);
			}

			if (!sqlRc)
				break;

			everyCnt->constvalue++;
			/* if we have lastval set, the every test will have been done */

			if (lastval)
				do_every_param_test = false;
			lastval = curval;
		}						/* end loop until exceed end */

l_EveryLoopEnd:

		if (allNewPartns)
			Assert(pBSpec);		/* check for default partitions... */

		if (pBSpec)
			pBSpec->everyGenList = allNewPartns;

l_next_iteration:
		lc_prev = lc;
		lc = lnext(lc);
	}							/* end foreach */

	pSpec->enc_clauses = stenc;

	return 1;
}	/* end partition_range_every */


/*
 * Basic partition validation:
 * Perform basic error checking on boundary specifications.
 */
static void
validate_range_partition(partValidationState *vstate)
{
	ParseState *pstate = vstate->cxt->pstate;
	bool		bAppendRange = false;
	PartitionBoundSpec *prevBSpec = NULL;
	PartitionBoundSpec *spec = (PartitionBoundSpec *) vstate->spec;

	vstate->spec = (Node *) spec;

	if (spec)
	{
		/* XXX: create a type2name function */
		char	   *specTName = "LIST";

		if (IsA(spec, PartitionValuesSpec))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of %s boundary "
							"specification in partition clause",
							specTName),
			/* MPP-4249: use value spec location if have one */
					 parser_errposition(pstate,
								 ((PartitionValuesSpec *) spec)->location)));
		}
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("missing boundary specification"),
			   parser_errposition(pstate, vstate->pElem->location)));

	}

	{
		/*
		 * if the previous partition was named, and current is not, and the
		 * prev and current use single, complementary START/END specs, then
		 * complain.
		 */

		/*
		 * must have a valid RANGE pBSpec now or else would have error'd
		 * out...
		 */
		int			currStartEnd = 0;

		if (spec->partStart)
			currStartEnd += 1;
		if (spec->partEnd)
			currStartEnd += 2;

		/*
		 * Note: for first loop, prevStartEnd = 0, so this test is ok
		 */

		if (((vstate->prevHadName && !(vstate->pElem->partName))
			 || (!vstate->prevHadName && vstate->pElem->partName))
			&&
			(((vstate->prevStartEnd == 1)
			  && (currStartEnd == 2))
			 ||
			 ((vstate->prevStartEnd == 2)
			  && (currStartEnd == 1)))
			)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of mixed named and "
							"unnamed RANGE boundary "
							"specifications%s",
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}

		vstate->prevStartEnd = currStartEnd;
	}

	if (vstate->pElem->partName != NULL)
		vstate->prevHadName = true;
	else
		vstate->prevHadName = false;

	if (spec->partStart)
		PartitionRangeItemIsValid(pstate, (PartitionRangeItem *) spec->partStart);
	if (spec->partEnd)
		PartitionRangeItemIsValid(pstate, (PartitionRangeItem *) spec->partEnd);

	/*
	 * Fixup boundaries for previous partition ending if necessary
	 */
	if (!vstate->prevElem)
	{
		bAppendRange = true;
		goto L_setprevElem;
	}

	prevBSpec = (PartitionBoundSpec *) vstate->prevElem->boundSpec;
	/* XXX XXX: can check for overlap here too */

	if (!prevBSpec)
	{
		/*
		 * XXX XXX: if the previous partition declaration does not have a
		 * boundary spec then its end needs to be the start of the current
		 */

		bAppendRange = true;
		goto L_setprevElem;
	}

	if (spec->partStart)
	{
		if (!prevBSpec->partEnd)
		{
			PartitionRangeItem *pRI = (PartitionRangeItem *) (spec->partStart);
			PartitionRangeItem *prevRI = makeNode(PartitionRangeItem);

			if (prevBSpec->partStart)
			{
				int			compareRc = 0;
				PartitionRangeItem *pRI1 =
				(PartitionRangeItem *) spec->partStart;
				PartitionRangeItem *pRI2 =
				(PartitionRangeItem *) prevBSpec->partStart;


				compareRc = partition_range_compare(vstate->cxt, vstate->stmt,
													vstate->pBy,
													vstate->at_depth,
													vstate->partNumber,
													"<", pRI2, pRI1);

				if (1 != compareRc)
				{
					/* XXX: better message */
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("START of partition%s less "
									"than START of previous%s",
									vstate->namBuf,
									vstate->at_depth),
							 parser_errposition(pstate,
												spec->location)));

				}

			}

			prevRI->location = pRI->location;

			prevRI->partRangeVal = list_copy(pRI->partRangeVal);

			/* invert sense of inclusiveness */
			prevRI->partedge =
				(PART_EDGE_INCLUSIVE == pRI->partedge) ?
				PART_EDGE_EXCLUSIVE : PART_EDGE_INCLUSIVE;

			prevBSpec->partEnd = (Node *) prevRI;

			/* don't need to check if range overlaps */
			bAppendRange = true;

		}
		else if (0)				/* XXX XXX XXX XXX */
		{
			/* else if overlap complain */
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("start of partition%s overlaps previous%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}

	}							/* end if current partStart */
	else	/* no start, so use END of previous */
	{
		if (!prevBSpec->partEnd)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot derive starting value of "
							"partition%s based upon ending of "
							"previous%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}
		else	/* build ri for current */
		{
			PartitionRangeItem *pRI =
			makeNode(PartitionRangeItem);
			PartitionRangeItem *prevRI =
			(PartitionRangeItem *)
			(prevBSpec->partEnd);

			pRI->location =
				prevRI->location;

			pRI->partRangeVal =
				list_copy(prevRI->partRangeVal);

			/* invert sense of inclusiveness */
			pRI->partedge =
				(PART_EDGE_INCLUSIVE == prevRI->partedge) ?
				PART_EDGE_EXCLUSIVE : PART_EDGE_INCLUSIVE;

			spec->partStart = (Node *) pRI;

			/* don't need to check if range overlaps */
			bAppendRange = true;
		}						/* end build ri for current */
	}							/* end use END of previous */

L_setprevElem:

	/*
	 * check for overlap, then add new partitions to sorted list
	 */

	if (spec->partStart &&
		spec->partEnd)
	{
		int			compareRc = 0;
		PartitionRangeItem *pRI1 =
		(PartitionRangeItem *) spec->partStart;
		PartitionRangeItem *pRI2 =
		(PartitionRangeItem *) spec->partEnd;

		PartitionRangeItemIsValid(pstate, (PartitionRangeItem *) spec->partEnd);
		compareRc =
			partition_range_compare(vstate->cxt, vstate->stmt,
									vstate->pBy,
									vstate->at_depth,
									vstate->partNumber,
									">",
									pRI1,
									pRI2);

		if (0 != compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("START greater than END for partition%s%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}
		compareRc =
			partition_range_compare(vstate->cxt, vstate->stmt,
									vstate->pBy,
									vstate->at_depth,
									vstate->partNumber,
									"=",
									pRI1,
									pRI2);

		if (0 != compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("START equal to END for partition%s%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}

	}

	if (NIL == vstate->allRangeVals)
		bAppendRange = true;

	/* check previous first */
	if (!bAppendRange &&
		(spec->partStart &&
		 prevBSpec &&
		 prevBSpec->partEnd))
	{
		/* compare to last */
		int			rc = 0;
		PartitionRangeItem *pRI1 =
		(PartitionRangeItem *) spec->partStart;
		PartitionRangeItem *pRI2 =
		(PartitionRangeItem *) prevBSpec->partEnd;

		rc = partition_range_compare(vstate->cxt, vstate->stmt,
									 vstate->pBy,
									 vstate->at_depth,
									 vstate->partNumber,
									 "=",
									 pRI1,
									 pRI2);

		if ((pRI2->partedge == PART_EDGE_INCLUSIVE) &&
			(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
			(1 == rc))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("starting value of partition%s "
							"overlaps previous range%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}

		/*
		 * If values are equal, but not inclusive to both, then append is
		 * possible
		 */
		if (1 != rc)
			rc = partition_range_compare(vstate->cxt, vstate->stmt,
										 vstate->pBy,
										 vstate->at_depth,
										 vstate->partNumber,
										 ">",
										 pRI1,
										 pRI2);

		if (1 == rc)
			bAppendRange = 1;

		if (rc != 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("starting value of partition%s "
							"overlaps previous range%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));
		}

	}							/* end compare to last */

	if (bAppendRange)
	{
		vstate->allRangeVals = lappend(vstate->allRangeVals, spec);
	}
	else
	{							/* find position for current range */
		ListCell   *lc_all = list_head(vstate->allRangeVals);
		ListCell   *lc_allPrev = NULL;
		int			compareRc = 0;

		/* set up pieces of error messages */
		char	   *currPartRI = "Ending";
		char	   *otherPartPos = "next";

		/*
		 * linear search sorted list of range specs:
		 *
		 * While the current start key is >= loop val start key advance.
		 *
		 * If current start key < loop val start key, then see if curr start
		 * key < *previous* loop val end key, ie falls in previous range.
		 *
		 * If so, error out, else splice current range in after previous.
		 */

		for (; lc_all; lc_all = lnext(lc_all))	/* for lc_all */
		{
			PartitionBoundSpec *lcBSpec =
			(PartitionBoundSpec *) lfirst(lc_all);
			PartitionRangeItem *pRI1 =
			(PartitionRangeItem *) spec->partStart;
			PartitionRangeItem *pRI2 =
			(PartitionRangeItem *) lcBSpec->partStart;

			Assert(spec->partStart);

			if (lcBSpec->partStart)
			{
				compareRc =
					partition_range_compare(vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"=",
											pRI1,
											pRI2);

				if (-1 == compareRc)
					break;

				if ((pRI2->partedge == PART_EDGE_INCLUSIVE) &&
					(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
					(1 == compareRc))
				{
					currPartRI = "Starting";
					otherPartPos = "previous";
					compareRc = -2;
					break;
				}

				compareRc =
					partition_range_compare(vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"<",
											pRI1,
											pRI2);

			}
			else

				/*
				 * if first range spec has no start (ie start=MINVALUE) then
				 * current start must be after it
				 */
				compareRc = 0;	/* current > MINVALUE */

			if (-1 == compareRc)
				break;

			if (1 == compareRc) /* if curr less than loop val */
			{
				/*
				 * curr start is less than loop val, so check that curr end is
				 * less than loop val start (including equality case)
				 */

				pRI1 = (PartitionRangeItem *) spec->partEnd;

				if (!pRI1)
				{
					/*
					 * if current start is less than loop val start but
					 * current end is unterminated (ie ending=MAXVALUE) then
					 * it must overlap
					 */
					currPartRI = "Ending";
					otherPartPos = "next";
					compareRc = -2;
					break;
				}

				compareRc =
					partition_range_compare(vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"=",
											pRI2,
											pRI1);

				if (-1 == compareRc)
					break;

				if (
					(pRI2->partedge == PART_EDGE_INCLUSIVE) &&
					(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
					(1 == compareRc))
				{
					currPartRI = "Ending";
					otherPartPos = "next";
					compareRc = -2;
					break;
				}

				compareRc = partition_range_compare(vstate->cxt, vstate->stmt,
													vstate->pBy,
													vstate->at_depth,
													vstate->partNumber,
													"<",
													pRI2,
													pRI1);

				if (-1 == compareRc)
					break;

				if (1 == compareRc)
				{
					currPartRI = "Ending";
					otherPartPos = "next";
					compareRc = -2;
					break;
				}

				/*
				 * if current is less than loop val and no previous, then
				 * append vstate->allRangeVals to the current, instead of vice
				 * versa
				 */
				if (!lc_allPrev)
					break;

				lcBSpec =
					(PartitionBoundSpec *) lfirst(lc_allPrev);

				pRI2 =
					(PartitionRangeItem *) lcBSpec->partEnd;

				compareRc =
					partition_range_compare(vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"=",
											pRI1,
											pRI2);

				if (-1 == compareRc)
					break;

				if (
					(pRI2->partedge == PART_EDGE_INCLUSIVE) &&
					(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
					(1 == compareRc))
				{
					currPartRI = "Starting";
					otherPartPos = "previous";
					compareRc = -2;
					break;
				}

				compareRc =
					partition_range_compare(vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"<",
											pRI1,
											pRI2);

				if (-1 == compareRc)
					break;

				if (1 == compareRc)
				{
					currPartRI = "Starting";
					otherPartPos = "previous";
					compareRc = -2;
					break;
				}
			}					/* end if curr less than loop val */

			lc_allPrev = lc_all;
		}						/* end for lc_all */

		if (-1 == compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid range comparison for partition%s%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));

		}

		if (-2 == compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("%s value of partition%s overlaps %s range%s",
							currPartRI,
							vstate->namBuf,
							otherPartPos,
							vstate->at_depth),
					 parser_errposition(pstate, spec->location)));
		}

		if (lc_allPrev)
		{
			/* append cell returns new cell, not list */
			lappend_cell(vstate->allRangeVals,
						 lc_allPrev,
						 spec);
		}
		else	/* no previous, so current is start */
			vstate->allRangeVals = list_concat(list_make1(spec),
											   vstate->allRangeVals);
	}							/* end find position for current range */
	vstate->prevElem = vstate->pElem;
}


/*
 * Evaluate a basic operator expression from a partitioning specification.
 * The expression will only be an op expr but the sides might contain
 * a coercion function. The underlying value will be a simple constant,
 * however.
 *
 * If restypid is non-NULL and *restypid is set to InvalidOid, we tell the
 * caller what the return type of the operator is. If it is anything but
 * InvalidOid, coerce the operation's result to that type.
 */
static Datum
eval_basic_opexpr(ParseState *pstate, List *oprname, Node *leftarg,
				  Node *rightarg, bool *typbyval, int16 *typlen,
				  Oid *restypid, int location)
{
	Datum		res = 0;
	Datum		lhs = 0;
	Datum		rhs = 0;
	OpExpr	   *opexpr;
	bool		byval;
	int16		len;
	bool		need_typ_info = (PointerIsValid(restypid) && *restypid == InvalidOid);
	bool		isnull;
	Oid			oprcode;

	opexpr = (OpExpr *) make_op(pstate, oprname, leftarg, rightarg, location);

	oprcode = get_opcode(opexpr->opno);
	if (oprcode == InvalidOid)			/* should not fail */
		elog(ERROR, "cache lookup failed for operator %u", opexpr->opno);
	opexpr->opfuncid = oprcode;

	assign_expr_collations(pstate, (Node *) opexpr);

	lhs = partition_arg_get_val((Node *) linitial(opexpr->args), &isnull);
	if (!isnull)
	{
		rhs = partition_arg_get_val((Node *) lsecond(opexpr->args), &isnull);
		if (!isnull)
			res = OidFunctionCall2Coll(opexpr->opfuncid,
									   opexpr->inputcollid,
									   lhs, rhs);
	}

	/* If the caller supplied a target result type, coerce if necesssary */
	if (PointerIsValid(restypid))
	{
		if (OidIsValid(*restypid))
		{
			if (*restypid != opexpr->opresulttype)
			{
				Expr	   *e;
				Type		typ = typeidType(opexpr->opresulttype);
				int32		typmod;
				Const	   *c;

				c = makeConst(opexpr->opresulttype,
							  -1,
							  opexpr->opcollid,
							  typeLen(typ), res,
							  isnull, typeByVal(typ));
				ReleaseSysCache(typ);

				typ = typeidType(*restypid);
				typmod = ((Form_pg_type) GETSTRUCT(typ))->typtypmod;
				ReleaseSysCache(typ);

				e = (Expr *) coerce_type(NULL, (Node *) c, opexpr->opresulttype,
										 *restypid, typmod,
										 COERCION_EXPLICIT,
										 COERCE_IMPLICIT_CAST,
										 -1);

				assign_expr_collations(pstate, (Node *) e);

				res = partition_arg_get_val((Node *) e, &isnull);
			}
		}
		else
		{
			*restypid = opexpr->opresulttype;
		}
	}
	else
	{
		return res;
	}

	if (need_typ_info || !PointerIsValid(typbyval))
	{
		Type		typ = typeidType(*restypid);

		byval = typeByVal(typ);
		len = typeLen(typ);

		if (PointerIsValid(typbyval))
		{
			Assert(PointerIsValid(typlen));
			*typbyval = byval;
			*typlen = len;
		}

		ReleaseSysCache(typ);
	}
	else
	{
		byval = *typbyval;
		len = *typlen;
	}

	res = datumCopy(res, byval, len);
	return res;
}
static void
validate_list_partition(partValidationState *vstate)
{
	ParseState *pstate = vstate->cxt->pstate;
	PartitionValuesSpec *spec;
	Node	   *n = vstate->spec;
	ListCell   *lc;
	List	   *coltypes = NIL;

	/* just recreate attnum references */
	foreach(lc, vstate->pBy->keys)
	{
		ListCell   *llc2;
		char	   *colname = strVal(lfirst(lc));
		bool		found = false;
		TypeName   *typname = NULL;

		foreach(llc2, vstate->cxt->columns)
		{
			ColumnDef  *column = lfirst(llc2);

			if (strcmp(column->colname, colname) == 0)
			{
				found = true;
				if (!OidIsValid(column->typeName->typeOid))
				{
					typenameTypeIdAndMod(pstate, column->typeName,
										 &column->typeName->typeOid,
										 &column->typeName->typemod);
				}
				typname = column->typeName;
				break;
			}
		}
		Assert(found);
		coltypes = lappend(coltypes, typname);
	}

	if (!PointerIsValid(n))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("missing boundary specification in "
						"partition%s of type LIST%s",
						vstate->namBuf,
						vstate->at_depth),
			   parser_errposition(pstate, vstate->pElem->location)));

	}

	if (!IsA(n, PartitionValuesSpec))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("invalid boundary specification for LIST partition"),
			   parser_errposition(pstate, vstate->pElem->location)));


	}

	spec = (PartitionValuesSpec *) n;
	if (spec->partValues)
	{
		ListCell   *lc;
		List	   *newvals = NIL;

		foreach(lc, spec->partValues)
		{
			ListCell   *lc_val = NULL;
			List	   *vals = lfirst(lc);
			List	   *tvals = NIL;	/* transformed clause */
			int			nvals;
			int			nparts = list_length(vstate->pBy->keys);
			ListCell   *llc2;

			nvals = list_length(vals);

			/* Number of values should be same as specified partition keys */
			if (nvals != nparts)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					  errmsg("partition key has %i column%s but %i column%s "
							 "specified in VALUES clause",
							 list_length(vstate->pBy->keys),
							 list_length(vstate->pBy->keys) ? "s" : "",
							 nvals,
							 nvals ? "s" : ""),
						 parser_errposition(pstate, spec->location)));
			}

			/*
			 * Transform expressions
			 */
			llc2 = list_head(coltypes);
			foreach(lc_val, vals)
			{
				Node	   *node = transformExpr(pstate,
												 (Node *) lfirst(lc_val),
												 EXPR_KIND_PARTITION_EXPRESSION);
				TypeName   *type = lfirst(llc2);
				int32		typmod;
				Oid			typid;

				typenameTypeIdAndMod(pstate, type, &typid, &typmod);
				node = coerce_partition_value(pstate,
											  node, typid, typmod,
											  PARTTYP_LIST);

				tvals = lappend(tvals, node);

				if (lnext(llc2))
					llc2 = lnext(llc2);
				else
					llc2 = list_head(coltypes); /* circular */
			}
			Assert(list_length(tvals) == nvals);

			/*
			 * Check for duplicate keys
			 */
			foreach(lc_val, vstate->allListVals)		/* dups across all specs */
			{
				List	   *already = lfirst(lc_val);
				ListCell   *lc2;

				foreach(lc2, already)
				{
					List	   *item = lfirst(lc2);

					Assert(IsA(item, List) &&list_length(item) == nvals);

					/*
					 * Re MPP-17814 The lists tvals and item each represent a
					 * tuple of nvals attribute values.  If they are equal,
					 * the new value (tvals) is in vstate->allListVals, i.e.,
					 * tvals has been seen before.
					 */
					if (equal(tvals, item))
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("duplicate VALUES "
										"in partition%s%s",
										vstate->namBuf,
										vstate->at_depth),
						parser_errposition(pstate, spec->location)));
					}
				}
			}
			newvals = list_append_unique(newvals, tvals);
		}
		vstate->allListVals = lappend(vstate->allListVals, newvals);
		spec->partValues = newvals;
	}							/* end if spec partvalues */
}	/* end validate_list_partition */

static List *
transformPartitionStorageEncodingClauses(List *enc)
{
	ListCell   *lc;
	List	   *out = NIL;

	/*
	 * Test that directives at different levels of subpartitioning do not
	 * conflict. A conflict would be the case where a directive appears for
	 * the same column but different parameters have been supplied.
	 */
	foreach(lc, enc)
	{
		ListCell   *in;
		ColumnReferenceStorageDirective *a = lfirst(lc);
		bool		add = true;

		Insist(IsA(a, ColumnReferenceStorageDirective));

		foreach(in, out)
		{
			ColumnReferenceStorageDirective *b = lfirst(in);

			Insist(IsA(b, ColumnReferenceStorageDirective));

			if (lc == in)
				continue;

			if ((a->deflt && b->deflt) ||
				(!a->deflt && !b->deflt && strcmp(a->column, b->column) == 0))
			{
				if (!equal(a->encoding, b->encoding))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("conflicting ENCODING clauses for column "
									"\"%s\"", a->column ? a->column : "DEFAULT")));

				/*
				 * We found an identical directive on the same column. You'd
				 * think we should blow up but this is caused by recursive
				 * expansion of partitions. Anyway, only add it once.
				 */
				add = false;
			}
		}

		if (add)
			out = lappend(out, a);
	}

	/* validate and transform each encoding clauses */
	foreach(lc, out)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		c->encoding = transformStorageEncodingClause(c->encoding);
	}

	return out;

}

static void
split_encoding_clauses(List *encs, List **non_def,
					   ColumnReferenceStorageDirective ** def)
{
	ListCell   *lc;

	foreach(lc, encs)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		Insist(IsA(c, ColumnReferenceStorageDirective));

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

static void
merge_partition_encoding(ParseState *pstate, PartitionElem *elem, List *penc)
{
	List	   *elem_nondefs = NIL;
	List	   *part_nondefs = NIL;
	ColumnReferenceStorageDirective *elem_def = NULL;
	ColumnReferenceStorageDirective *part_def = NULL;
	ListCell   *lc;
	AlterPartitionCmd *pc;

	/*
	 * First of all, we shouldn't proceed if this partition isn't AOCO
	 */

	/*
	 * Yes, I am as surprised as you are that this is how we represent the
	 * WITH clause here.
	 */
	pc = (AlterPartitionCmd *) elem->storeAttr;
	if (pc && !is_aocs((List *) pc->arg1))
	{
		if (elem->colencs)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("ENCODING clause only supported with "
							"column oriented partitions"),
					 parser_errposition(pstate, elem->location)));
		else
			return;				/* nothing more to do */
	}

	/*
	 * If the specific partition has no specific column encoding, just set it
	 * to the partition level default and we're done.
	 */
	if (!elem->colencs)
	{
		elem->colencs = penc;
		return;
	}

	/*
	 * Fixup the actual column encoding clauses for this specific partition
	 * element.
	 *
	 * Rules:
	 *
	 * 1. If an element level clause mentions a specific column, do not
	 * override it. 2. Clauses at the partition configuration level which
	 * mention a column not already mentioned at the element level, are
	 * applied to the element. 3. If an element level default clause exists,
	 * we're done. 4. If a partition configuration level default clause
	 * exists, apply it to the element level. 5. We're done.
	 */

	/* Split specific clauses and default clauses from both our lists */
	split_encoding_clauses(elem->colencs, &elem_nondefs, &elem_def);
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

			if ((pd->deflt && ed->deflt) ||
				(!pd->deflt && !ed->deflt && strcmp(pd->column, ed->column) == 0))
			{
				found = true;
				break;
			}
		}

		if (!found)
			elem->colencs = lappend(elem->colencs, pd);
	}

	if (elem_def)
		return;

	if (part_def)
		elem->colencs = lappend(elem->colencs, part_def);
}

void
PartitionRangeItemIsValid(ParseState *pstate, PartitionRangeItem *pri)
{
	ListCell   *lc;
	range_partition_ctx ctx;

	if (!pri)
		return;

	ctx.pstate = pstate;
	ctx.location = pri->location;

	foreach(lc, pri->partRangeVal)
	{
		range_partition_walker(lfirst(lc), &ctx);
	}
}

static bool
range_partition_walker(Node *node, void *context)
{
	range_partition_ctx *ctx = (range_partition_ctx *) context;

	if (node == NULL)
		return false;
	else if (IsA(node, Const))
	{
		Const	   *c = (Const *) node;

		if (c->constisnull)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("cannot use NULL with range partition specification"),
					 parser_errposition(ctx->pstate, ctx->location)));
		return false;
	}
	else if (IsA(node, A_Const))
	{
		A_Const    *c = (A_Const *) node;

		if (IsA(&c->val, Null))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("cannot use NULL with range partition specification"),
					 parser_errposition(ctx->pstate, ctx->location)));
		return false;
	}
	return raw_expression_tree_walker(node, range_partition_walker, ctx);
}

Node *
coerce_partition_value(ParseState *pstate, Node *node, Oid typid, int32 typmod,
					   PartitionByType partype)
{
	Oid			typcollation;
	Node	   *out;

	/* Create a coercion expression to the target type */
	out = coerce_to_target_type(NULL, node, exprType(node),
								typid, typmod,
								COERCION_EXPLICIT,
								COERCE_IMPLICIT_CAST,
								-1);

	/* MPP-3626: better error message */
	if (!out)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot coerce partition parameter to column type \"%s\"",
						format_type_be(typid)),
				 parser_errposition(pstate, exprLocation(node))));

	/*
	 * GPDB_91_MERGE_FIXME: For now, partition specifications always use
	 * default collation. We're not very consistent about that though, I think.
	 */
	typcollation = get_typcollation(typid);

	/*
	 * And then evaluate it. evaluate_expr calls possible cast function, and
	 * returns a Const. (the check for that below is just for paranoia)
	 */
	out = (Node *) evaluate_expr((Expr *) out,
								 typid,
								 typmod,
								 typcollation);
	if (!IsA(out, Const))
		elog(ERROR, "partition parameter is not constant");

	return out;
}
