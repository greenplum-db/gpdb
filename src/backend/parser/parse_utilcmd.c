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
 *	$PostgreSQL: pgsql/src/backend/parser/parse_utilcmd.c,v 2.20 2009/01/01 17:23:46 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/reloptions.h"
#include "catalog/catquery.h"
#include "catalog/pg_constraint.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
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
#include "parser/parse_relation.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "rewrite/rewriteManip.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/relcache.h"
#include "utils/syscache.h"

static IndexStmt *generateClonedIndexStmt(CreateStmtContext *cxt,
						Relation parent_index, AttrNumber *attmap);
static List *get_opclass(Oid opclass, Oid actual_datatype);
static IndexStmt *transformIndexConstraint(Constraint *constraint,
						 CreateStmtContext *cxt);

/*
 * Generate an IndexStmt node using information from an already existing index
 * "source_idx".  Attribute numbers should be adjusted according to attmap.
 */
static IndexStmt *
generateClonedIndexStmt(CreateStmtContext *cxt, Relation source_idx,
						AttrNumber *attmap)
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
	cqContext  *pcqCtx;

	/*
	 * Fetch pg_class tuple of source index.  We can't use the copy in the
	 * relcache entry because it doesn't include optional fields.
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(source_relid)));

	ht_idxrel = caql_getnext(pcqCtx);

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
		
	/* If the index backs a constraint and we are adding a part to a paritioned
	 * table, we need to use the same constraint name as the partition root uses.  
	 * We assume that the inheritance hierarchy above is consistent, so we just 
	 * use the parent's constraint. 
	 */
	if (index->isconstraint && cxt->isaddpart )
	{
		char *conname;
		
		/*  */
		if ( ! OidIsValid(conoid) )
			conoid = get_index_constraint(source_relid);
			
		conname = GetConstraintNameByOid(conoid);
		
		if ( ! conname )
		{
			elog(ERROR, "missing constraint on partitioned table");
		}
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

			if (indexpr_item == NULL)
				elog(ERROR, "too few entries in indexprs list");
			indexkey = (Node *) lfirst(indexpr_item);
			indexpr_item = lnext(indexpr_item);

			/* OK to modify indexkey since we are working on a private copy */
			change_varattnos_of_a_node(indexkey, attmap);

			iparam->name = NULL;
			iparam->expr = indexkey;

			keycoltype = exprType(indexkey);
		}

		/* Add the operator class name, if non-default */
		iparam->opclass = get_opclass(indclass->values[keyno], keycoltype);

		index->indexParams = lappend(index->indexParams, iparam);
	}

	/* Copy reloptions if any */
	datum = caql_getattr(pcqCtx,
						 Anum_pg_class_reloptions, &isnull);
	if (!isnull)
		index->options = untransformRelOptions(datum);

	/* If it's a partial index, decompile and append the predicate */
	datum = SysCacheGetAttr(INDEXRELID, ht_idx,
							Anum_pg_index_indpred, &isnull);
	if (!isnull)
	{
		char	   *pred_str;

		/* Convert text string to node tree */
		pred_str = TextDatumGetCString(datum);
		index->whereClause = (Node *) stringToNode(pred_str);
		/* Adjust attribute numbers */
		change_varattnos_of_a_node(index->whereClause, attmap);
	}

	/* Clean up */
	caql_endscan(pcqCtx);

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
	cqContext  *pcqCtx;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_opclass "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opclass)));

	ht_opc = caql_getnext(pcqCtx);

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

	caql_endscan(pcqCtx);

	return result;
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

	/* We used to force the index name to be the constraint name, but they are 
	 * in different namespaces and so have different  requirements for uniqueness.  
	 * Here we leave the index name alone and put the constaint name in the IndexStmt 
	 * for use at a lower level.
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
		index->indexParams = lappend(index->indexParams, iparam);
	}

	return index;
}

/*
 * transformInhRelation
 *
 * Change the LIKE <subtable> portion of a CREATE TABLE statement into the
 * column definitions which recreate the user defined column portions of <subtable>.
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
	AclResult	aclresult;
	bool		including_defaults = false;
	bool		including_constraints = false;
	bool		including_indexes = false;
	ListCell   *elem;

	relation = heap_openrv(inhRelation->relation, AccessShareLock);

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
	 * Insert the copied attributes into the cxt for the new table
	 * definition.
	 */
	for (parent_attno = 1; parent_attno <= tupleDesc->natts;
		 parent_attno++)
	{
		Form_pg_attribute attribute = tupleDesc->attrs[parent_attno - 1];
		char	   *attributeName = NameStr(attribute->attname);
		ColumnDef  *def;

		/*
		 * Ignore dropped columns in the parent.
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
		def->typname = makeTypeNameFromOid(attribute->atttypid,
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
	 * Copy CHECK constraints if requested, being careful to adjust
	 * attribute numbers
	 */
	if (including_constraints && tupleDesc->constr)
	{
		AttrNumber *attmap = varattnos_map_schema(tupleDesc, cxt->columns);
		int			ccnum;

		for (ccnum = 0; ccnum < tupleDesc->constr->num_check; ccnum++)
		{
			char	   *ccname = tupleDesc->constr->check[ccnum].ccname;
			char	   *ccbin = tupleDesc->constr->check[ccnum].ccbin;
			Node	   *ccbin_node = stringToNode(ccbin);
			Constraint *n = makeNode(Constraint);

			change_varattnos_of_a_node(ccbin_node, attmap);

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
		AttrNumber *attmap = varattnos_map_schema(tupleDesc, cxt->columns);
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
			index_stmt = generateClonedIndexStmt(cxt, parent_index, attmap);

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
 * transformIndexConstraints
 *		Handle UNIQUE and PRIMARY KEY constraints, which create indexes.
 *		We also merge in any index definitions arising from
 *		LIKE ... INCLUDING INDEXES.
 */
void
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
