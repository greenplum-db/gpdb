/*-------------------------------------------------------------------------
 *
 * parse_utilcmd.c
 *	  Perform parse analysis work for various utility commands
 *
 * Formerly we did this work during parse_analyze() in analyze.c.  However
 * that is fairly unsafe in the presence of querytree caching, since any
 * database state that we depend on in making the transformations might be
 * obsolete by the time the utility command is executed; and utility commands
 * have no infrastructure for holding locks or rechecking plan validity.
 * Hence these functions are now called at the start of execution of their
 * respective utility commands.
 *
 * NOTE: in general we must avoid scribbling on the passed-in raw parse
 * tree, since it might be in a plan cache.  The simplest solution is
 * a quick copyObject() call before manipulating the query tree.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	$PostgreSQL: pgsql/src/backend/parser/parse_utilcmd.c,v 2.21 2009/06/11 14:49:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/reloptions.h"
#include "catalog/pg_compression.h"
#include "catalog/pg_constraint.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_encoding.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/analyze.h"
#include "parser/gramparse.h"
#include "parser/parse_clause.h"
#include "parser/parse_expr.h"
#include "parser/parse_partition.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "rewrite/rewriteManip.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/syscache.h"

#include "cdb/cdbhash.h"
#include "cdb/cdbpartition.h"
#include "cdb/partitionselection.h"
#include "cdb/cdbvars.h"


/* State shared by transformCreateSchemaStmt and its subroutines */
typedef struct
{
	const char *stmtType;		/* "CREATE SCHEMA" or "ALTER SCHEMA" */
	char	   *schemaname;		/* name of schema */
	char	   *authid;			/* owner of schema */
	List	   *sequences;		/* CREATE SEQUENCE items */
	List	   *tables;			/* CREATE TABLE items */
	List	   *views;			/* CREATE VIEW items */
	List	   *indexes;		/* CREATE INDEX items */
	List	   *triggers;		/* CREATE TRIGGER items */
	List	   *grants;			/* GRANT items */
} CreateSchemaStmtContext;


static void transformColumnDefinition(ParseState *pstate,
						  CreateStmtContext *cxt,
						  ColumnDef *column);
static void transformTableConstraint(ParseState *pstate,
						 CreateStmtContext *cxt,
						 Constraint *constraint);
static IndexStmt *generateClonedIndexStmt(CreateStmtContext *cxt,
						Relation source_idx,
						const AttrNumber *attmap, int attmap_length);
static List *get_opclass(Oid opclass, Oid actual_datatype);
static void transformIndexConstraints(ParseState *pstate,
						  CreateStmtContext *cxt, bool mayDefer);
static IndexStmt *transformIndexConstraint(Constraint *constraint,
						 CreateStmtContext *cxt);
static void transformFKConstraints(ParseState *pstate,
					   CreateStmtContext *cxt,
					   bool skipValidation,
					   bool isAddConstraint);
static void transformConstraintAttrs(List *constraintList);
static void transformColumnType(ParseState *pstate, ColumnDef *column);
static void setSchemaName(char *context_schema, char **stmt_schema_name);

static List *getLikeDistributionPolicy(InhRelation *e);
static bool co_explicitly_disabled(List *opts);
static void transformDistributedBy(ParseState *pstate, CreateStmtContext *cxt,
					   List *distributedBy, GpPolicy **policyp,
					   List *likeDistributedBy,
					   bool bQuiet);
static List *transformAttributeEncoding(List *stenc, CreateStmt *stmt,
										CreateStmtContext *cxt);
static bool encodings_overlap(List *a, List *b, bool test_conflicts);

static AlterTableCmd *transformAlterTable_all_PartitionStmt(ParseState *pstate,
									  AlterTableStmt *stmt,
									  CreateStmtContext *pCxt,
									  AlterTableCmd *cmd);
static List *transformIndexStmt_recurse(IndexStmt *stmt, const char *queryString,
						   ParseState *masterpstate, bool recurseToPartitions);

/*
 * transformCreateStmt -
 *	  parse analysis for CREATE TABLE
 *
 * Returns a List of utility commands to be done in sequence.  One of these
 * will be the transformed CreateStmt, but there may be additional actions
 * to be done before and after the actual DefineRelation() call.
 *
 * SQL92 allows constraints to be scattered all over, so thumb through
 * the columns and collect all constraints into one place.
 * If there are any implied indices (e.g. UNIQUE or PRIMARY KEY)
 * then expand those into multiple IndexStmt blocks.
 *	  - thomas 1997-12-02
 */
List *
transformCreateStmt(CreateStmt *stmt, const char *queryString, bool createPartition)
{
	ParseState *pstate;
	CreateStmtContext cxt;
	List	   *result;
	List	   *save_alist;
	ListCell   *elements;
	List	   *likeDistributedBy = NIL;
	bool		bQuiet = false;		/* shut up transformDistributedBy messages */
	List	   *stenc = NIL;		/* column reference storage encoding clauses */

 	/*
	 * We don't normally care much about the memory consumption of parsing,
	 * because any memory leaked is leaked into MessageContext which is
	 * reset between each command. But if a table is heavily partitioned,
	 * the CREATE TABLE statement can be expanded into hundreds or even
	 * thousands of CreateStmts, so the leaks start to add up. To reduce
	 * the memory consumption, we use a temporary memory context that's
	 * destroyed after processing the CreateStmt for some parts of the
	 * processing.
	 */
	cxt.tempCtx =
		AllocSetContextCreate(CurrentMemoryContext,
							  "CreateStmt analyze context",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * We must not scribble on the passed-in CreateStmt, so copy it.  (This is
	 * overkill, but easy.)
	 */
	stmt = (CreateStmt *) copyObject(stmt);

	/*
	 * If the target relation name isn't schema-qualified, make it so.  This
	 * prevents some corner cases in which added-on rewritten commands might
	 * think they should apply to other relations that have the same name and
	 * are earlier in the search path.	"istemp" is equivalent to a
	 * specification of pg_temp, so no need for anything extra in that case.
	 */
	if (stmt->relation->schemaname == NULL && !stmt->relation->istemp)
	{
		Oid			namespaceid = RangeVarGetCreationNamespace(stmt->relation);

		stmt->relation->schemaname = get_namespace_name(namespaceid);
	}

	/* Set up pstate */
	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	cxt.stmtType = "CREATE TABLE";
	cxt.relation = stmt->relation;
	cxt.rel = NULL;
	cxt.inhRelations = stmt->inhRelations;
	cxt.isalter = false;
	cxt.iscreatepart = createPartition;
	cxt.issplitpart = stmt->is_split_part;
	cxt.columns = NIL;
	cxt.ckconstraints = NIL;
	cxt.fkconstraints = NIL;
	cxt.ixconstraints = NIL;
	cxt.inh_indexes = NIL;
	cxt.blist = NIL;
	cxt.alist = NIL;
	cxt.dlist = NIL; /* for deferred analysis requiring the created table */
	cxt.pkey = NULL;
	cxt.hasoids = interpretOidsOption(stmt->options);

	stmt->policy = NULL;

	/* Disallow inheritance in combination with partitioning. */
	if (stmt->inhRelations && (stmt->partitionBy || stmt->is_part_child))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot mix inheritance with partitioning")));

	/* Disallow inheritance for CO table */
	if (stmt->inhRelations && is_aocs(stmt->options))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("INHERITS clause cannot be used with column oriented tables")));

	/* Only on top-most partitioned tables. */
	if (stmt->partitionBy && !stmt->is_part_child)
		fixCreateStmtForPartitionedTable(stmt);

	/*
	 * Run through each primary element in the table creation clause. Separate
	 * column defs from constraints, and do preliminary analysis.
	 */
	foreach(elements, stmt->tableElts)
	{
		Node	   *element = lfirst(elements);

		switch (nodeTag(element))
		{
			case T_ColumnDef:
				transformColumnDefinition(pstate, &cxt,
										  (ColumnDef *) element);
				break;

			case T_Constraint:
				transformTableConstraint(pstate, &cxt,
										 (Constraint *) element);
				break;

			case T_FkConstraint:
				/* No pre-transformation needed */
				cxt.fkconstraints = lappend(cxt.fkconstraints, element);
				break;

			case T_InhRelation:
			{
				bool            isBeginning = (cxt.columns == NIL);

				transformInhRelation(pstate, &cxt,
									 (InhRelation *) element, false);

				if (Gp_role == GP_ROLE_DISPATCH && isBeginning &&
					stmt->distributedBy == NIL &&
					stmt->inhRelations == NIL &&
					stmt->policy == NULL)
				{
					likeDistributedBy = getLikeDistributionPolicy((InhRelation *) element);
				}
				break;
			}

			case T_ColumnReferenceStorageDirective:
				/* processed below in transformAttributeEncoding() */
				stenc = lappend(stenc, element);
				break;

			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(element));
				break;
		}
	}

	/*
	 * transformIndexConstraints wants cxt.alist to contain only index
	 * statements, so transfer anything we already have into save_alist.
	 */
	save_alist = cxt.alist;
	cxt.alist = NIL;

	Assert(stmt->constraints == NIL);

	/*
	 * Postprocess constraints that give rise to index definitions.
	 */
	transformIndexConstraints(pstate, &cxt, stmt->is_add_part || stmt->is_split_part);

	/*
	 * Carry any deferred analysis statements forward.  Added for MPP-13750
	 * but should also apply to the similar case involving simple inheritance.
	 */
	if (cxt.dlist)
	{
		stmt->deferredStmts = list_concat(stmt->deferredStmts, cxt.dlist);
		cxt.dlist = NIL;
	}

	/*
	 * Postprocess foreign-key constraints.
	 * But don't cascade FK constraints to parts, yet.
	 */
	if (!stmt->is_part_child)
		transformFKConstraints(pstate, &cxt, true, false);

	/*-----------
	 * Analyze attribute encoding clauses.
	 *
	 * Partitioning configurations may have things like:
	 *
	 * CREATE TABLE ...
	 *  ( a int ENCODING (...))
	 * WITH (appendonly=true, orientation=column)
	 * PARTITION BY ...
	 * (PARTITION ... WITH (appendonly=false));
	 *
	 * We don't want to throw an error when we try to apply the ENCODING clause
	 * to the partition which the user wants to be non-AO. Just ignore it
	 * instead.
	 *-----------
	 */
	if (!is_aocs(stmt->options) && stmt->is_part_child)
	{
		if (co_explicitly_disabled(stmt->options) || !stenc)
			stmt->attr_encodings = NIL;
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("ENCODING clause only supported with column oriented partitioned tables")));
		}
	}
	else
		stmt->attr_encodings = transformAttributeEncoding(stenc, stmt, &cxt);

	/*
	 * Postprocess Greenplum Database distribution columns
	 */
	/* silence distro messages for partitions */
	if (stmt->is_part_child)
		bQuiet = true;
	else if (stmt->partitionBy)
	{
		PartitionBy *partitionBy = (PartitionBy *) stmt->partitionBy;

		/* be very quiet if set subpartn template */
		if (partitionBy->partQuiet == PART_VERBO_NOPARTNAME)
			bQuiet = true;
		/* quiet for partitions of depth > 0 */
		else if (partitionBy->partDepth != 0 &&
				 partitionBy->partQuiet != PART_VERBO_NORMAL)
			bQuiet = true;
	}
	transformDistributedBy(pstate, &cxt, stmt->distributedBy, &stmt->policy,
						   likeDistributedBy, bQuiet);

	/*
	 * Process table partitioning clause
	 */
	transformPartitionBy(pstate, &cxt, stmt, stmt->partitionBy, stmt->policy);

	/*
	 * Output results.
	 */
	stmt->tableElts = cxt.columns;
	stmt->constraints = cxt.ckconstraints;

	result = lappend(cxt.blist, stmt);
	result = list_concat(result, cxt.alist);
	result = list_concat(result, save_alist);

	MemoryContextDelete(cxt.tempCtx);

	return result;
}

/*
 * transformColumnDefinition -
 *		transform a single ColumnDef within CREATE TABLE
 *		Also used in ALTER TABLE ADD COLUMN
 */
static void
transformColumnDefinition(ParseState *pstate, CreateStmtContext *cxt,
						  ColumnDef *column)
{
	bool		is_serial;
	bool		saw_nullable;
	bool		saw_default;
	Constraint *constraint;
	ListCell   *clist;

	cxt->columns = lappend(cxt->columns, column);

	/* Check for SERIAL pseudo-types */
	is_serial = false;
	if (list_length(column->typeName->names) == 1 &&
		!column->typeName->pct_type)
	{
		char	   *typname = strVal(linitial(column->typeName->names));

		if (strcmp(typname, "serial") == 0 ||
			strcmp(typname, "serial4") == 0)
		{
			is_serial = true;
			column->typeName->names = NIL;
			column->typeName->typid = INT4OID;
		}
		else if (strcmp(typname, "bigserial") == 0 ||
				 strcmp(typname, "serial8") == 0)
		{
			is_serial = true;
			column->typeName->names = NIL;
			column->typeName->typid = INT8OID;
		}

		/*
		 * We have to reject "serial[]" explicitly, because once we've set
		 * typeid, LookupTypeName won't notice arrayBounds.  We don't need any
		 * special coding for serial(typmod) though.
		 */
		if (is_serial && column->typeName->arrayBounds != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("array of serial is not implemented")));
	}

	/* Do necessary work on the column type declaration */
	transformColumnType(pstate, column);

	/* Special actions for SERIAL pseudo-types */
	if (is_serial)
	{
		Oid			snamespaceid;
		char	   *snamespace;
		char	   *sname;
		char	   *qstring;
		A_Const    *snamenode;
		TypeCast   *castnode;
		FuncCall   *funccallnode;
		CreateSeqStmt *seqstmt;
		AlterSeqStmt *altseqstmt;
		List	   *attnamelist;

		/*
		 * Determine namespace and name to use for the sequence.
		 *
		 * Although we use ChooseRelationName, it's not guaranteed that the
		 * selected sequence name won't conflict; given sufficiently long
		 * field names, two different serial columns in the same table could
		 * be assigned the same sequence name, and we'd not notice since we
		 * aren't creating the sequence quite yet.  In practice this seems
		 * quite unlikely to be a problem, especially since few people would
		 * need two serial columns in one table.
		 */
		if (cxt->rel)
			snamespaceid = RelationGetNamespace(cxt->rel);
		else
			snamespaceid = RangeVarGetCreationNamespace(cxt->relation);
		snamespace = get_namespace_name(snamespaceid);
		sname = ChooseRelationName(cxt->relation->relname,
								   column->colname,
								   "seq",
								   snamespaceid);

		ereport(NOTICE,
				(errmsg("%s will create implicit sequence \"%s\" for serial column \"%s.%s\"",
						cxt->stmtType, sname,
						cxt->relation->relname, column->colname)));

		/*
		 * Build a CREATE SEQUENCE command to create the sequence object, and
		 * add it to the list of things to be done before this CREATE/ALTER
		 * TABLE.
		 */
		seqstmt = makeNode(CreateSeqStmt);
		seqstmt->sequence = makeRangeVar(snamespace, sname, -1);
		seqstmt->sequence->istemp = cxt->relation->istemp;
		seqstmt->options = NIL;

		cxt->blist = lappend(cxt->blist, seqstmt);

		/*
		 * Build an ALTER SEQUENCE ... OWNED BY command to mark the sequence
		 * as owned by this column, and add it to the list of things to be
		 * done after this CREATE/ALTER TABLE.
		 */
		altseqstmt = makeNode(AlterSeqStmt);
		altseqstmt->sequence = makeRangeVar(snamespace, sname, -1);
		attnamelist = list_make3(makeString(snamespace),
								 makeString(cxt->relation->relname),
								 makeString(column->colname));
		altseqstmt->options = list_make1(makeDefElem("owned_by",
													 (Node *) attnamelist));

		cxt->alist = lappend(cxt->alist, altseqstmt);

		/*
		 * Create appropriate constraints for SERIAL.  We do this in full,
		 * rather than shortcutting, so that we will detect any conflicting
		 * constraints the user wrote (like a different DEFAULT).
		 *
		 * Create an expression tree representing the function call
		 * nextval('sequencename').  We cannot reduce the raw tree to cooked
		 * form until after the sequence is created, but there's no need to do
		 * so.
		 */
		qstring = quote_qualified_identifier(snamespace, sname);
		snamenode = makeNode(A_Const);
		snamenode->val.type = T_String;
		snamenode->val.val.str = qstring;
		snamenode->location = -1;
		castnode = makeNode(TypeCast);
		castnode->typeName = SystemTypeName("regclass");
		castnode->arg = (Node *) snamenode;
		castnode->location = -1;
		funccallnode = makeNode(FuncCall);
		funccallnode->funcname = SystemFuncName("nextval");
		funccallnode->args = list_make1(castnode);
		funccallnode->agg_star = false;
		funccallnode->agg_distinct = false;
		funccallnode->func_variadic = false;
		funccallnode->location = -1;

		constraint = makeNode(Constraint);
		constraint->contype = CONSTR_DEFAULT;
		constraint->raw_expr = (Node *) funccallnode;
		constraint->cooked_expr = NULL;
		constraint->keys = NIL;
		column->constraints = lappend(column->constraints, constraint);

		constraint = makeNode(Constraint);
		constraint->contype = CONSTR_NOTNULL;
		column->constraints = lappend(column->constraints, constraint);
	}

	/* Process column constraints, if any... */
	transformConstraintAttrs(column->constraints);

	saw_nullable = false;
	saw_default = false;

	foreach(clist, column->constraints)
	{
		constraint = lfirst(clist);

		/*
		 * If this column constraint is a FOREIGN KEY constraint, then we fill
		 * in the current attribute's name and throw it into the list of FK
		 * constraints to be processed later.
		 */
		if (IsA(constraint, FkConstraint))
		{
			FkConstraint *fkconstraint = (FkConstraint *) constraint;

			fkconstraint->fk_attrs = list_make1(makeString(column->colname));
			cxt->fkconstraints = lappend(cxt->fkconstraints, fkconstraint);
			continue;
		}

		Assert(IsA(constraint, Constraint));

		switch (constraint->contype)
		{
			case CONSTR_NULL:
				if (saw_nullable && column->is_not_null)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("conflicting NULL/NOT NULL declarations for column \"%s\" of table \"%s\"",
								  column->colname, cxt->relation->relname)));
				column->is_not_null = FALSE;
				saw_nullable = true;
				break;

			case CONSTR_NOTNULL:
				if (saw_nullable && !column->is_not_null)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("conflicting NULL/NOT NULL declarations for column \"%s\" of table \"%s\"",
								  column->colname, cxt->relation->relname)));
				column->is_not_null = TRUE;
				saw_nullable = true;
				break;

			case CONSTR_DEFAULT:
				if (saw_default)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("multiple default values specified for column \"%s\" of table \"%s\"",
								  column->colname, cxt->relation->relname)));
				column->raw_default = constraint->raw_expr;
				Assert(constraint->cooked_expr == NULL);
				saw_default = true;
				break;

			case CONSTR_PRIMARY:
			case CONSTR_UNIQUE:
				if (constraint->keys == NIL)
					constraint->keys = list_make1(makeString(column->colname));
				cxt->ixconstraints = lappend(cxt->ixconstraints, constraint);
				break;

			case CONSTR_CHECK:
				cxt->ckconstraints = lappend(cxt->ckconstraints, constraint);
				break;

			case CONSTR_ATTR_DEFERRABLE:
			case CONSTR_ATTR_NOT_DEFERRABLE:
			case CONSTR_ATTR_DEFERRED:
			case CONSTR_ATTR_IMMEDIATE:
				/* transformConstraintAttrs took care of these */
				break;

			default:
				elog(ERROR, "unrecognized constraint type: %d",
					 constraint->contype);
				break;
		}
	}
}

/*
 * transformTableConstraint
 *		transform a Constraint node within CREATE TABLE or ALTER TABLE
 */
static void
transformTableConstraint(ParseState *pstate, CreateStmtContext *cxt,
						 Constraint *constraint)
{
	switch (constraint->contype)
	{
		case CONSTR_PRIMARY:
		case CONSTR_UNIQUE:
			cxt->ixconstraints = lappend(cxt->ixconstraints, constraint);
			break;

		case CONSTR_CHECK:
			cxt->ckconstraints = lappend(cxt->ckconstraints, constraint);
			break;

		case CONSTR_NULL:
		case CONSTR_NOTNULL:
		case CONSTR_DEFAULT:
		case CONSTR_ATTR_DEFERRABLE:
		case CONSTR_ATTR_NOT_DEFERRABLE:
		case CONSTR_ATTR_DEFERRED:
		case CONSTR_ATTR_IMMEDIATE:
			elog(ERROR, "invalid context for constraint type %d",
				 constraint->contype);
			break;

		default:
			elog(ERROR, "unrecognized constraint type: %d",
				 constraint->contype);
			break;
	}
}

/*
 * transformInhRelation
 *
 * Change the LIKE <subtable> portion of a CREATE TABLE statement into
 * column definitions which recreate the user defined column portions of
 * <subtable>.
 *
 * if forceBareCol is true we disallow inheriting any indexes/constr/defaults.
 */
void
transformInhRelation(ParseState *pstate, CreateStmtContext *cxt,
					 InhRelation *inhRelation, bool forceBareCol)
{
	AttrNumber	parent_attno;
	Relation	relation;
	TupleDesc	tupleDesc;
	TupleConstr *constr;
	AttrNumber *attmap;
	AclResult	aclresult;
	bool		including_defaults = false;
	bool		including_constraints = false;
	bool		including_indexes = false;
	ListCell   *elem;

	relation = parserOpenTable(pstate, inhRelation->relation, AccessShareLock,
							   false, NULL);

	if (relation->rd_rel->relkind != RELKIND_RELATION)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("inherited relation \"%s\" is not a table",
						inhRelation->relation->relname)));

	/*
	 * Check for SELECT privilages
	 */
	aclresult = pg_class_aclcheck(RelationGetRelid(relation), GetUserId(),
								  ACL_SELECT);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_CLASS,
					   RelationGetRelationName(relation));

	tupleDesc = RelationGetDescr(relation);
	constr = tupleDesc->constr;

	foreach(elem, inhRelation->options)
	{
		int			option = lfirst_int(elem);

		switch (option)
		{
			case CREATE_TABLE_LIKE_INCLUDING_DEFAULTS:
				including_defaults = true;
				break;
			case CREATE_TABLE_LIKE_EXCLUDING_DEFAULTS:
				including_defaults = false;
				break;
			case CREATE_TABLE_LIKE_INCLUDING_CONSTRAINTS:
				including_constraints = true;
				break;
			case CREATE_TABLE_LIKE_EXCLUDING_CONSTRAINTS:
				including_constraints = false;
				break;
			case CREATE_TABLE_LIKE_INCLUDING_INDEXES:
				including_indexes = true;
				break;
			case CREATE_TABLE_LIKE_EXCLUDING_INDEXES:
				including_indexes = false;
				break;
			default:
				elog(ERROR, "unrecognized CREATE TABLE LIKE option: %d",
					 option);
		}
	}

	if (forceBareCol && (including_indexes || including_constraints || including_defaults))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("LIKE INCLUDING may not be used with this kind of relation")));

	/*
	 * Initialize column number map for map_variable_attnos().  We need this
	 * since dropped columns in the source table aren't copied, so the new
	 * table can have different column numbers.
	 */
	attmap = (AttrNumber *) palloc0(sizeof(AttrNumber) * tupleDesc->natts);

	/*
	 * Insert the copied attributes into the cxt for the new table definition.
	 */
	for (parent_attno = 1; parent_attno <= tupleDesc->natts;
		 parent_attno++)
	{
		Form_pg_attribute attribute = tupleDesc->attrs[parent_attno - 1];
		char	   *attributeName = NameStr(attribute->attname);
		ColumnDef  *def;

		/*
		 * Ignore dropped columns in the parent.  attmap entry is left zero.
		 */
		if (attribute->attisdropped)
			continue;

		/*
		 * Create a new column, which is marked as NOT inherited.
		 *
		 * For constraints, ONLY the NOT NULL constraint is inherited by the
		 * new column definition per SQL99.
		 */
		def = makeNode(ColumnDef);
		def->colname = pstrdup(attributeName);
		def->typeName = makeTypeNameFromOid(attribute->atttypid,
											attribute->atttypmod);
		def->inhcount = 0;
		def->is_local = true;
		def->is_not_null = (forceBareCol ? false : attribute->attnotnull);
		def->raw_default = NULL;
		def->cooked_default = NULL;
		def->constraints = NIL;

		/*
		 * Add to column list
		 */
		cxt->columns = lappend(cxt->columns, def);

		attmap[parent_attno - 1] = list_length(cxt->columns);

		/*
		 * Copy default, if present and the default has been requested
		 */
		if (attribute->atthasdef && including_defaults)
		{
			char	   *this_default = NULL;
			AttrDefault *attrdef;
			int			i;

			/* Find default in constraint structure */
			Assert(constr != NULL);
			attrdef = constr->defval;
			for (i = 0; i < constr->num_defval; i++)
			{
				if (attrdef[i].adnum == parent_attno)
				{
					this_default = attrdef[i].adbin;
					break;
				}
			}
			Assert(this_default != NULL);

			/*
			 * If default expr could contain any vars, we'd need to fix 'em,
			 * but it can't; so default is ready to apply to child.
			 */

			def->cooked_default = pstrdup(this_default);
		}
	}

	/*
	 * Copy CHECK constraints if requested, being careful to adjust attribute
	 * numbers so they match the child.
	 */
	if (including_constraints && tupleDesc->constr)
	{
		int			ccnum;

		for (ccnum = 0; ccnum < tupleDesc->constr->num_check; ccnum++)
		{
			char	   *ccname = tupleDesc->constr->check[ccnum].ccname;
			char	   *ccbin = tupleDesc->constr->check[ccnum].ccbin;
			Constraint *n = makeNode(Constraint);
			Node	   *ccbin_node;
			bool		found_whole_row;

			ccbin_node = map_variable_attnos(stringToNode(ccbin),
											 1, 0,
											 attmap, tupleDesc->natts,
											 &found_whole_row);

			/*
			 * We reject whole-row variables because the whole point of LIKE
			 * is that the new table's rowtype might later diverge from the
			 * parent's.  So, while translation might be possible right now,
			 * it wouldn't be possible to guarantee it would work in future.
			 */
			if (found_whole_row)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot convert whole-row table reference"),
						 errdetail("Constraint \"%s\" contains a whole-row reference to table \"%s\".",
								   ccname,
								   RelationGetRelationName(relation))));

			n->contype = CONSTR_CHECK;
			n->name = pstrdup(ccname);
			n->raw_expr = NULL;
			n->cooked_expr = nodeToString(ccbin_node);
			n->indexspace = NULL;
			cxt->ckconstraints = lappend(cxt->ckconstraints, (Node *) n);
		}
	}

	/*
	 * Likewise, copy indexes if requested
	 */
	if (including_indexes && relation->rd_rel->relhasindex)
	{
		List	   *parent_indexes;
		ListCell   *l;

		parent_indexes = RelationGetIndexList(relation);

		foreach(l, parent_indexes)
		{
			Oid			parent_index_oid = lfirst_oid(l);
			Relation	parent_index;
			IndexStmt  *index_stmt;

			parent_index = index_open(parent_index_oid, AccessShareLock);

			/* Build CREATE INDEX statement to recreate the parent_index */
			index_stmt = generateClonedIndexStmt(cxt, parent_index,
												 attmap, tupleDesc->natts);

			/* Save it in the inh_indexes list for the time being */
			cxt->inh_indexes = lappend(cxt->inh_indexes, index_stmt);

			index_close(parent_index, AccessShareLock);
		}
	}

	/*
	 * Close the parent rel, but keep our AccessShareLock on it until xact
	 * commit.	That will prevent someone else from deleting or ALTERing the
	 * parent before the child is committed.
	 */
	heap_close(relation, NoLock);
}

/*
 * Generate an IndexStmt node using information from an already existing index
 * "source_idx".  Attribute numbers should be adjusted according to attmap.
 */
static IndexStmt *
generateClonedIndexStmt(CreateStmtContext *cxt, Relation source_idx,
						const AttrNumber *attmap, int attmap_length)
{
	Oid			source_relid = RelationGetRelid(source_idx);
	HeapTuple	ht_idxrel;
	HeapTuple	ht_idx;
	Form_pg_class idxrelrec;
	Form_pg_index idxrec;
	Form_pg_am	amrec;
	oidvector  *indclass;
	IndexStmt  *index;
	List	   *indexprs;
	ListCell   *indexpr_item;
	Oid			indrelid;
	Oid			conoid = InvalidOid;
	int			keyno;
	Oid			keycoltype;
	Datum		datum;
	bool		isnull;

	/*
	 * Fetch pg_class tuple of source index.  We can't use the copy in the
	 * relcache entry because it doesn't include optional fields.
	 */
	ht_idxrel = SearchSysCache(RELOID,
							   ObjectIdGetDatum(source_relid),
							   0, 0, 0);
	if (!HeapTupleIsValid(ht_idxrel))
		elog(ERROR, "cache lookup failed for relation %u", source_relid);
	idxrelrec = (Form_pg_class) GETSTRUCT(ht_idxrel);

	/* Fetch pg_index tuple for source index from relcache entry */
	ht_idx = source_idx->rd_indextuple;
	idxrec = (Form_pg_index) GETSTRUCT(ht_idx);
	indrelid = idxrec->indrelid;

	/* Fetch pg_am tuple for source index from relcache entry */
	amrec = source_idx->rd_am;

	/* Must get indclass the hard way, since it's not stored in relcache */
	datum = SysCacheGetAttr(INDEXRELID, ht_idx,
							Anum_pg_index_indclass, &isnull);
	Assert(!isnull);
	indclass = (oidvector *) DatumGetPointer(datum);

	/* Begin building the IndexStmt */
	index = makeNode(IndexStmt);
	index->relation = cxt->relation;
	index->accessMethod = pstrdup(NameStr(amrec->amname));
	if (OidIsValid(idxrelrec->reltablespace))
		index->tableSpace = get_tablespace_name(idxrelrec->reltablespace);
	else
		index->tableSpace = NULL;
	index->unique = idxrec->indisunique;
	index->primary = idxrec->indisprimary;
	index->concurrent = false;
	index->is_split_part = cxt->issplitpart;

	/*
	 * We don't try to preserve the name of the source index; instead, just
	 * let DefineIndex() choose a reasonable name.
	 */
	index->idxname = NULL;

	/*
	 * If the index is marked PRIMARY, it's certainly from a constraint; else,
	 * if it's not marked UNIQUE, it certainly isn't; else, we have to search
	 * pg_depend to see if there's an associated unique constraint.
	 */
	if (index->primary)
		index->isconstraint = true;
	else if (!index->unique)
		index->isconstraint = false;
	else
	{
		conoid = get_index_constraint(source_relid);
		index->isconstraint = OidIsValid(conoid);
	}

	/*
	 * If the index backs a constraint, use the same name for the constraint
	 * as the source uses. This is particularly important for partitioned
	 * tables, as some places assume that when a partitioned table has
	 * a constraint, the constraint has the same name in all the partitions.
	 */
	if (index->isconstraint)
	{
		char	   *conname;

		if (!OidIsValid(conoid))
			conoid = get_index_constraint(source_relid);

		conname = GetConstraintNameByOid(conoid);
		if (!conname)
			elog(ERROR, "could not find constraint that index \"%s\" backs in source table",
				 RelationGetRelationName(source_idx));

		index->altconname = conname;
	}

	/* Get the index expressions, if any */
	datum = SysCacheGetAttr(INDEXRELID, ht_idx,
							Anum_pg_index_indexprs, &isnull);
	if (!isnull)
	{
		char	   *exprsString;

		exprsString = TextDatumGetCString(datum);
		indexprs = (List *) stringToNode(exprsString);
	}
	else
		indexprs = NIL;

	/* Build the list of IndexElem */
	index->indexParams = NIL;

	indexpr_item = list_head(indexprs);
	for (keyno = 0; keyno < idxrec->indnatts; keyno++)
	{
		IndexElem  *iparam;
		AttrNumber	attnum = idxrec->indkey.values[keyno];
		int16		opt = source_idx->rd_indoption[keyno];

		iparam = makeNode(IndexElem);

		if (AttributeNumberIsValid(attnum))
		{
			/* Simple index column */
			char	   *attname;

			attname = get_relid_attribute_name(indrelid, attnum);
			keycoltype = get_atttype(indrelid, attnum);

			iparam->name = attname;
			iparam->expr = NULL;
		}
		else
		{
			/* Expressional index */
			Node	   *indexkey;
			bool		found_whole_row;

			if (indexpr_item == NULL)
				elog(ERROR, "too few entries in indexprs list");
			indexkey = (Node *) lfirst(indexpr_item);
			indexpr_item = lnext(indexpr_item);

			/* Adjust Vars to match new table's column numbering */
			indexkey = map_variable_attnos(indexkey,
										   1, 0,
										   attmap, attmap_length,
										   &found_whole_row);

			/* As in transformTableLikeClause, reject whole-row variables */
			if (found_whole_row)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot convert whole-row table reference"),
						 errdetail("Index \"%s\" contains a whole-row table reference.",
								   RelationGetRelationName(source_idx))));

			iparam->name = NULL;
			iparam->expr = indexkey;

			keycoltype = exprType(indexkey);
		}

		/* Add the operator class name, if non-default */
		iparam->opclass = get_opclass(indclass->values[keyno], keycoltype);

		iparam->ordering = SORTBY_DEFAULT;
		iparam->nulls_ordering = SORTBY_NULLS_DEFAULT;

		/* Adjust options if necessary */
		if (amrec->amcanorder)
		{
			/*
			 * If it supports sort ordering, copy DESC and NULLS opts. Don't
			 * set non-default settings unnecessarily, though, so as to
			 * improve the chance of recognizing equivalence to constraint
			 * indexes.
			 */
			if (opt & INDOPTION_DESC)
			{
				iparam->ordering = SORTBY_DESC;
				if ((opt & INDOPTION_NULLS_FIRST) == 0)
					iparam->nulls_ordering = SORTBY_NULLS_LAST;
			}
			else
			{
				if (opt & INDOPTION_NULLS_FIRST)
					iparam->nulls_ordering = SORTBY_NULLS_FIRST;
			}
		}

		index->indexParams = lappend(index->indexParams, iparam);
	}

	/* Copy reloptions if any */
	datum = SysCacheGetAttr(RELOID, ht_idxrel,
							Anum_pg_class_reloptions, &isnull);
	if (!isnull)
		index->options = untransformRelOptions(datum);

	/* If it's a partial index, decompile and append the predicate */
	datum = SysCacheGetAttr(INDEXRELID, ht_idx,
							Anum_pg_index_indpred, &isnull);
	if (!isnull)
	{
		char	   *pred_str;
		Node	   *pred_tree;
		bool		found_whole_row;

		/* Convert text string to node tree */
		pred_str = TextDatumGetCString(datum);
		pred_tree = (Node *) stringToNode(pred_str);

		/* Adjust Vars to match new table's column numbering */
		pred_tree = map_variable_attnos(pred_tree,
										1, 0,
										attmap, attmap_length,
										&found_whole_row);

		/* As in transformTableLikeClause, reject whole-row variables */
		if (found_whole_row)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot convert whole-row table reference"),
					 errdetail("Index \"%s\" contains a whole-row table reference.",
							   RelationGetRelationName(source_idx))));

		index->whereClause = pred_tree;
		/* Adjust attribute numbers */
		change_varattnos_of_a_node(index->whereClause, attmap);
	}

	/* Clean up */
	ReleaseSysCache(ht_idxrel);

	return index;
}

/*
 * get_opclass			- fetch name of an index operator class
 *
 * If the opclass is the default for the given actual_datatype, then
 * the return value is NIL.
 */
static List *
get_opclass(Oid opclass, Oid actual_datatype)
{
	HeapTuple	ht_opc;
	Form_pg_opclass opc_rec;
	List	   *result = NIL;

	ht_opc = SearchSysCache(CLAOID,
							ObjectIdGetDatum(opclass),
							0, 0, 0);
	if (!HeapTupleIsValid(ht_opc))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);
	opc_rec = (Form_pg_opclass) GETSTRUCT(ht_opc);

	if (GetDefaultOpClass(actual_datatype, opc_rec->opcmethod) != opclass)
	{
		/* For simplicity, we always schema-qualify the name */
		char	   *nsp_name = get_namespace_name(opc_rec->opcnamespace);
		char	   *opc_name = pstrdup(NameStr(opc_rec->opcname));

		result = list_make2(makeString(nsp_name), makeString(opc_name));
	}

	ReleaseSysCache(ht_opc);
	return result;
}

List *
transformCreateExternalStmt(CreateExternalStmt *stmt, const char *queryString)
{
	ParseState *pstate;
	CreateStmtContext cxt;
	List	   *result;
	ListCell   *elements;
	List  	   *likeDistributedBy = NIL;
	bool	    bQuiet = false;	/* shut up transformDistributedBy messages */
	bool		iswritable = stmt->iswritable;
	
	/* Set up pstate */
	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	cxt.stmtType = "CREATE EXTERNAL TABLE";
	cxt.relation = stmt->relation;
	cxt.inhRelations = NIL;
	cxt.hasoids = false;
	cxt.isalter = false;
	cxt.iscreatepart = false;
	cxt.columns = NIL;
	cxt.ckconstraints = NIL;
	cxt.fkconstraints = NIL;
	cxt.ixconstraints = NIL;
	cxt.pkey = NULL;
	cxt.rel = NULL;

	cxt.blist = NIL;
	cxt.alist = NIL;

	/*
	 * Run through each primary element in the table creation clause. Separate
	 * column defs from constraints, and do preliminary analysis.
	 */
	foreach(elements, stmt->tableElts)
	{
		Node	   *element = lfirst(elements);

		switch (nodeTag(element))
		{
			case T_ColumnDef:
				transformColumnDefinition(pstate, &cxt,
										  (ColumnDef *) element);
				break;

			case T_Constraint:
			case T_FkConstraint:
				/* should never happen. If it does fix gram.y */
				elog(ERROR, "node type %d not supported for external tables",
					 (int) nodeTag(element));
				break;

			case T_InhRelation:
				{
					/* LIKE */
					bool	isBeginning = (cxt.columns == NIL);

					transformInhRelation(pstate, &cxt,
										 (InhRelation *) element, true);

					if (Gp_role == GP_ROLE_DISPATCH && isBeginning &&
						stmt->distributedBy == NIL &&
						stmt->policy == NULL &&
						iswritable /* dont bother if readable table */)
					{
						likeDistributedBy = getLikeDistributionPolicy((InhRelation *) element);
					}
				}
				break;

			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(element));
				break;
		}
	}

	/*
	 * Forbid LOG ERRORS and ON MASTER combination.
	 */
	if (stmt->exttypedesc->exttabletype == EXTTBL_TYPE_EXECUTE)
	{
		ListCell   *exec_location_opt;

		foreach(exec_location_opt, stmt->exttypedesc->on_clause)
		{
			DefElem    *defel = (DefElem *) lfirst(exec_location_opt);

			if (strcmp(defel->defname, "master") == 0)
			{
				SingleRowErrorDesc *srehDesc = (SingleRowErrorDesc *)stmt->sreh;

				if(srehDesc && srehDesc->into_file)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("External web table with ON MASTER clause "
									"cannot use LOG ERRORS feature.")));
			}
		}
	}

	/*
	 * Handle DISTRIBUTED BY clause, if any.
	 *
	 * For writeable external tables, by default we distribute RANDOMLY, or
	 * by the distribution key of the LIKE table if exists. However, if
	 * DISTRIBUTED BY was specified we use it by calling the regular
	 * transformDistributedBy and handle it like we would for non external
	 * tables.
	 *
	 * For readable external tables, don't create a policy row at all.
	 * Non-EXECUTE type external tables are implicitly randomly distributed.
	 * EXECUTE type external tables encapsulate similar information in the
	 * "ON <segment spec>" clause, which is stored in pg_exttable.location.
	 */
	if (iswritable)
	{
		if (stmt->distributedBy == NIL && likeDistributedBy == NIL)
		{
			/*
			 * defaults to DISTRIBUTED RANDOMLY irrespective of the
			 * gp_create_table_random_default_distribution guc.
			 */
			stmt->policy = createRandomDistribution();
		}
		else
		{
			/* regular DISTRIBUTED BY transformation */
			transformDistributedBy(pstate, &cxt, stmt->distributedBy, &stmt->policy,
								   likeDistributedBy, bQuiet);
		}
	}
	else if (stmt->distributedBy != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Readable external tables can\'t specify a DISTRIBUTED BY clause.")));

	Assert(cxt.ckconstraints == NIL);
	Assert(cxt.fkconstraints == NIL);
	Assert(cxt.ixconstraints == NIL);

	/*
	 * Output results.
	 */
	stmt->tableElts = cxt.columns;

	result = lappend(cxt.blist, stmt);
	result = list_concat(result, cxt.alist);

	return result;
}


/****************stmt->policy*********************/
static void
transformDistributedBy(ParseState *pstate, CreateStmtContext *cxt,
					   List *distributedBy, GpPolicy **policyp,
					   List *likeDistributedBy,
					   bool bQuiet)
{
	ListCell   *keys = NULL;
	GpPolicy  *policy = NULL;
	int			colindex = 0;
	int			maxattrs = MaxPolicyAttributeNumber;
	int			numUniqueIndexes = 0;
	Constraint *uniqueindex = NULL;

	/*
	 * utility mode creates can't have a policy.  Only the QD can have policies
	 *
	 */
	if (Gp_role != GP_ROLE_DISPATCH && !IsBinaryUpgrade)
	{
		*policyp = NULL;
		return;
	}

	policy = (GpPolicy *) palloc(sizeof(GpPolicy) + maxattrs *
								 sizeof(policy->attrs[0]));
	policy->ptype = POLICYTYPE_PARTITIONED;
	policy->nattrs = 0;
	policy->attrs[0] = 1;

	/*
	 * If distributedBy is NIL, the user did not explicitly say what he
	 * wanted for a distribution policy.  So, we need to assign one.
	 */
	if (distributedBy == NIL && cxt && cxt->pkey != NULL)
	{
		/*
		 * We have a PRIMARY KEY, so let's assign the default distribution
		 * to be the key
		 */

		IndexStmt  *index = cxt->pkey;
		List	   *indexParams;
		ListCell   *ip = NULL;

		Assert(index->indexParams != NULL);
		indexParams = index->indexParams;

		foreach(ip, indexParams)
		{
			IndexElem  *iparam = lfirst(ip);

			if (iparam && iparam->name != 0)
			{

				if (distributedBy)
					distributedBy = lappend(distributedBy,
											(Node *) makeString(iparam->name));
				else
					distributedBy = list_make1((Node *) makeString(iparam->name));

			}
		}
	}

	if (cxt && cxt->ixconstraints != NULL)
	{
		ListCell   *lc = NULL;

		foreach(lc, cxt->ixconstraints)
		{
			Constraint *cons = lfirst(lc);

			if (cons->contype == CONSTR_UNIQUE)
			{
				if (uniqueindex == NULL)
					uniqueindex = cons;

				numUniqueIndexes++;

				if (cxt->pkey)
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("Greenplum Database does not allow having both PRIMARY KEY and UNIQUE constraints")));
				}
			}
		}
		if (numUniqueIndexes > 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("Greenplum Database does not allow having multiple UNIQUE constraints")));
		}
	}

	if (distributedBy == NIL && cxt && cxt->ixconstraints != NULL &&
		numUniqueIndexes > 0)
	{
		/*
		 * No explicit distributed by clause, and no primary key.
		 * If there is a UNIQUE clause, let's use that
		 */
		ListCell   *lc = NULL;

		foreach(lc, cxt->ixconstraints)
		{

			Constraint *constraint = lfirst(lc);

			if (constraint->contype == CONSTR_UNIQUE)
			{

				ListCell   *ip = NULL;


				foreach(ip, constraint->keys)
				{
					Value	   *v = lfirst(ip);

					if (v && v->val.str != 0)
					{

						if (distributedBy)
							distributedBy = lappend(distributedBy, (Node *) makeString(v->val.str));
						else
							distributedBy = list_make1((Node *) makeString(v->val.str));

					}
				}
			}
		}

	}

	/*
	 * If new table INHERITS from one or more parent tables, check parents.
	 */
	if (cxt->inhRelations != NIL)
	{
		ListCell   *entry;

		foreach(entry, cxt->inhRelations)
		{
			RangeVar   *parent = (RangeVar *) lfirst(entry);
			Oid			relId = RangeVarGetRelid(parent, false);
			GpPolicy  *oldTablePolicy =
				GpPolicyFetch(CurrentMemoryContext, relId);

			/*
			 * Partitioned child must have partitioned parents. During binary
			 * upgrade we allow to skip this check since that runs against a
			 * segment in utility mode and the distribution policy isn't stored
			 * in the segments.
			 */
			if ((oldTablePolicy == NULL ||
					oldTablePolicy->ptype != POLICYTYPE_PARTITIONED) &&
					!IsBinaryUpgrade)
			{
				ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
						errmsg("cannot inherit from catalog table \"%s\" "
							   "to create table \"%s\".",
							   parent->relname, cxt->relation->relname),
						errdetail("An inheritance hierarchy cannot contain a "
								  "mixture of distributed and "
								  "non-distributed tables.")));
			}
			/*
			 * If we still don't know what distribution to use, and this
			 * is an inherited table, set the distribution based on the
			 * parent (or one of the parents)
			 */
			if (distributedBy == NIL && oldTablePolicy->nattrs >= 0)
			{
				int ia;

				if (oldTablePolicy->nattrs > 0)
				{
					for (ia=0; ia<oldTablePolicy->nattrs; ia++)
					{
						char *attname =
							get_attname(relId, oldTablePolicy->attrs[ia]);

						distributedBy = lappend(distributedBy,
												(Node *) makeString(attname));
					}
				}
				else
				{
					/* strewn parent */
					distributedBy = lappend(distributedBy, (Node *)NULL);
				}
				if (!bQuiet)
				 elog(NOTICE, "Table has parent, setting distribution columns "
					 "to match parent table");
			}
			pfree(oldTablePolicy);
		}
	}

	if (distributedBy == NIL && likeDistributedBy != NIL)
	{
		distributedBy = likeDistributedBy;
		if (!bQuiet)
			elog(NOTICE, "Table doesn't have 'DISTRIBUTED BY' clause, "
				 "defaulting to distribution columns from LIKE table");
	}

	if (gp_create_table_random_default_distribution && NIL == distributedBy)
	{
		Assert(NIL == likeDistributedBy);
		policy = createRandomDistribution();
		
		if (!bQuiet)
		{
			ereport(NOTICE,
				(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
				 errmsg("Using default RANDOM distribution since no distribution was specified."),
				 errhint("Consider including the 'DISTRIBUTED BY' clause to determine the distribution of rows.")));
		}
	}
	else if (distributedBy == NIL)
	{
		/*
		 * if we get here, we haven't a clue what to use for the distribution columns.
		 */

		bool	assignedDefault = false;

		/*
		 * table has one or more attributes and there is still no distribution
		 * key. pick a default one. the winner is the first attribute that is
		 * an Greenplum Database-hashable data type.
		 */

		ListCell   *columns;
		ColumnDef  *column = NULL;

		/* we will distribute on at most one column */
		policy->nattrs = 1;

		colindex = 0;

		if (cxt->inhRelations)
		{
			bool		found = false;
			/* try inherited tables */
			ListCell   *inher;

			foreach(inher, cxt->inhRelations)
			{
				RangeVar   *inh = (RangeVar *) lfirst(inher);
				Relation	rel;
				int			count;

				Assert(IsA(inh, RangeVar));
				rel = heap_openrv(inh, AccessShareLock);
				if (rel->rd_rel->relkind != RELKIND_RELATION)
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					   errmsg("inherited relation \"%s\" is not a table",
							  inh->relname)));
				for (count = 0; count < rel->rd_att->natts; count++)
				{
					Form_pg_attribute inhattr = rel->rd_att->attrs[count];
					Oid typeOid = inhattr->atttypid;

					if (inhattr->attisdropped)
						continue;
					colindex++;
					if(isGreenplumDbHashable(typeOid))
					{
						char	   *inhname = NameStr(inhattr->attname);
						policy->attrs[0] = colindex;
						assignedDefault = true;
						if (!bQuiet)
							ereport(NOTICE,
								(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
								 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column "
										"named '%s' from parent table as the Greenplum Database data distribution key for this "
										"table. ", inhname),
								 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
								 		 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
						found = true;
						break;
					}
				}
				heap_close(rel, NoLock);

				if (assignedDefault)
					break;
			}

		}

		if (!assignedDefault)
		{
			foreach(columns, cxt->columns)
			{
				Oid			typeOid;
				int32		typmod;

				column = (ColumnDef *) lfirst(columns);
				colindex++;

				typeOid = typenameTypeId(NULL, column->typeName, &typmod);

				/*
				 * if we can hash on this type, or if it's an array type (which
				 * we can always hash) this column with be our default key.
				 */
				if (isGreenplumDbHashable(typeOid))
				{
					policy->attrs[0] = colindex;
					assignedDefault = true;
					if (!bQuiet)
						ereport(NOTICE,
							(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
							 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column "
									"named '%s' as the Greenplum Database data distribution key for this "
									"table. ", column->colname),
							 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
							 		 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
					break;
				}
			}
		}

		if (!assignedDefault)
		{
			/*
			 * There was no eligible distribution column to default to. This table
			 * will be partitioned on an empty distribution key list. In other words,
			 * tuples coming into the system will be randomly assigned a bucket.
			 */
			policy->nattrs = 0;
			if (!bQuiet)
				elog(NOTICE, "Table doesn't have 'DISTRIBUTED BY' clause, and no column type is suitable for a distribution key. Creating a NULL policy entry.");
		}

	}
	else
	{
		/*
		 * We have a DISTRIBUTED BY column list, either specified by the user
		 * or defaulted to a primary key or unique column. Process it now and
		 * set the distribution policy.
		 */
		policy->nattrs = 0;
		if (!(distributedBy->length == 1 && linitial(distributedBy) == NULL))
		{
			foreach(keys, distributedBy)
			{
				char	   *key = strVal(lfirst(keys));
				bool		found = false;
				ColumnDef  *column = NULL;
				ListCell   *columns;

				colindex = 0;

				if (cxt->inhRelations)
				{
					/* try inherited tables */
					ListCell   *inher;

					foreach(inher, cxt->inhRelations)
					{
						RangeVar   *inh = (RangeVar *) lfirst(inher);
						Relation	rel;
						int			count;

						Assert(IsA(inh, RangeVar));
						rel = heap_openrv(inh, AccessShareLock);
						if (rel->rd_rel->relkind != RELKIND_RELATION)
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							   errmsg("inherited relation \"%s\" is not a table",
									  inh->relname)));
						for (count = 0; count < rel->rd_att->natts; count++)
						{
							Form_pg_attribute inhattr = rel->rd_att->attrs[count];
							char	   *inhname = NameStr(inhattr->attname);

							if (inhattr->attisdropped)
								continue;
							colindex++;
							if (strcmp(key, inhname) == 0)
							{
								found = true;

								break;
							}
						}
						heap_close(rel, NoLock);
						if (found)
							elog(DEBUG1, "DISTRIBUTED BY clause refers to columns of inherited table");

						if (found)
							break;
					}
				}

				if (!found)
				{
					foreach(columns, cxt->columns)
					{
						column = (ColumnDef *) lfirst(columns);
						Assert(IsA(column, ColumnDef));
						colindex++;

						if (strcmp(column->colname, key) == 0)
						{
							Oid			typeOid;
							int32		typmod;

							typeOid = typenameTypeId(NULL, column->typeName, &typmod);
							
							/*
							 * To be a part of a distribution key, this type must
							 * be supported for hashing internally in Greenplum
							 * Database. We check if the base type is supported
							 * for hashing or if it is an array type (we support
							 * hashing on all array types).
							 */
							if (!isGreenplumDbHashable(typeOid))
							{
								ereport(ERROR,
										(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
										 errmsg("type \"%s\" can't be a part of a "
												"distribution key",
												format_type_be(typeOid))));
							}

							found = true;
							break;
						}
					}
				}

				/*
				* In the ALTER TABLE case, don't complain about index keys
				* not created in the command; they may well exist already.
				* DefineIndex will complain about them if not, and will also
				* take care of marking them NOT NULL.
				*/
				if (!found && !cxt->isalter)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" named in 'DISTRIBUTED BY' clause does not exist",
									key)));

				policy->attrs[policy->nattrs++] = colindex;
			}
		}
	}


	*policyp = policy;


	if (cxt && cxt->pkey)		/* Primary key	specified.	Make sure
								 * distribution columns match */
	{
		int			i = 0;
		IndexStmt  *index = cxt->pkey;
		List	   *indexParams = index->indexParams;
		ListCell   *ip;

		foreach(ip, indexParams)
		{
			IndexElem  *iparam;

			if (i >= policy->nattrs)
				break;

			iparam = lfirst(ip);
			if (iparam->name != 0)
			{
				bool	found = false;
				ColumnDef  *column = NULL;
				ListCell   *columns;

				colindex = 0;

				if (cxt->inhRelations)
				{
					/* try inherited tables */
					ListCell   *inher;

					foreach(inher, cxt->inhRelations)
					{
						RangeVar   *inh = (RangeVar *) lfirst(inher);
						Relation	rel;
						int			count;

						Assert(IsA(inh, RangeVar));
						rel = heap_openrv(inh, AccessShareLock);
						if (rel->rd_rel->relkind != RELKIND_RELATION)
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							   errmsg("inherited relation \"%s\" is not a table",
									  inh->relname)));
						for (count = 0; count < rel->rd_att->natts; count++)
						{
							Form_pg_attribute inhattr = rel->rd_att->attrs[count];
							char	   *inhname = NameStr(inhattr->attname);

							if (inhattr->attisdropped)
								continue;
							colindex++;

							if (strcmp(iparam->name, inhname) == 0)
							{
								found = true;
								break;
							}
						}
						heap_close(rel, NoLock);

						if (found)
							elog(DEBUG1, "'DISTRIBUTED BY' clause refers to "
								 "columns of inherited table");

						if (found)
							break;
					}
				}

				if (!found)
				{
					foreach(columns, cxt->columns)
					{
						column = (ColumnDef *) lfirst(columns);
						Assert(IsA(column, ColumnDef));
						colindex++;
						if (strcmp(column->colname, iparam->name) == 0)
						{
							found = true;
							break;
						}
					}
				}
				if (colindex != policy->attrs[i])
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("PRIMARY KEY and DISTRIBUTED BY definitions incompatible"),
							 errhint("When there is both a PRIMARY KEY, and a "
									"DISTRIBUTED BY clause, the DISTRIBUTED BY "
									"clause must be equal to or a left-subset "
									"of the PRIMARY KEY")));
				}

				i++;
			}
		}
	}

	if (uniqueindex)			/* UNIQUE specified.  Make sure distribution
								 * columns match */
	{
		int			i = 0;

		List	   *keys = uniqueindex->keys;
		ListCell   *ip;

		foreach(ip, keys)
		{
			IndexElem  *iparam;

			if (i >= policy->nattrs)
				break;

			iparam = lfirst(ip);
			if (iparam->name != 0)
			{
				bool	found = false;
				ColumnDef  *column = NULL;
				ListCell   *columns;

				colindex = 0;

				if (cxt->inhRelations)
				{
					/* try inherited tables */
					ListCell   *inher;

					foreach(inher, cxt->inhRelations)
					{
						RangeVar   *inh = (RangeVar *) lfirst(inher);
						Relation	rel;
						int			count;

						Assert(IsA(inh, RangeVar));
						rel = heap_openrv(inh, AccessShareLock);
						if (rel->rd_rel->relkind != RELKIND_RELATION)
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							   errmsg("inherited relation \"%s\" is not a table",
									  inh->relname)));
						for (count = 0; count < rel->rd_att->natts; count++)
						{
							Form_pg_attribute inhattr = rel->rd_att->attrs[count];
							char	   *inhname = NameStr(inhattr->attname);

							if (inhattr->attisdropped)
								continue;
							colindex++;

							if (strcmp(iparam->name, inhname) == 0)
							{
								found = true;

								break;
							}
						}
						heap_close(rel, NoLock);
						if (found)
							elog(NOTICE, "'DISTRIBUTED BY' clause refers to columns of inherited table");

						if (found)
							break;
					}
				}

				if (!found)
				foreach(columns, cxt->columns)
				{
					column = (ColumnDef *) lfirst(columns);
					Assert(IsA(column, ColumnDef));
					colindex++;
					if (strcmp(column->colname, iparam->name) == 0)
					{
						found = true;
						break;
					}
				}

				if (colindex != policy->attrs[i])
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("UNIQUE constraint and DISTRIBUTED BY "
									"definitions incompatible"),
							 errhint("When there is both a UNIQUE constraint, "
									 "and a DISTRIBUTED BY clause, the "
									 "DISTRIBUTED BY clause must be equal to "
									 "or a left-subset of the UNIQUE columns")));
				}
				i++;
			}
		}
	}
}

/*
 * Add any missing encoding attributes (compresstype = none,
 * blocksize=...).  The column specific encoding attributes supported
 * today are compresstype, compresslevel and blocksize.  Refer to
 * pg_compression.c for more info.
 */
static List *
fillin_encoding(List *list)
{
	bool foundCompressType = false;
	bool foundCompressTypeNone = false;
	char *cmplevel = NULL;
	bool foundBlockSize = false;
	char *arg;
	List *retList = list_copy(list);
	ListCell *lc;
	DefElem *el;
	const StdRdOptions *ao_opts = currentAOStorageOptions();

	foreach(lc, list)
	{
		el = lfirst(lc);

		if (pg_strcasecmp("compresstype", el->defname) == 0)
		{
			foundCompressType = true;
			arg = defGetString(el);
			if (pg_strcasecmp("none", arg) == 0)
				foundCompressTypeNone = true;
		}
		else if (pg_strcasecmp("compresslevel", el->defname) == 0)
		{
			cmplevel = defGetString(el);
		}
		else if (pg_strcasecmp("blocksize", el->defname) == 0)
			foundBlockSize = true;
	}

	if (foundCompressType == false && cmplevel == NULL)
	{
		/* No compression option specified, use current defaults. */
		arg = ao_opts->compresstype ?
				pstrdup(ao_opts->compresstype) : "none";
		el = makeDefElem("compresstype", (Node *) makeString(arg));
		retList = lappend(retList, el);
		el = makeDefElem("compresslevel",
						 (Node *) makeInteger(ao_opts->compresslevel));
		retList = lappend(retList, el);
	}
	else if (foundCompressType == false && cmplevel)
	{
		if (strcmp(cmplevel, "0") == 0)
		{
			/*
			 * User wants to disable compression by specifying
			 * compresslevel=0.
			 */
			el = makeDefElem("compresstype", (Node *) makeString("none"));
			retList = lappend(retList, el);
		}
		else
		{
			/*
			 * User wants to enable compression by specifying non-zero
			 * compresslevel.  Therefore, choose default compresstype
			 * if configured, otherwise use zlib.
			 */
			if (ao_opts->compresstype &&
				strcmp(ao_opts->compresstype, "none") != 0)
			{
				arg = pstrdup(ao_opts->compresstype);
			}
			else
			{
				arg = AO_DEFAULT_COMPRESSTYPE;
			}
			el = makeDefElem("compresstype", (Node *) makeString(arg));
			retList = lappend(retList, el);
		}
	}
	else if (foundCompressType && cmplevel == NULL)
	{
		if (foundCompressTypeNone)
		{
			/*
			 * User wants to disable compression by specifying
			 * compresstype=none.
			 */
			el = makeDefElem("compresslevel", (Node *) makeInteger(0));
			retList = lappend(retList, el);
		}
		else
		{
			/*
			 * Valid compresstype specified.  Use default
			 * compresslevel if it's non-zero, otherwise use 1.
			 */
			el = makeDefElem("compresslevel",
							 (Node *) makeInteger(ao_opts->compresslevel > 0 ?
												  ao_opts->compresslevel : 1));
			retList = lappend(retList, el);
		}
	}
	if (foundBlockSize == false)
	{
		el = makeDefElem("blocksize", (Node *) makeInteger(ao_opts->blocksize));
		retList = lappend(retList, el);
	}
	return retList;
}

/*
 * transformIndexConstraints
 *		Handle UNIQUE and PRIMARY KEY constraints, which create indexes.
 *		We also merge in any index definitions arising from
 *		LIKE ... INCLUDING INDEXES.
 */
static void
transformIndexConstraints(ParseState *pstate, CreateStmtContext *cxt, bool mayDefer)
{
	IndexStmt  *index;
	List	   *indexlist = NIL;
	ListCell   *lc;

	/*
	 * Run through the constraints that need to generate an index. For PRIMARY
	 * KEY, mark each column as NOT NULL and create an index. For UNIQUE,
	 * create an index as for PRIMARY KEY, but do not insist on NOT NULL.
	 */
	foreach(lc, cxt->ixconstraints)
	{
		Constraint *constraint = (Constraint *) lfirst(lc);

		Assert(IsA(constraint, Constraint));
		Assert(constraint->contype == CONSTR_PRIMARY ||
			   constraint->contype == CONSTR_UNIQUE);

		index = transformIndexConstraint(constraint, cxt);

		indexlist = lappend(indexlist, index);
	}

	/* Add in any indexes defined by LIKE ... INCLUDING INDEXES */
	foreach(lc, cxt->inh_indexes)
	{
		index = (IndexStmt *) lfirst(lc);

		if (index->primary)
		{
			if (cxt->pkey != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("multiple primary keys for table \"%s\" are not allowed",
								cxt->relation->relname)));
			cxt->pkey = index;
		}

		indexlist = lappend(indexlist, index);
	}

	/*
	 * Scan the index list and remove any redundant index specifications. This
	 * can happen if, for instance, the user writes UNIQUE PRIMARY KEY. A
	 * strict reading of SQL92 would suggest raising an error instead, but
	 * that strikes me as too anal-retentive. - tgl 2001-02-14
	 *
	 * XXX in ALTER TABLE case, it'd be nice to look for duplicate
	 * pre-existing indexes, too.
	 */
	Assert(cxt->alist == NIL);
	if (cxt->pkey != NULL)
	{
		/* Make sure we keep the PKEY index in preference to others... */
		cxt->alist = list_make1(cxt->pkey);
	}

	foreach(lc, indexlist)
	{
		bool		keep = true;
		bool		defer = false;
		ListCell   *k;

		index = lfirst(lc);

		/* if it's pkey, it's already in cxt->alist */
		if (index == cxt->pkey)
			continue;

		foreach(k, cxt->alist)
		{
			IndexStmt  *priorindex = lfirst(k);

			if (equal(index->indexParams, priorindex->indexParams) &&
				equal(index->whereClause, priorindex->whereClause) &&
				strcmp(index->accessMethod, priorindex->accessMethod) == 0)
			{
				priorindex->unique |= index->unique;

				/*
				 * If the prior index is as yet unnamed, and this one is
				 * named, then transfer the name to the prior index. This
				 * ensures that if we have named and unnamed constraints,
				 * we'll use (at least one of) the names for the index.
				 */
				if (priorindex->idxname == NULL)
					priorindex->idxname = index->idxname;
				keep = false;
				break;
			}
		}
		
		defer = index->whereClause != NULL;
		if ( !defer )
		{
			ListCell *j;
			foreach(j, index->indexParams)
			{
				IndexElem *elt = (IndexElem*)lfirst(j);
				Assert(IsA(elt, IndexElem));
				
				if (elt->expr != NULL)
				{
					defer = true;
					break;
				}
			}
		}

		if (keep)
		{
			if (defer && mayDefer)
			{
				/* An index on an expression with a WHERE clause or for an 
				 * inheritance child will cause a trip through parse_analyze.  
				 * If we do that before creating the table, it will fail, so 
				 * we put it on a list for later.
				 */
			
				ereport(DEBUG1,
						(errmsg("deferring index creation for table \"%s\"",
								cxt->relation->relname)
						 ));
				cxt->dlist = lappend(cxt->dlist, index);
			}
			else
			{
				cxt->alist = lappend(cxt->alist, index);
			}
		}
	}
}

/*
 * transformIndexConstraint
 *		Transform one UNIQUE or PRIMARY KEY constraint for
 *		transformIndexConstraints.
 */
static IndexStmt *
transformIndexConstraint(Constraint *constraint, CreateStmtContext *cxt)
{
	IndexStmt  *index;
	ListCell   *keys;
	IndexElem  *iparam;

	index = makeNode(IndexStmt);

	index->unique = true;
	index->primary = (constraint->contype == CONSTR_PRIMARY);
	if (index->primary)
	{
		if (cxt->pkey != NULL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
			 errmsg("multiple primary keys for table \"%s\" are not allowed",
					cxt->relation->relname)));
		cxt->pkey = index;

		/*
		 * In ALTER TABLE case, a primary index might already exist, but
		 * DefineIndex will check for it.
		 */
	}
	index->isconstraint = true;

	/*
	 * We used to force the index name to be the constraint name, but they
	 * are in different namespaces and so have different  requirements for
	 * uniqueness. Here we leave the index name alone and put the constraint
	 * name in the IndexStmt, for use in DefineIndex.
	 */
	index->idxname = NULL;	/* DefineIndex will choose name */
	index->altconname = constraint->name; /* User may have picked the name. */

	index->relation = cxt->relation;
	index->accessMethod = DEFAULT_INDEX_TYPE;
	index->options = constraint->options;
	index->tableSpace = constraint->indexspace;
	index->indexParams = NIL;
	index->whereClause = NULL;
	index->concurrent = false;

	/*
	 * Make sure referenced keys exist.  If we are making a PRIMARY KEY index,
	 * also make sure they are NOT NULL, if possible. (Although we could leave
	 * it to DefineIndex to mark the columns NOT NULL, it's more efficient to
	 * get it right the first time.)
	 */
	foreach(keys, constraint->keys)
	{
		char	   *key = strVal(lfirst(keys));
		bool		found = false;
		ColumnDef  *column = NULL;
		ListCell   *columns;

		foreach(columns, cxt->columns)
		{
			column = (ColumnDef *) lfirst(columns);
			Assert(IsA(column, ColumnDef));
			if (strcmp(column->colname, key) == 0)
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			/* found column in the new table; force it to be NOT NULL */
			if (constraint->contype == CONSTR_PRIMARY)
				column->is_not_null = TRUE;
		}
		else if (SystemAttributeByName(key, cxt->hasoids) != NULL)
		{
			/*
			 * column will be a system column in the new table, so accept it.
			 * System columns can't ever be null, so no need to worry about
			 * PRIMARY/NOT NULL constraint.
			 */
			found = true;
		}
		else if (cxt->inhRelations)
		{
			/* try inherited tables */
			ListCell   *inher;

			foreach(inher, cxt->inhRelations)
			{
				RangeVar   *inh = (RangeVar *) lfirst(inher);
				Relation	rel;
				int			count;

				Assert(IsA(inh, RangeVar));
				rel = heap_openrv(inh, AccessShareLock);
				if (rel->rd_rel->relkind != RELKIND_RELATION)
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						   errmsg("inherited relation \"%s\" is not a table",
								  inh->relname)));
				for (count = 0; count < rel->rd_att->natts; count++)
				{
					Form_pg_attribute inhattr = rel->rd_att->attrs[count];
					char	   *inhname = NameStr(inhattr->attname);

					if (inhattr->attisdropped)
						continue;
					if (strcmp(key, inhname) == 0)
					{
						found = true;

						/*
						 * We currently have no easy way to force an inherited
						 * column to be NOT NULL at creation, if its parent
						 * wasn't so already. We leave it to DefineIndex to
						 * fix things up in this case.
						 */
						break;
					}
				}
				heap_close(rel, NoLock);
				if (found)
					break;
			}
		}

		/*
		 * In the ALTER TABLE case, don't complain about index keys not
		 * created in the command; they may well exist already. DefineIndex
		 * will complain about them if not, and will also take care of marking
		 * them NOT NULL.
		 */
		if (!found && !cxt->isalter)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" named in key does not exist",
							key)));

		/* Check for PRIMARY KEY(foo, foo) */
		foreach(columns, index->indexParams)
		{
			iparam = (IndexElem *) lfirst(columns);
			if (iparam->name && strcmp(key, iparam->name) == 0)
			{
				if (index->primary)
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_COLUMN),
							 errmsg("column \"%s\" appears twice in primary key constraint",
									key)));
				else
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_COLUMN),
					errmsg("column \"%s\" appears twice in unique constraint",
						   key)));
			}
		}

		/* OK, add it to the index definition */
		iparam = makeNode(IndexElem);
		iparam->name = pstrdup(key);
		iparam->expr = NULL;
		iparam->opclass = NIL;
		iparam->ordering = SORTBY_DEFAULT;
		iparam->nulls_ordering = SORTBY_NULLS_DEFAULT;
		index->indexParams = lappend(index->indexParams, iparam);
	}

	return index;
}

/*
 * transformFKConstraints
 *		handle FOREIGN KEY constraints
 */
static void
transformFKConstraints(ParseState *pstate, CreateStmtContext *cxt,
					   bool skipValidation, bool isAddConstraint)
{
	ListCell   *fkclist;

	if (cxt->fkconstraints == NIL)
		return;

	/*
	 * If CREATE TABLE or adding a column with NULL default, we can safely
	 * skip validation of the constraint.
	 */
	if (skipValidation)
	{
		foreach(fkclist, cxt->fkconstraints)
		{
			FkConstraint *fkconstraint = (FkConstraint *) lfirst(fkclist);

			fkconstraint->skip_validation = true;
		}
	}

	/*
	 * For CREATE TABLE or ALTER TABLE ADD COLUMN, gin up an ALTER TABLE ADD
	 * CONSTRAINT command to execute after the basic command is complete. (If
	 * called from ADD CONSTRAINT, that routine will add the FK constraints to
	 * its own subcommand list.)
	 *
	 * Note: the ADD CONSTRAINT command must also execute after any index
	 * creation commands.  Thus, this should run after
	 * transformIndexConstraints, so that the CREATE INDEX commands are
	 * already in cxt->alist.
	 */
	if (!isAddConstraint)
	{
		AlterTableStmt *alterstmt = makeNode(AlterTableStmt);

		alterstmt->relation = cxt->relation;
		alterstmt->cmds = NIL;
		alterstmt->relkind = OBJECT_TABLE;

		foreach(fkclist, cxt->fkconstraints)
		{
			FkConstraint *fkconstraint = (FkConstraint *) lfirst(fkclist);
			AlterTableCmd *altercmd = makeNode(AlterTableCmd);

			altercmd->subtype = AT_ProcessedConstraint;
			altercmd->name = NULL;
			altercmd->def = (Node *) fkconstraint;
			alterstmt->cmds = lappend(alterstmt->cmds, altercmd);
		}

		cxt->alist = lappend(cxt->alist, alterstmt);
	}
}

/*
 * transformIndexStmt - parse analysis for CREATE INDEX
 *
 * Note: this is a no-op for an index not using either index expressions or
 * a predicate expression.	There are several code paths that create indexes
 * without bothering to call this, because they know they don't have any
 * such expressions to deal with.
 *
 * In GPDB, this returns a list, because the single statement can be
 * expanded into multiple IndexStmts, if the table is a partitioned table.
 */
List *
transformIndexStmt(IndexStmt *stmt, const char *queryString)
{
	bool		recurseToPartitions = false;

	/*
	 * If the table already exists (i.e., this isn't a create table time
	 * expansion of primary key() or unique()) and we're the ultimate parent
	 * of a partitioned table, cascade to all children. We don't do this
	 * at create table time because transformPartitionBy() automatically
	 * creates the indexes on the child tables for us.
	 *
	 * If this is a CREATE INDEX statement, idxname should already exist.
	 */
	if (Gp_role != GP_ROLE_EXECUTE && stmt->idxname != NULL)
	{
		Oid			relId;

		relId = RangeVarGetRelid(stmt->relation, true);

		if (relId != InvalidOid && rel_is_partitioned(relId))
			recurseToPartitions = true;
	}

	return transformIndexStmt_recurse(stmt, queryString, NULL, recurseToPartitions);

}
static List *
transformIndexStmt_recurse(IndexStmt *stmt, const char *queryString,
						   ParseState *masterpstate, bool recurseToPartitions)
{
	Relation	rel;
	ParseState *pstate;
	RangeTblEntry *rte;
	ListCell   *l;
	List	   *result = NIL;
	LOCKMODE	lockmode;

	/*
	 * We must not scribble on the passed-in IndexStmt, so copy it.  (This is
	 * overkill, but easy.)
	 */
	stmt = (IndexStmt *) copyObject(stmt);

	/*
	 * Open the parent table with appropriate locking.	We must do this
	 * because addRangeTableEntry() would acquire only AccessShareLock,
	 * leaving DefineIndex() needing to do a lock upgrade with consequent risk
	 * of deadlock.  Make sure this stays in sync with the type of lock
	 * DefineIndex() wants.
	 */
	lockmode = stmt->concurrent ? ShareUpdateExclusiveLock : ShareLock;
	rel = heap_openrv(stmt->relation, lockmode);

	/* Set up pstate */
	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	/* Recurse into (sub)partitions if this is a partitioned table */
	if (recurseToPartitions)
	{
		List		*children;
		struct HTAB *nameCache;
		Oid			nspOid;

		nspOid = RangeVarGetCreationNamespace(stmt->relation);

		if (masterpstate == NULL)
			masterpstate = pstate;

		/* Lookup the parser object name cache */
		nameCache = parser_get_namecache(masterpstate);

		/* Loop over all partition children */
		/* GPDB_84_MERGE_FIXME: do we need another lock here, or did the above
		 * heap_openrv() take care of it? */
		children = find_inheritance_children(RelationGetRelid(rel), NoLock);

		foreach(l, children)
		{
			Oid			relid = lfirst_oid(l);
			Relation	crel = heap_open(relid, NoLock); /* lock on master
															 is enough */
			IndexStmt  *chidx;
			Relation	partrel;
			HeapTuple	tuple;
			ScanKeyData scankey;
			SysScanDesc sscan;
			char	   *parname;
			int2		position;
			int4		depth;
			NameData	name;
			Oid			paroid;
			char		depthstr[NAMEDATALEN];
			char		prtstr[NAMEDATALEN];

			if (RelationIsExternal(crel))
			{
				elog(NOTICE, "skip building index for external partition \"%s\"",
					 RelationGetRelationName(crel));
				heap_close(crel, NoLock);
				continue;
			}

			chidx = (IndexStmt *)copyObject((Node *)stmt);

			/* now just update the relation and index name fields */
			chidx->relation =
				makeRangeVar(get_namespace_name(RelationGetNamespace(crel)),
							 pstrdup(RelationGetRelationName(crel)), -1);

			elog(NOTICE, "building index for child partition \"%s\"",
				 RelationGetRelationName(crel));

			/*
			 * We want the index name to resemble our partition table name
			 * with the master index name on the front. This means, we
			 * append to the indexname the parname, position, and depth
			 * as we do in transformPartitionBy().
			 *
			 * So, firstly we must retrieve from pg_partition_rule the
			 * partition descriptor for the current relid. This gives us
			 * partition name and position. With paroid, we can get the
			 * partition level descriptor from pg_partition and therefore
			 * our depth.
			 */
			partrel = heap_open(PartitionRuleRelationId, AccessShareLock);

			/* SELECT * FROM pg_partition_rule WHERE parchildrelid = :1 */
			ScanKeyInit(&scankey,
						Anum_pg_partition_rule_parchildrelid,
						BTEqualStrategyNumber, F_OIDEQ,
						ObjectIdGetDatum(relid));
			sscan = systable_beginscan(partrel, PartitionRuleParchildrelidIndexId,
									   true, SnapshotNow, 1, &scankey);
			tuple = systable_getnext(sscan);
			Assert(HeapTupleIsValid(tuple));

			name = ((Form_pg_partition_rule)GETSTRUCT(tuple))->parname;
			parname = pstrdup(NameStr(name));
			position = ((Form_pg_partition_rule)GETSTRUCT(tuple))->parruleord;
			paroid = ((Form_pg_partition_rule)GETSTRUCT(tuple))->paroid;

			systable_endscan(sscan);
			heap_close(partrel, NoLock);

			tuple = SearchSysCache1(PARTOID,
									ObjectIdGetDatum(paroid));
			Assert(HeapTupleIsValid(tuple));

			depth = ((Form_pg_partition)GETSTRUCT(tuple))->parlevel + 1;

			ReleaseSysCache(tuple);

			heap_close(crel, NoLock);

			/* now, build the piece to append */
			snprintf(depthstr, sizeof(depthstr), "%d", depth);
			if (strlen(parname) == 0)
				snprintf(prtstr, sizeof(prtstr), "prt_%d", position);
			else
				snprintf(prtstr, sizeof(prtstr), "prt_%s", parname);

			chidx->idxname = ChooseRelationNameWithCache(stmt->idxname,
														 depthstr, /* depth */
														 prtstr,   /* part spec */
														 nspOid,
														 nameCache);

			result = list_concat(result,
								 transformIndexStmt_recurse(chidx, queryString,
															masterpstate, true));
		}
	}

	/*
	 * Put the parent table into the rtable so that the expressions can refer
	 * to its fields without qualification.
	 */
	rte = addRangeTableEntryForRelation(pstate, rel, NULL, false, true);

	/* no to join list, yes to namespaces */
	addRTEtoQuery(pstate, rte, false, true, true);

	/* take care of the where clause */
	if (stmt->whereClause)
		stmt->whereClause = transformWhereClause(pstate,
												 stmt->whereClause,
												 EXPR_KIND_INDEX_PREDICATE,
												 "WHERE");

	/* take care of any index expressions */
	foreach(l, stmt->indexParams)
	{
		IndexElem  *ielem = (IndexElem *) lfirst(l);

		if (ielem->expr)
		{
			ielem->expr = transformExpr(pstate, ielem->expr,
										EXPR_KIND_INDEX_EXPRESSION);

			/*
			 * transformExpr() should have already rejected subqueries,
			 * aggregates, and window functions, based on the EXPR_KIND_ for
			 * an index expression.
			 *
			 * Also reject expressions returning sets; this is for consistency
			 * with what transformWhereClause() checks for the predicate.
			 * DefineIndex() will make more checks.
			 */
			if (expression_returns_set(ielem->expr))
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("index expression cannot return a set")));
		}
	}

	/*
	 * Check that only the base rel is mentioned.
	 */
	if (list_length(pstate->p_rtable) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
				 errmsg("index expressions and predicates can refer only to the table being indexed")));

	free_parsestate(pstate);

	/*
	 * Close relation, but keep the lock. Unless this is a CREATE INDEX
	 * for a partitioned table, and we're processing a partition. In that
	 * case, we want to release the lock on the partition early, so that
	 * you don't run out of space in the lock manager if there are a lot
	 * of partitions. Holding the lock on the parent table should be
	 * enough.
	 */
	if (!rel_needs_long_lock(RelationGetRelid(rel)))
		heap_close(rel, lockmode);
	else
		heap_close(rel, NoLock);

	result = lcons(stmt, result);

	return result;
}


/*
 * transformRuleStmt -
 *	  transform a CREATE RULE Statement. The action is a list of parse
 *	  trees which is transformed into a list of query trees, and we also
 *	  transform the WHERE clause if any.
 *
 * actions and whereClause are output parameters that receive the
 * transformed results.
 *
 * Note that we must not scribble on the passed-in RuleStmt, so we do
 * copyObject() on the actions and WHERE clause.
 */
void
transformRuleStmt(RuleStmt *stmt, const char *queryString,
				  List **actions, Node **whereClause)
{
	Relation	rel;
	ParseState *pstate;
	RangeTblEntry *oldrte;
	RangeTblEntry *newrte;

	/*
	 * To avoid deadlock, make sure the first thing we do is grab
	 * AccessExclusiveLock on the target relation.	This will be needed by
	 * DefineQueryRewrite(), and we don't want to grab a lesser lock
	 * beforehand.
	 */
	rel = heap_openrv(stmt->relation, AccessExclusiveLock);

	/* Set up pstate */
	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	/*
	 * NOTE: 'OLD' must always have a varno equal to 1 and 'NEW' equal to 2.
	 * Set up their RTEs in the main pstate for use in parsing the rule
	 * qualification.
	 */
	oldrte = addRangeTableEntryForRelation(pstate, rel,
										   makeAlias("*OLD*", NIL),
										   false, false);
	newrte = addRangeTableEntryForRelation(pstate, rel,
										   makeAlias("*NEW*", NIL),
										   false, false);
	/* Must override addRangeTableEntry's default access-check flags */
	oldrte->requiredPerms = 0;
	newrte->requiredPerms = 0;

	/*
	 * They must be in the namespace too for lookup purposes, but only add the
	 * one(s) that are relevant for the current kind of rule.  In an UPDATE
	 * rule, quals must refer to OLD.field or NEW.field to be unambiguous, but
	 * there's no need to be so picky for INSERT & DELETE.  We do not add them
	 * to the joinlist.
	 */
	switch (stmt->event)
	{
		case CMD_SELECT:
			addRTEtoQuery(pstate, oldrte, false, true, true);
			break;
		case CMD_UPDATE:
			addRTEtoQuery(pstate, oldrte, false, true, true);
			addRTEtoQuery(pstate, newrte, false, true, true);
			break;
		case CMD_INSERT:
			addRTEtoQuery(pstate, newrte, false, true, true);
			break;
		case CMD_DELETE:
			addRTEtoQuery(pstate, oldrte, false, true, true);
			break;
		default:
			elog(ERROR, "unrecognized event type: %d",
				 (int) stmt->event);
			break;
	}

	/* take care of the where clause */
	*whereClause = transformWhereClause(pstate,
									  (Node *) copyObject(stmt->whereClause),
										EXPR_KIND_WHERE,
										"WHERE");

	if (list_length(pstate->p_rtable) != 2)		/* naughty, naughty... */
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("rule WHERE condition cannot contain references to other relations")));

	/*
	 * 'instead nothing' rules with a qualification need a query rangetable so
	 * the rewrite handler can add the negated rule qualification to the
	 * original query. We create a query with the new command type CMD_NOTHING
	 * here that is treated specially by the rewrite system.
	 */
	if (stmt->actions == NIL)
	{
		Query	   *nothing_qry = makeNode(Query);

		nothing_qry->commandType = CMD_NOTHING;
		nothing_qry->rtable = pstate->p_rtable;
		nothing_qry->jointree = makeFromExpr(NIL, NULL);		/* no join wanted */

		*actions = list_make1(nothing_qry);
	}
	else
	{
		ListCell   *l;
		List	   *newactions = NIL;

		/*
		 * transform each statement, like parse_sub_analyze()
		 */
		foreach(l, stmt->actions)
		{
			Node	   *action = (Node *) lfirst(l);
			ParseState *sub_pstate = make_parsestate(NULL);
			Query	   *sub_qry,
					   *top_subqry;
			bool		has_old,
						has_new;

			/*
			 * Since outer ParseState isn't parent of inner, have to pass down
			 * the query text by hand.
			 */
			sub_pstate->p_sourcetext = queryString;

			/*
			 * Set up OLD/NEW in the rtable for this statement.  The entries
			 * are added only to relnamespace, not varnamespace, because we
			 * don't want them to be referred to by unqualified field names
			 * nor "*" in the rule actions.  We decide later whether to put
			 * them in the joinlist.
			 */
			oldrte = addRangeTableEntryForRelation(sub_pstate, rel,
												   makeAlias("*OLD*", NIL),
												   false, false);
			newrte = addRangeTableEntryForRelation(sub_pstate, rel,
												   makeAlias("*NEW*", NIL),
												   false, false);
			oldrte->requiredPerms = 0;
			newrte->requiredPerms = 0;
			addRTEtoQuery(sub_pstate, oldrte, false, true, false);
			addRTEtoQuery(sub_pstate, newrte, false, true, false);

			/* Transform the rule action statement */
			top_subqry = transformStmt(sub_pstate,
									   (Node *) copyObject(action));

			/*
			 * We cannot support utility-statement actions (eg NOTIFY) with
			 * nonempty rule WHERE conditions, because there's no way to make
			 * the utility action execute conditionally.
			 */
			if (top_subqry->commandType == CMD_UTILITY &&
				*whereClause != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("rules with WHERE conditions can only have SELECT, INSERT, UPDATE, or DELETE actions")));

			/*
			 * If the action is INSERT...SELECT, OLD/NEW have been pushed down
			 * into the SELECT, and that's what we need to look at. (Ugly
			 * kluge ... try to fix this when we redesign querytrees.)
			 */
			sub_qry = getInsertSelectQuery(top_subqry, NULL);

			/*
			 * If the sub_qry is a setop, we cannot attach any qualifications
			 * to it, because the planner won't notice them.  This could
			 * perhaps be relaxed someday, but for now, we may as well reject
			 * such a rule immediately.
			 */
			if (sub_qry->setOperations != NULL && *whereClause != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("conditional UNION/INTERSECT/EXCEPT statements are not implemented")));

			/*
			 * Validate action's use of OLD/NEW, qual too
			 */
			has_old =
				rangeTableEntry_used((Node *) sub_qry, PRS2_OLD_VARNO, 0) ||
				rangeTableEntry_used(*whereClause, PRS2_OLD_VARNO, 0);
			has_new =
				rangeTableEntry_used((Node *) sub_qry, PRS2_NEW_VARNO, 0) ||
				rangeTableEntry_used(*whereClause, PRS2_NEW_VARNO, 0);

			switch (stmt->event)
			{
				case CMD_SELECT:
					if (has_old)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON SELECT rule cannot use OLD")));
					if (has_new)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON SELECT rule cannot use NEW")));
					break;
				case CMD_UPDATE:
					/* both are OK */
					break;
				case CMD_INSERT:
					if (has_old)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON INSERT rule cannot use OLD")));
					break;
				case CMD_DELETE:
					if (has_new)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON DELETE rule cannot use NEW")));
					break;
				default:
					elog(ERROR, "unrecognized event type: %d",
						 (int) stmt->event);
					break;
			}

			/*
			 * For efficiency's sake, add OLD to the rule action's jointree
			 * only if it was actually referenced in the statement or qual.
			 *
			 * For INSERT, NEW is not really a relation (only a reference to
			 * the to-be-inserted tuple) and should never be added to the
			 * jointree.
			 *
			 * For UPDATE, we treat NEW as being another kind of reference to
			 * OLD, because it represents references to *transformed* tuples
			 * of the existing relation.  It would be wrong to enter NEW
			 * separately in the jointree, since that would cause a double
			 * join of the updated relation.  It's also wrong to fail to make
			 * a jointree entry if only NEW and not OLD is mentioned.
			 */
			if (has_old || (has_new && stmt->event == CMD_UPDATE))
			{
				/*
				 * If sub_qry is a setop, manipulating its jointree will do no
				 * good at all, because the jointree is dummy. (This should be
				 * a can't-happen case because of prior tests.)
				 */
				if (sub_qry->setOperations != NULL)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("conditional UNION/INTERSECT/EXCEPT statements are not implemented")));
				/* hack so we can use addRTEtoQuery() */
				sub_pstate->p_rtable = sub_qry->rtable;
				sub_pstate->p_joinlist = sub_qry->jointree->fromlist;
				addRTEtoQuery(sub_pstate, oldrte, true, false, false);
				sub_qry->jointree->fromlist = sub_pstate->p_joinlist;
			}

			newactions = lappend(newactions, top_subqry);

			free_parsestate(sub_pstate);
		}

		*actions = newactions;
	}

	free_parsestate(pstate);

	/* Close relation, but keep the exclusive lock */
	heap_close(rel, NoLock);
}


/*
 * transformAlterTableStmt -
 *		parse analysis for ALTER TABLE
 *
 * Returns a List of utility commands to be done in sequence.  One of these
 * will be the transformed AlterTableStmt, but there may be additional actions
 * to be done before and after the actual AlterTable() call.
 */
List *
transformAlterTableStmt(AlterTableStmt *stmt, const char *queryString)
{
	Relation	rel;
	ParseState *pstate;
	CreateStmtContext cxt;
	List	   *result;
	List	   *save_alist;
	ListCell   *lcmd,
			   *l;
	List	   *newcmds = NIL;
	bool		skipValidation = true;
	AlterTableCmd *newcmd;

	/*
	 * We must not scribble on the passed-in AlterTableStmt, so copy it. (This
	 * is overkill, but easy.)
	 */
	stmt = (AlterTableStmt *) copyObject(stmt);

	/*
	 * Acquire exclusive lock on the target relation, which will be held until
	 * end of transaction.	This ensures any decisions we make here based on
	 * the state of the relation will still be good at execution. We must get
	 * exclusive lock now because execution will; taking a lower grade lock
	 * now and trying to upgrade later risks deadlock.
	 *
	 * In GPDB, we release the lock early if this command is part of a
	 * partitioned CREATE TABLE.
	 */
	rel = relation_openrv(stmt->relation, AccessExclusiveLock);

	/* Set up pstate */
	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = queryString;

	cxt.stmtType = "ALTER TABLE";
	cxt.relation = stmt->relation;
	cxt.rel = rel;
	cxt.inhRelations = NIL;
	cxt.isalter = true;
	cxt.iscreatepart = false;
	cxt.hasoids = false;		/* need not be right */
	cxt.columns = NIL;
	cxt.ckconstraints = NIL;
	cxt.fkconstraints = NIL;
	cxt.ixconstraints = NIL;
	cxt.inh_indexes = NIL;
	cxt.blist = NIL;
	cxt.alist = NIL;
	cxt.dlist = NIL; /* used by transformCreateStmt, not here */
	cxt.pkey = NULL;

	/*
	 * The only subtypes that currently require parse transformation handling
	 * are ADD COLUMN and ADD CONSTRAINT.  These largely re-use code from
	 * CREATE TABLE.
	 * And ALTER TABLE ... <operator> PARTITION ...
	 */
	foreach(lcmd, stmt->cmds)
	{
		AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);

		switch (cmd->subtype)
		{
			case AT_AddColumn:
			case AT_AddColumnToView:
				{
					ColumnDef  *def = (ColumnDef *) cmd->def;

					Assert(IsA(def, ColumnDef));

					/*
					 * Adding a column with a primary key or unique constraint
					 * is not supported in GPDB.
					 */
					if (Gp_role == GP_ROLE_DISPATCH)
					{
						ListCell *c;
						foreach(c, def->constraints)
						{
							Constraint *cons = (Constraint *) lfirst(c);
							if (cons->contype == CONSTR_PRIMARY)
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("cannot add column with primary key constraint")));
							if (cons->contype == CONSTR_UNIQUE)
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("cannot add column with unique constraint")));
						}
					}
					transformColumnDefinition(pstate, &cxt, def);

					/*
					 * If the column has a non-null default, we can't skip
					 * validation of foreign keys.
					 */
					if (def->raw_default != NULL)
						skipValidation = false;

					/*
					 * All constraints are processed in other ways. Remove the
					 * original list
					 */
					def->constraints = NIL;

					newcmds = lappend(newcmds, cmd);
					break;
				}
			case AT_AddConstraint:

				/*
				 * The original AddConstraint cmd node doesn't go to newcmds
				 */
				if (IsA(cmd->def, Constraint))
					transformTableConstraint(pstate, &cxt,
											 (Constraint *) cmd->def);
				else if (IsA(cmd->def, FkConstraint))
				{
					cxt.fkconstraints = lappend(cxt.fkconstraints, cmd->def);

					/* GPDB: always skip validation of foreign keys */
					skipValidation = true;
				}
				else
					elog(ERROR, "unrecognized node type: %d",
						 (int) nodeTag(cmd->def));
				break;

			case AT_ProcessedConstraint:

				/*
				 * Already-transformed ADD CONSTRAINT, so just make it look
				 * like the standard case.
				 */
				cmd->subtype = AT_AddConstraint;
				newcmds = lappend(newcmds, cmd);
				break;

				/* CDB: Partitioned Tables */
            case AT_PartAlter:			/* Alter */
            case AT_PartAdd:			/* Add */
            case AT_PartDrop:			/* Drop */
            case AT_PartExchange:		/* Exchange */
            case AT_PartRename:			/* Rename */
            case AT_PartSetTemplate:	/* Set Subpartition Template */
            case AT_PartSplit:			/* Split */
            case AT_PartTruncate:		/* Truncate */
				cmd = transformAlterTable_all_PartitionStmt(
					pstate,
					stmt,
					&cxt,
					cmd);

				newcmds = lappend(newcmds, cmd);
				break;

			case AT_PartAddInternal:	/* Add partition, as part of CREATE TABLE */
				cxt.iscreatepart = true;
				newcmds = lappend(newcmds, cmd);
				break;

			default:
				newcmds = lappend(newcmds, cmd);
				break;
		}
	}

	/*
	 * transformIndexConstraints wants cxt.alist to contain only index
	 * statements, so transfer anything we already have into save_alist
	 * immediately.
	 */
	save_alist = cxt.alist;
	cxt.alist = NIL;

	/* Postprocess index and FK constraints */
	transformIndexConstraints(pstate, &cxt, false);

	transformFKConstraints(pstate, &cxt, skipValidation, true);

	/*
	 * Push any index-creation commands into the ALTER, so that they can be
	 * scheduled nicely by tablecmds.c.  Note that tablecmds.c assumes that
	 * the IndexStmt attached to an AT_AddIndex subcommand has already been
	 * through transformIndexStmt.
	 */
	foreach(l, cxt.alist)
	{
		Node	   *idxstmt = (Node *) lfirst(l);
		List	   *idxstmts;
		ListCell   *li;

		idxstmts = transformIndexStmt((IndexStmt *) idxstmt,
									  queryString);
		foreach(li, idxstmts)
		{
			Assert(IsA(idxstmt, IndexStmt));
			newcmd = makeNode(AlterTableCmd);
			newcmd->subtype = AT_AddIndex;
			newcmd->def = lfirst(li);
			newcmds = lappend(newcmds, newcmd);
		}
	}
	cxt.alist = NIL;

	/* Append any CHECK or FK constraints to the commands list */
	foreach(l, cxt.ckconstraints)
	{
		newcmd = makeNode(AlterTableCmd);
		newcmd->subtype = AT_AddConstraint;
		newcmd->def = (Node *) lfirst(l);
		newcmds = lappend(newcmds, newcmd);
	}
	foreach(l, cxt.fkconstraints)
	{
		newcmd = makeNode(AlterTableCmd);
		newcmd->subtype = AT_AddConstraint;
		newcmd->def = (Node *) lfirst(l);
		newcmds = lappend(newcmds, newcmd);
	}

	/*
	 * Close rel but keep lock
	 *
	 * If this is part of a CREATE TABLE of a partitioned table, creating
	 * the partitions, we release the lock immediately, however. We hold
	 * a lock on the parent table, and no-one can see the partitions yet,
	 * so the lock on each partition isn't strictly required. Creating a
	 * massively partitioned table could otherwise require holding a lot
	 * of locks, running out of shared memory in the lock manager.
	 */
	if (cxt.iscreatepart)
		relation_close(rel, AccessExclusiveLock);
	else
		relation_close(rel, NoLock);

	/*
	 * Output results.
	 */
	stmt->cmds = newcmds;

	result = lappend(cxt.blist, stmt);
	result = list_concat(result, cxt.alist);
	result = list_concat(result, save_alist);

	return result;
}


/*
 * Preprocess a list of column constraint clauses
 * to attach constraint attributes to their primary constraint nodes
 * and detect inconsistent/misplaced constraint attributes.
 *
 * NOTE: currently, attributes are only supported for FOREIGN KEY primary
 * constraints, but someday they ought to be supported for other constraints.
 */
static void
transformConstraintAttrs(List *constraintList)
{
	Node	   *lastprimarynode = NULL;
	bool		saw_deferrability = false;
	bool		saw_initially = false;
	ListCell   *clist;

	foreach(clist, constraintList)
	{
		Node	   *node = lfirst(clist);

		if (!IsA(node, Constraint))
		{
			lastprimarynode = node;
			/* reset flags for new primary node */
			saw_deferrability = false;
			saw_initially = false;
		}
		else
		{
			Constraint *con = (Constraint *) node;

			switch (con->contype)
			{
				case CONSTR_ATTR_DEFERRABLE:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("misplaced DEFERRABLE clause")));
					if (saw_deferrability)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple DEFERRABLE/NOT DEFERRABLE clauses not allowed")));
					saw_deferrability = true;
					((FkConstraint *) lastprimarynode)->deferrable = true;
					break;
				case CONSTR_ATTR_NOT_DEFERRABLE:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("misplaced NOT DEFERRABLE clause")));
					if (saw_deferrability)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple DEFERRABLE/NOT DEFERRABLE clauses not allowed")));
					saw_deferrability = true;
					((FkConstraint *) lastprimarynode)->deferrable = false;
					if (saw_initially &&
						((FkConstraint *) lastprimarynode)->initdeferred)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("constraint declared INITIALLY DEFERRED must be DEFERRABLE")));
					break;
				case CONSTR_ATTR_DEFERRED:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("misplaced INITIALLY DEFERRED clause")));
					if (saw_initially)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple INITIALLY IMMEDIATE/DEFERRED clauses not allowed")));
					saw_initially = true;
					((FkConstraint *) lastprimarynode)->initdeferred = true;

					/*
					 * If only INITIALLY DEFERRED appears, assume DEFERRABLE
					 */
					if (!saw_deferrability)
						((FkConstraint *) lastprimarynode)->deferrable = true;
					else if (!((FkConstraint *) lastprimarynode)->deferrable)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("constraint declared INITIALLY DEFERRED must be DEFERRABLE")));
					break;
				case CONSTR_ATTR_IMMEDIATE:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("misplaced INITIALLY IMMEDIATE clause")));
					if (saw_initially)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple INITIALLY IMMEDIATE/DEFERRED clauses not allowed")));
					saw_initially = true;
					((FkConstraint *) lastprimarynode)->initdeferred = false;
					break;
				default:
					/* Otherwise it's not an attribute */
					lastprimarynode = node;
					/* reset flags for new primary node */
					saw_deferrability = false;
					saw_initially = false;
					break;
			}
		}
	}
}

/*
 * Special handling of type definition for a column
 */
static void
transformColumnType(ParseState *pstate, ColumnDef *column)
{
	/*
	 * All we really need to do here is verify that the type is valid.
	 */
	Type		ctype = typenameType(pstate, column->typeName, NULL);

	ReleaseSysCache(ctype);
}


/*
 * transformCreateSchemaStmt -
 *	  analyzes the CREATE SCHEMA statement
 *
 * Split the schema element list into individual commands and place
 * them in the result list in an order such that there are no forward
 * references (e.g. GRANT to a table created later in the list). Note
 * that the logic we use for determining forward references is
 * presently quite incomplete.
 *
 * SQL92 also allows constraints to make forward references, so thumb through
 * the table columns and move forward references to a posterior alter-table
 * command.
 *
 * The result is a list of parse nodes that still need to be analyzed ---
 * but we can't analyze the later commands until we've executed the earlier
 * ones, because of possible inter-object references.
 *
 * Note: this breaks the rules a little bit by modifying schema-name fields
 * within passed-in structs.  However, the transformation would be the same
 * if done over, so it should be all right to scribble on the input to this
 * extent.
 */
List *
transformCreateSchemaStmt(CreateSchemaStmt *stmt)
{
	CreateSchemaStmtContext cxt;
	List	   *result;
	ListCell   *elements;

	cxt.stmtType = "CREATE SCHEMA";
	cxt.schemaname = stmt->schemaname;
	cxt.authid = stmt->authid;
	cxt.sequences = NIL;
	cxt.tables = NIL;
	cxt.views = NIL;
	cxt.indexes = NIL;
	cxt.triggers = NIL;
	cxt.grants = NIL;

	/*
	 * Run through each schema element in the schema element list. Separate
	 * statements by type, and do preliminary analysis.
	 */
	foreach(elements, stmt->schemaElts)
	{
		Node	   *element = lfirst(elements);

		switch (nodeTag(element))
		{
			case T_CreateSeqStmt:
				{
					CreateSeqStmt *elp = (CreateSeqStmt *) element;

					setSchemaName(cxt.schemaname, &elp->sequence->schemaname);
					cxt.sequences = lappend(cxt.sequences, element);
				}
				break;

			case T_CreateStmt:
				{
					CreateStmt *elp = (CreateStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);

					/*
					 * XXX todo: deal with constraints
					 */
					cxt.tables = lappend(cxt.tables, element);
				}
				break;

			case T_ViewStmt:
				{
					ViewStmt   *elp = (ViewStmt *) element;

					setSchemaName(cxt.schemaname, &elp->view->schemaname);

					/*
					 * XXX todo: deal with references between views
					 */
					cxt.views = lappend(cxt.views, element);
				}
				break;

			case T_IndexStmt:
				{
					IndexStmt  *elp = (IndexStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);
					cxt.indexes = lappend(cxt.indexes, element);
				}
				break;

			case T_CreateTrigStmt:
				{
					CreateTrigStmt *elp = (CreateTrigStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);
					cxt.triggers = lappend(cxt.triggers, element);
				}
				break;

			case T_GrantStmt:
				cxt.grants = lappend(cxt.grants, element);
				break;

			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(element));
		}
	}

	result = NIL;
	result = list_concat(result, cxt.sequences);
	result = list_concat(result, cxt.tables);
	result = list_concat(result, cxt.views);
	result = list_concat(result, cxt.indexes);
	result = list_concat(result, cxt.triggers);
	result = list_concat(result, cxt.grants);

	return result;
}

/*
 * setSchemaName
 *		Set or check schema name in an element of a CREATE SCHEMA command
 */
static void
setSchemaName(char *context_schema, char **stmt_schema_name)
{
	if (*stmt_schema_name == NULL)
		*stmt_schema_name = context_schema;
	else if (strcmp(context_schema, *stmt_schema_name) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_SCHEMA_DEFINITION),
				 errmsg("CREATE specifies a schema (%s) "
						"different from the one being created (%s)",
						*stmt_schema_name, context_schema)));
}

/*
 * getLikeDistributionPolicy
 *
 * For Greenplum Database distributed tables, default to
 * the same distribution as the first LIKE table, unless
 * we also have INHERITS
 */
static List *
getLikeDistributionPolicy(InhRelation* e)
{
	List*			likeDistributedBy = NIL;
	Oid				relId;
	GpPolicy*		oldTablePolicy;

	relId = RangeVarGetRelid(e->relation, false);
	oldTablePolicy = GpPolicyFetch(CurrentMemoryContext, relId);

	if (oldTablePolicy != NULL &&
		oldTablePolicy->ptype == POLICYTYPE_PARTITIONED)
	{
		int ia;

		if (oldTablePolicy->nattrs > 0)
		{
			for (ia = 0 ; ia < oldTablePolicy->nattrs ; ia++)
			{
				char *attname = get_attname(relId, oldTablePolicy->attrs[ia]);

				if (likeDistributedBy)
					likeDistributedBy = lappend(likeDistributedBy, (Node *) makeString(attname));
				else
					likeDistributedBy = list_make1((Node *) makeString(attname));
			}
		}
		else
		{	/* old table is distributed randomly. */
			likeDistributedBy = list_make1((Node *) NULL);
		}
	}

	return likeDistributedBy;
}



/*
 * Transform and validate the actual encoding clauses.
 *
 * We need tell the underlying system that these are AO/CO tables too,
 * hence the concatenation of the extra elements.
 */
List *
transformStorageEncodingClause(List *options)
{
	Datum d;
	ListCell *lc;
	DefElem *dl;
	foreach(lc, options)
	{
		dl = (DefElem *) lfirst(lc);
		if (pg_strncasecmp(dl->defname, SOPT_CHECKSUM, strlen(SOPT_CHECKSUM))
			== 0)
		{
			elog(ERROR, "\"%s\" is not a column specific option.",
				 SOPT_CHECKSUM);
		}
	}
	List *extra = list_make2(makeDefElem("appendonly",
										 (Node *)makeString("true")),
							 makeDefElem("orientation",
										 (Node *)makeString("column")));

	/* add defaults for missing values */
	options = fillin_encoding(options);

	/*
	 * The following two statements validate that the encoding clause is well
	 * formed.
	 */
	d = transformRelOptions(PointerGetDatum(NULL),
									  list_concat(extra, options),
									  /* GPDB_84_MERGE_FIXME: do we need any
									   * namespaces? */
									  NULL, NULL,
									  true, false);
	(void)heap_reloptions(RELKIND_RELATION, d, true);

	return options;
}

/*
 * Validate the sanity of column reference storage clauses.
 *
 * 1. Ensure that we only refer to columns that exist.
 * 2. Ensure that each column is referenced either zero times or once.
 */
static void
validateColumnStorageEncodingClauses(List *stenc, CreateStmt *stmt)
{
	ListCell *lc;
	struct HTAB *ht = NULL;
	struct colent {
		char colname[NAMEDATALEN];
		int count;
	} *ce = NULL;

	if (!stenc)
		return;

	/* Generate a hash table for all the columns */
	foreach(lc, stmt->tableElts)
	{
		Node *n = lfirst(lc);

		if (IsA(n, ColumnDef))
		{
			ColumnDef *c = (ColumnDef *)n;
			char *colname;
			bool found = false;
			size_t n = NAMEDATALEN - 1 < strlen(c->colname) ?
							NAMEDATALEN - 1 : strlen(c->colname);

			colname = palloc0(NAMEDATALEN);
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, c->colname, n);
			colname[n] = '\0';

			if (!ht)
			{
				HASHCTL  cacheInfo;
				int      cacheFlags;

				memset(&cacheInfo, 0, sizeof(cacheInfo));
				cacheInfo.keysize = NAMEDATALEN;
				cacheInfo.entrysize = sizeof(*ce);
				cacheFlags = HASH_ELEM;

				ht = hash_create("column info cache",
								 list_length(stmt->tableElts),
								 &cacheInfo, cacheFlags);
			}

			ce = hash_search(ht, colname, HASH_ENTER, &found);

			/*
			 * The user specified a duplicate column name. We check duplicate
			 * column names VERY late (under MergeAttributes(), which is called
			 * by DefineRelation(). For the specific case here, it is safe to
			 * call out that this is a duplicate. We don't need to delay until
			 * we look at inheritance.
			 */
			if (found)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" duplicated",
								colname)));
				
			}
			ce->count = 0;
		}
	}

	/*
	 * If the table has no columns -- usually in the partitioning case -- then
	 * we can short circuit.
	 */
	if (!ht)
		return;

	/*
	 * All column reference storage directives without the DEFAULT
	 * clause should refer to real columns.
	 */
	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		Insist(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
			continue;
		else
		{
			bool found = false;
			char colname[NAMEDATALEN];
			size_t collen = strlen(c->column);
			size_t n = NAMEDATALEN - 1 < collen ? NAMEDATALEN - 1 : collen;
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, c->column, n);
			colname[n] = '\0';

			ce = hash_search(ht, colname, HASH_FIND, &found);

			if (!found)
				elog(ERROR, "column \"%s\" does not exist", colname);

			ce->count++;

			if (ce->count > 1)
				elog(ERROR, "column \"%s\" referenced in more than one "
					 "COLUMN ENCODING clause", colname);

		}
	}

	hash_destroy(ht);
}

/*
 * Find the column reference storage encoding clause for `column'.
 *
 * This is called by transformAttributeEncoding() in a loop but stenc should be
 * quite small in practice.
 */
static ColumnReferenceStorageDirective *
find_crsd(char *column, List *stenc)
{
	ListCell *lc;

	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		if (c->deflt == false && strcmp(column, c->column) == 0)
			return c;
	}
	return NULL;
}


List *
TypeNameGetStorageDirective(TypeName *typname)
{
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			typid;
	int32		typmod;
	List	   *out = NIL;

	typid = typenameTypeId(NULL, typname, &typmod);

	rel = heap_open(TypeEncodingRelationId, AccessShareLock);

	/* SELECT typoptions FROM pg_type_encoding where typid = :1 */
	ScanKeyInit(&scankey,
				Anum_pg_type_encoding_typid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(typid));
	sscan = systable_beginscan(rel, TypeEncodingTypidIndexId,
							   true, SnapshotNow, 1, &scankey);
	tuple = systable_getnext(sscan);
	if (HeapTupleIsValid(tuple))
	{
		Datum options;
		bool isnull;

		options = heap_getattr(tuple,
							   Anum_pg_type_encoding_typoptions,
							   RelationGetDescr(rel),
							   &isnull);

		Insist(!isnull);

		out = untransformRelOptions(options);
	}

	systable_endscan(sscan);
	heap_close(rel, AccessShareLock);

	return out;
}

/*
 * Make a default column storage directive from a WITH clause
 * Ignore options in the WITH clause that don't appear in 
 * storage_directives for column-level compression.
 */
List *
form_default_storage_directive(List *enc)
{
	List *out = NIL;
	ListCell *lc;

	foreach(lc, enc)
	{
		DefElem *el = lfirst(lc);

		if (!el->defname)
			out = lappend(out, copyObject(el));

		if (pg_strcasecmp("appendonly", el->defname) == 0)
			continue;
		if (pg_strcasecmp("orientation", el->defname) == 0)
			continue;
		if (pg_strcasecmp("oids", el->defname) == 0)
			continue;
		if (pg_strcasecmp("fillfactor", el->defname) == 0)
			continue;
		if (pg_strcasecmp("tablename", el->defname) == 0)
			continue;
		/* checksum is not a column specific attribute. */
		if (pg_strcasecmp("checksum", el->defname) == 0)
			continue;
		out = lappend(out, copyObject(el));
	}
	return out;
}

static List *
transformAttributeEncoding(List *stenc, CreateStmt *stmt, CreateStmtContext *cxt)
{
	ListCell *lc;
	bool found_enc = stenc != NIL;
	bool can_enc = is_aocs(stmt->options);
	ColumnReferenceStorageDirective *deflt = NULL;
	List *newenc = NIL;
	List *tmpenc;
	MemoryContext oldCtx;

#define UNSUPPORTED_ORIENTATION_ERROR() \
	ereport(ERROR, \
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED), \
			 errmsg("ENCODING clause only supported with column oriented tables")))

	/* We only support the attribute encoding clause on AOCS tables */
	if (stenc && !can_enc)
		UNSUPPORTED_ORIENTATION_ERROR();

	/* Use the temporary context to avoid leaving behind so much garbage. */
	oldCtx = MemoryContextSwitchTo(cxt->tempCtx);

	/* get the default clause, if there is one. */
	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);
		Insist(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
		{
			/*
			 * Some quick validation: there should only be one default
			 * clause
			 */
			if (deflt)
				elog(ERROR, "only one default column encoding may be specified");

			deflt = copyObject(c);
			deflt->encoding = transformStorageEncodingClause(deflt->encoding);

			/*
			 * The default encoding and the with clause better not
			 * try and set the same options!
			 */
			if (encodings_overlap(stmt->options, deflt->encoding, false))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("DEFAULT COLUMN ENCODING clause cannot "
								"override values set in WITH clause")));
		}
	}

	/*
	 * If no default has been specified, we might create one out of the
	 * WITH clause.
	 */
	tmpenc = form_default_storage_directive(stmt->options);

	if (tmpenc)
	{
		deflt = makeNode(ColumnReferenceStorageDirective);
		deflt->deflt = true;
		deflt->encoding = transformStorageEncodingClause(tmpenc);
	}

	/*
	 * Loop over all columns. If a column has a column reference storage clause
	 * -- i.e., COLUMN name ENCODING () -- apply that. Otherwise, apply the
	 * default.
	 */
	foreach(lc, cxt->columns)
	{
		ColumnDef *d = (ColumnDef *) lfirst(lc);
		ColumnReferenceStorageDirective *c;

		Insist(IsA(d, ColumnDef));

		c = makeNode(ColumnReferenceStorageDirective);
		c->column = pstrdup(d->colname);

		/*
		 * Find a storage encoding for this column, in this order:
		 *
		 * 1. An explicit encoding clause in the ColumnDef
		 * 2. A column reference storage directive for this column
		 * 3. A default column encoding in the statement
		 * 4. A default for the type.
		 */
		if (d->encoding)
		{
			found_enc = true;
			c->encoding = transformStorageEncodingClause(d->encoding);
		}
		else
		{
			ColumnReferenceStorageDirective *s = find_crsd(c->column, stenc);

			if (s)
				c->encoding = transformStorageEncodingClause(s->encoding);
			else
			{
				if (deflt)
					c->encoding = copyObject(deflt->encoding);
				else
				{
					List *te = TypeNameGetStorageDirective(d->typeName);

					if (te)
						c->encoding = copyObject(te);
					else
						c->encoding = default_column_encoding_clause();
				}
			}
		}
		newenc = lappend(newenc, c);
	}

	/* Check again incase we expanded a some column encoding clauses */
	if (!can_enc)
	{
		if (found_enc)
			UNSUPPORTED_ORIENTATION_ERROR();
		else
			newenc = NULL;
	}

	validateColumnStorageEncodingClauses(newenc, stmt);

	/* copy the result out of the temporary memory context */
	MemoryContextSwitchTo(oldCtx);
	newenc = copyObject(newenc);

	return newenc;
}

/*
 * Tells the caller if CO is explicitly disabled, to handle cases where we
 * want to ignore encoding clauses in partition expansion.
 *
 * This is an ugly special case that backup expects to work and since we've got
 * tonnes of dumps out there and the possibility that users have learned this
 * grammar from them, we must continue to support it.
 */
static bool
co_explicitly_disabled(List *opts)
{
	ListCell *lc;

	foreach(lc, opts)
	{
		DefElem *el = lfirst(lc);
		char *arg = NULL;

		/* Argument will be a Value */
		if (!el->arg)
		{
			continue;
		}

		arg = defGetString(el);
		bool result = false;
		if (pg_strcasecmp("appendonly", el->defname) == 0 &&
			pg_strcasecmp("false", arg) == 0)
		{
			result = true;
		}
		else if (pg_strcasecmp("orientation", el->defname) == 0 &&
				 pg_strcasecmp("column", arg) != 0)
		{
			result = true;
		}

		if (result)
		{
			return true;
		}
	}
	return false;
}

/*
 * Tell the caller whether appendonly=true and orientation=column
 * have been specified.
 */
bool
is_aocs(List *opts)
{
	bool found_ao = false;
	bool found_cs = false;
	bool aovalue = false;
	bool csvalue = false;

	ListCell *lc;

	foreach(lc, opts)
	{
		DefElem *el = lfirst(lc);
		char *arg = NULL;

		/* Argument will be a Value */
		if (!el->arg)
		{
			continue;
		}

		arg = defGetString(el);

		if (pg_strcasecmp("appendonly", el->defname) == 0)
		{
			found_ao = true;
			if (!parse_bool(arg, &aovalue))
				elog(ERROR, "invalid value for option \"appendonly\"");
		}
		else if (pg_strcasecmp("orientation", el->defname) == 0)
		{
			found_cs = true;
			csvalue = pg_strcasecmp("column", arg) == 0;
		}
	}
	if (!found_ao)
		aovalue = isDefaultAO();
	if (!found_cs)
		csvalue = isDefaultAOCS();
	return (aovalue && csvalue);
}

/*
 * See if two encodings attempt to see the same parameters. If test_conflicts is
 * true, allow setting the same value, but the setting must be identical.
 */
static bool
encodings_overlap(List *a, List *b, bool test_conflicts)
{
	ListCell *lca;

	foreach(lca, a)
	{
		ListCell *lcb;
		DefElem *ela = lfirst(lca);

		foreach(lcb, b)
		{
			DefElem *elb = lfirst(lcb);

			if (pg_strcasecmp(ela->defname, elb->defname) == 0)
			{
				if (test_conflicts)
				{
					if (!ela->arg && !elb->arg)
						return true;
					else if (!ela->arg || !elb->arg)
					{
						/* skip */
					}
					else
					{
						char *ela_str = defGetString(ela);
						char *elb_str = defGetString(elb);

						if (pg_strcasecmp(ela_str,elb_str) != 0)
							return true;
					}
				}
				else
					return true;
			}
		}
	}
	return false;
}

/*
 * transformAlterTable_all_PartitionStmt -
 *	transform an Alter Table Statement for some Partition operation
 */
static AlterTableCmd *
transformAlterTable_all_PartitionStmt(
		ParseState *pstate,
		AlterTableStmt *stmt,
		CreateStmtContext *pCxt,
		AlterTableCmd *cmd)
{
	AlterPartitionCmd 	*pc   	   = (AlterPartitionCmd *) cmd->def;
	AlterPartitionCmd 	*pci  	   = pc;
	AlterPartitionId  	*pid  	   = (AlterPartitionId *)pci->partid;
	AlterTableCmd 		*atc1 	   = cmd;
	RangeVar 			*rv   	   = stmt->relation;
	PartitionNode 		*pNode 	   = NULL;
	PartitionNode 		*prevNode  = NULL;
	int 			 	 partDepth = 0;
	Oid 			 	 par_oid   = InvalidOid;
	StringInfoData   sid1, sid2;

	if (atc1->subtype == AT_PartAlter)
	{
		PgPartRule* 	 prule = NULL;
		char 			*lrelname;
		Relation 		 rel   = heap_openrv(rv, AccessShareLock);

		initStringInfo(&sid1);
		initStringInfo(&sid2);

		appendStringInfo(&sid1, "relation \"%s\"",
						 RelationGetRelationName(rel));

		lrelname = sid1.data;

		pNode = RelationBuildPartitionDesc(rel, false);

		if (!pNode)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("%s is not partitioned",
							lrelname)));

		/* Processes nested ALTER (if it exists) */
		while (1)
		{
			AlterPartitionId  	*pid2 = NULL;

			if (atc1->subtype != AT_PartAlter)
			{
				rv = makeRangeVar(
						get_namespace_name(
								RelationGetNamespace(rel)),
						pstrdup(RelationGetRelationName(rel)), -1);
				heap_close(rel, AccessShareLock);
				rel = NULL;
				break;
			}

			pid2 = (AlterPartitionId *)pci->partid;

			if (pid2 && (pid2->idtype == AT_AP_IDValue))
			{
				List *vallist = (List *)pid2->partiddef;
				pid2->partiddef =
						(Node *)transformExpressionList(
							pstate, vallist,
							EXPR_KIND_PARTITION_EXPRESSION);
			}

			partDepth++;

			if (!pNode)
				prule = NULL;
			else
				prule = get_part_rule1(rel,
									   pid2,
									   false, true,
									   NULL,
									   pNode,
									   sid1.data, NULL);

			if (prule && prule->topRule &&
				prule->topRule->children)
			{
				prevNode = pNode;
				pNode = prule->topRule->children;
				par_oid = RelationGetRelid(rel);

				/*
				 * Don't hold a long lock -- lock on the master is
				 * sufficient
				 */
				heap_close(rel, AccessShareLock);
				rel = heap_open(prule->topRule->parchildrelid,
								AccessShareLock);

				appendStringInfo(&sid2, "partition%s of %s",
								 prule->partIdStr, sid1.data);
				truncateStringInfo(&sid1, 0);
				appendStringInfo(&sid1, "%s", sid2.data);
				truncateStringInfo(&sid2, 0);
			}
			else
			{
				prevNode = pNode;
				pNode = NULL;
			}

			atc1 = (AlterTableCmd *)pci->arg1;
			pci = (AlterPartitionCmd *)atc1->def;
		} /* end while */
		if (rel)
			/* No need to hold onto the lock -- see above */
			heap_close(rel, AccessShareLock);
	} /* end if alter */

	switch (atc1->subtype)
	{
		case AT_PartAdd:				/* Add */
		case AT_PartSetTemplate:		/* Set Subpartn Template */
		case AT_PartDrop:				/* Drop */
		case AT_PartExchange:			/* Exchange */
		case AT_PartRename:				/* Rename */
		case AT_PartTruncate:			/* Truncate */
		case AT_PartSplit:				/* Split */
			/* MPP-4011: get right pid for FOR(value) */
			pid  	   = (AlterPartitionId *)pci->partid;
			if (pid && (pid->idtype == AT_AP_IDValue))
			{
				List *vallist = (List *)pid->partiddef;
				pid->partiddef =
						(Node *)transformExpressionList(
							pstate, vallist,
							EXPR_KIND_PARTITION_EXPRESSION);
			}
	break;
		default:
			break;
	}
	/* transform boundary specifications at execute time */
	return cmd;
} /* end transformAlterTable_all_PartitionStmt */
