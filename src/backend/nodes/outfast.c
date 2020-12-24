/*-------------------------------------------------------------------------
 *
 * outfast.c
 *	  Fast serialization functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * NOTES
 *	  Every node type that can appear in an Greenplum Database serialized query or plan
 *    tree must have an output function defined here.
 *
 * 	  There *MUST* be a one-to-one correspondence between this routine
 *    and readfast.c.  If not, you will likely crash the system.
 *
 *     By design, the only user of these routines is the function
 *     serializeNode in cdbsrlz.c.  Other callers beware.
 *
 *    Like readfast.c, this file borrows the definitions of most functions
 *    from outfuncs.c.
 *
 * 	  Rather than serialize to a (somewhat human-readable) string, these
 *    routines create a binary serialization via a simple depth-first walk
 *    of the tree.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/params.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "utils/datum.h"
#include "catalog/heap.h"
#include "cdb/cdbgang.h"
#include "utils/workfile_mgr.h"
#include "parser/parsetree.h"

/*
 * Macros to simplify output of different kinds of fields.	Use these
 * wherever possible to reduce the chance for silly typos.	Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/*
 * Write the label for the node type.  nodelabel is accepted for
 * compatibility with outfuncs.c, but is ignored
 */
#define WRITE_NODE_TYPE(nodelabel) \
	{ int16 nt =nodeTag(node); appendBinaryStringInfo(str, (const char *)&nt, sizeof(int16)); }

/* Write an integer field  */
#define WRITE_INT_FIELD(fldname) \
	{ appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int)); }

/* Write an integer field  */
#define WRITE_INT8_FIELD(fldname) \
	{ appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int8)); }

/* Write an integer field  */
#define WRITE_INT16_FIELD(fldname) \
	{ appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int16)); }

/* Write an unsigned integer field */
#define WRITE_UINT_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int))

/* Write an uint64 field */
#define WRITE_UINT64_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(uint64))

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(Oid))

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(long))

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) \
	appendBinaryStringInfo(str, &node->fldname, 1)

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) \
	{ int16 en=node->fldname; appendBinaryStringInfo(str, (const char *)&en, sizeof(int16)); }

/* Write a float field --- the format is accepted but ignored (for compat with outfuncs.c)  */
#define WRITE_FLOAT_FIELD(fldname,format) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(double))

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) \
	{ \
		char b = node->fldname ? 1 : 0; \
		appendBinaryStringInfo(str, (const char *)&b, 1); }

/* Write a character-string (possibly NULL) varable */
#define WRITE_STRING_VAR(var) \
	{ int slen = var != NULL ? strlen(var) : 0; \
		appendBinaryStringInfo(str, (const char *)&slen, sizeof(int)); \
		if (slen>0) appendBinaryStringInfo(str, var, slen);}

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname)  WRITE_STRING_VAR(node->fldname)

/* Write a parse location field (actually same as INT case) */
#define WRITE_LOCATION_FIELD(fldname) \
	{ appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int)); }

/*
 * Write a Node field
 *
 * If compiled with GP_SERIALIZATION_DEBUG, write the field name in the
 * serialized form, and check that it matches in the read function
 * (READ_NODE_FIELD in readfast.c). That makes it much easier to debug bugs
 * where the out and read functions are not in sync, as you get an error
 * much earlier, and it can print the field name where the mismatch occurred.
 * It makes the serialized plans much larger, though, so we don't want to do
 * it production.
 */
#ifdef GP_SERIALIZATION_DEBUG
#define WRITE_NODE_FIELD(fldname) \
	do { \
		const char *xx = CppAsString(fldname); \
		appendBinaryStringInfo(str, xx, strlen(xx) + 1); \
		(_outNode(str, node->fldname)); \
	} while (0)
#else
#define WRITE_NODE_FIELD(fldname) \
	(_outNode(str, node->fldname))
#endif


/* Write a bitmapset field */
#define WRITE_BITMAPSET_FIELD(fldname) \
	 _outBitmapset(str, node->fldname)

/* Write a binary field */
#define WRITE_BINARY_FIELD(fldname, sz) \
{ appendBinaryStringInfo(str, (const char *) &node->fldname, (sz)); }

/* Write a bytea field */
#define WRITE_BYTEA_FIELD(fldname) \
	(_outDatum(str, PointerGetDatum(node->fldname), -1, false))

/* Write a dummy field -- value not displayable or copyable */
#define WRITE_DUMMY_FIELD(fldname) \
	{ /*int * dummy = 0; appendBinaryStringInfo(str,(const char *)&dummy, sizeof(int *)) ;*/ }

	/* Read an integer array */
#define WRITE_INT_ARRAY(fldname, count) \
	StaticAssertStmt(sizeof(node->fldname[0]) == sizeof(int32), \
					 "WRITE_INT_ARRAY() used on wrong array type"); \
	if ( (count) > 0 ) \
	{ \
		int i; \
		for(i = 0; i < (count); i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(int32)); \
		} \
	}

/* Write a boolean array  */
#define WRITE_BOOL_ARRAY(fldname, count) \
	if ( (count) > 0 ) \
	{ \
		int i; \
		for(i = 0; i < (count); i++) \
		{ \
			char b = node->fldname[i] ? 1 : 0;								\
			appendBinaryStringInfo(str, (const char *)&b, 1); \
		} \
	}

/* Write an Trasnaction ID array  */
#define WRITE_XID_ARRAY(fldname, count) \
	if ( (count) > 0 ) \
	{ \
		int i; \
		for(i = 0; i < (count); i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(TransactionId)); \
		} \
	}

/* Write an AttrNumber array  */
#define WRITE_ATTRNUMBER_ARRAY(fldname, count) \
	if ( (count) > 0 ) \
	{ \
		int i; \
		for(i = 0; i < (count); i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(AttrNumber)); \
		} \
	}

/* Write an Oid array  */
#define WRITE_OID_ARRAY(fldname, count) \
	if ( (count) > 0 ) \
	{ \
		int i; \
		for(i = 0; i < (count); i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(Oid)); \
		} \
	}

static void _outNode(StringInfo str, void *obj);

#define outDatum(str, value, typlen, typbyval) _outDatum(str, value, typlen, typbyval)

static void
_outList(StringInfo str, List *node)
{
	ListCell   *lc;

	if (node == NULL)
	{
		int16 tg = 0;
		appendBinaryStringInfo(str, (const char *)&tg, sizeof(int16));
		return;
	}

	WRITE_NODE_TYPE("");
    WRITE_INT_FIELD(length);

	foreach(lc, node)
	{

		if (IsA(node, List))
		{
			_outNode(str, lfirst(lc));
		}
		else if (IsA(node, IntList))
		{
			int n = lfirst_int(lc);
			appendBinaryStringInfo(str, (const char *)&n, sizeof(int));
		}
		else if (IsA(node, OidList))
		{
			Oid n = lfirst_oid(lc);
			appendBinaryStringInfo(str, (const char *)&n, sizeof(Oid));
		}
	}
}

/*
 * _outBitmapset -
 *	   converts a bitmap set of integers
 *
 * Currently bitmapsets do not appear in any node type that is stored in
 * rules, so there is no support in readfast.c for reading this format.
 */
static void
_outBitmapset(StringInfo str, const Bitmapset *bms)
{
	int			i;
	int			nwords = 0;

	if (bms)
		nwords = bms->nwords;

	appendBinaryStringInfo(str, (char *) &nwords, sizeof(int));
	for (i = 0; i < nwords; i++)
	{
		appendBinaryStringInfo(str, (char *) &bms->words[i], sizeof(bitmapword));
	}

}

/*
 * Print the value of a Datum given its type.
 */
static void
_outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
	Size		length;
	char	   *s;

	if (typbyval)
	{
		s = (char *) (&value);
		appendBinaryStringInfo(str, s, sizeof(Datum));
	}
	else
	{
		s = (char *) DatumGetPointer(value);
		if (!PointerIsValid(s))
		{
			length = 0;
			appendBinaryStringInfo(str, (char *)&length, sizeof(Size));
		}
		else
		{
			if (typlen == -1 && VARATT_IS_EXTERNAL_EXPANDED(s))
			{
				ExpandedObjectHeader *eoh = DatumGetEOHP(value);
				Size		resultsize;
				char		*resultptr;

				resultsize = EOH_get_flat_size(eoh);
				resultptr = (char *) palloc(resultsize);
				EOH_flatten_into(eoh, (void *) resultptr, resultsize);
				appendBinaryStringInfo(str, (char *)&resultsize, sizeof(Size));
				appendBinaryStringInfo(str, resultptr, resultsize);
			}
			else
			{
				length = datumGetSize(value, typbyval, typlen);
				appendBinaryStringInfo(str, (char *)&length, sizeof(Size));
				appendBinaryStringInfo(str, s, length);
			}
		}
	}
}

#define COMPILING_BINARY_FUNCS
#include "outfuncs.c"

static void
_outCopyStmt(StringInfo str, CopyStmt *node)
{
	WRITE_NODE_TYPE("COPYSTMT");
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(attlist);
	WRITE_BOOL_FIELD(is_from);
	WRITE_BOOL_FIELD(is_program);
	WRITE_STRING_FIELD(filename);
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(sreh);
}

/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

static void
_outConst(StringInfo str, Const *node)
{
	WRITE_NODE_TYPE("CONST");

	WRITE_OID_FIELD(consttype);
	WRITE_INT_FIELD(consttypmod);
	WRITE_OID_FIELD(constcollid);
	WRITE_INT_FIELD(constlen);
	WRITE_BOOL_FIELD(constbyval);
	WRITE_BOOL_FIELD(constisnull);
	WRITE_LOCATION_FIELD(location);

	if (!node->constisnull)
		_outDatum(str, node->constvalue, node->constlen, node->constbyval);
}

static void
_outBoolExpr(StringInfo str, BoolExpr *node)
{
	WRITE_NODE_TYPE("BOOLEXPR");
	WRITE_ENUM_FIELD(boolop, BoolExprType);

	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCurrentOfExpr(StringInfo str, CurrentOfExpr *node)
{
	WRITE_NODE_TYPE("CURRENTOFEXPR");

	WRITE_UINT_FIELD(cvarno);
	WRITE_STRING_FIELD(cursor_name);
	WRITE_INT_FIELD(cursor_param);
	WRITE_OID_FIELD(target_relid);
}

static void
_outJoinExpr(StringInfo str, JoinExpr *node)
{
	WRITE_NODE_TYPE("JOINEXPR");

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_BOOL_FIELD(isNatural);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(usingClause);
	WRITE_NODE_FIELD(quals);
	WRITE_NODE_FIELD(alias);
	WRITE_INT_FIELD(rtindex);
}

/*****************************************************************************
 *
 *	Stuff from extensible.h
 *
 *****************************************************************************/

static void
_outExtensibleNode(StringInfo str, const ExtensibleNode *node)
{
	const ExtensibleNodeMethods *methods;
	StringInfoData buf;
	initStringInfo(&buf);

	methods = GetExtensibleNodeMethods(node->extnodename, false);

	WRITE_NODE_TYPE("EXTENSIBLENODE");

	WRITE_STRING_FIELD(extnodename);

	/* serialize the private fields */
	methods->nodeOut(&buf, node);

	WRITE_STRING_VAR(buf.data);
}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

static void
_outCreateExtensionStmt(StringInfo str, CreateExtensionStmt *node)
{
	WRITE_NODE_TYPE("CREATEEXTENSIONSTMT");
	WRITE_STRING_FIELD(extname);
	WRITE_BOOL_FIELD(if_not_exists);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(create_ext_state, CreateExtensionState);
}

static void
_outRoleSpec(StringInfo str, const RoleSpec *node)
{
	WRITE_NODE_TYPE("ROLESPEC");

	WRITE_ENUM_FIELD(roletype, RoleSpecType);
	WRITE_STRING_FIELD(rolename);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCreateDomainStmt(StringInfo str, CreateDomainStmt *node)
{
	WRITE_NODE_TYPE("CREATEDOMAINSTMT");
	WRITE_NODE_FIELD(domainname);
	WRITE_NODE_FIELD(typeName);
	WRITE_NODE_FIELD(constraints);
}

static void
_outAlterDomainStmt(StringInfo str, AlterDomainStmt *node)
{
	WRITE_NODE_TYPE("ALTERDOMAINSTMT");
	WRITE_CHAR_FIELD(subtype);
	WRITE_NODE_FIELD(typeName);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(def);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outAlterDefaultPrivilegesStmt(StringInfo str, AlterDefaultPrivilegesStmt *node)
{
	WRITE_NODE_TYPE("ALTERDEFAULTPRIVILEGESSTMT");
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(action);
}

static void
_outQuery(StringInfo str, Query *node)
{
	WRITE_NODE_TYPE("QUERY");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(querySource, QuerySource);
	WRITE_BOOL_FIELD(canSetTag);

	WRITE_NODE_FIELD(utilityStmt);
	WRITE_INT_FIELD(resultRelation);
	WRITE_BOOL_FIELD(hasAggs);
	WRITE_BOOL_FIELD(hasWindowFuncs);
	WRITE_BOOL_FIELD(hasSubLinks);
	WRITE_BOOL_FIELD(hasDynamicFunctions);
	WRITE_BOOL_FIELD(hasFuncsWithExecRestrictions);
	WRITE_BOOL_FIELD(hasDistinctOn);
	WRITE_BOOL_FIELD(hasRecursive);
	WRITE_BOOL_FIELD(hasModifyingCTE);
	WRITE_BOOL_FIELD(hasForUpdate);
	WRITE_BOOL_FIELD(hasRowSecurity);
	WRITE_BOOL_FIELD(canOptSelectLockingClause);
	WRITE_NODE_FIELD(cteList);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(jointree);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(withCheckOptions);
	WRITE_NODE_FIELD(onConflict);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(groupingSets);
	WRITE_NODE_FIELD(havingQual);
	WRITE_NODE_FIELD(windowClause);
	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(scatterClause);
	WRITE_BOOL_FIELD(isTableValueSelect);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(setOperations);
	WRITE_NODE_FIELD(constraintDeps);
	WRITE_BOOL_FIELD(parentStmtType);

	/* Don't serialize policy */
}

static void
_outAExpr(StringInfo str, A_Expr *node)
{
	WRITE_NODE_TYPE("AEXPR");
	WRITE_ENUM_FIELD(kind, A_Expr_Kind);

	switch (node->kind)
	{
		case AEXPR_OP:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_OP_ANY:

			WRITE_NODE_FIELD(name);

			break;
		case AEXPR_OP_ALL:

			WRITE_NODE_FIELD(name);

			break;
		case AEXPR_DISTINCT:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NULLIF:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_OF:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_IN:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_LIKE:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_ILIKE:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_SIMILAR:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_BETWEEN:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NOT_BETWEEN:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_BETWEEN_SYM:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NOT_BETWEEN_SYM:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_PAREN:

			break;

		default:

			break;
	}

	WRITE_NODE_FIELD(lexpr);
	WRITE_NODE_FIELD(rexpr);
	WRITE_LOCATION_FIELD(location);
}

static void
_outValue(StringInfo str, Value *value)
{

	int16 vt = value->type;
	appendBinaryStringInfo(str, (const char *)&vt, sizeof(int16));
	switch (value->type)
	{
		case T_Integer:
			appendBinaryStringInfo(str, (const char *)&value->val.ival, sizeof(long));
			break;
		case T_Float:
		case T_String:
		case T_BitString:
			{
				int slen = (value->val.str != NULL ? strlen(value->val.str) : 0);
				appendBinaryStringInfo(str, (const char *)&slen, sizeof(int));
				if (slen > 0)
					appendBinaryStringInfo(str, value->val.str, slen);
			}
			break;
		case T_Null:
			/* nothing to do */
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) value->type);
			break;
	}
}

static void
_outAConst(StringInfo str, A_Const *node)
{
	WRITE_NODE_TYPE("A_CONST");

	_outValue(str, &(node->val));
	WRITE_LOCATION_FIELD(location);  /*CDB*/

}

static void
_outCreateQueueStmt(StringInfo str, CreateQueueStmt *node)
{
	WRITE_NODE_TYPE("CREATEQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}

static void
_outAlterQueueStmt(StringInfo str, AlterQueueStmt *node)
{
	WRITE_NODE_TYPE("ALTERQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}

static void
_outCreateResourceGroupStmt(StringInfo str, CreateResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("CREATERESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}

static void
_outAlterResourceGroupStmt(StringInfo str, AlterResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("ALTERRESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}

/*
 * Support for serializing TupleDescs and ParamListInfos.
 *
 * TupleDescs and ParamListInfos are not Nodes as such, but if you wrap them
 * in TupleDescNode and ParamListInfoNode structs, we allow serializing them.
 */
static void
_outTupleDescNode(StringInfo str, TupleDescNode *node)
{
	int			i;

	Assert(node->tuple->tdtypeid == RECORDOID);

	WRITE_NODE_TYPE("TUPLEDESCNODE");
	WRITE_INT_FIELD(natts);
	WRITE_INT_FIELD(tuple->natts);

	for (i = 0; i < node->tuple->natts; i++)
		appendBinaryStringInfo(str, (char *) &node->tuple->attrs[i], ATTRIBUTE_FIXED_PART_SIZE);

	Assert(node->tuple->constr == NULL);

	WRITE_OID_FIELD(tuple->tdtypeid);
	WRITE_INT_FIELD(tuple->tdtypmod);
	WRITE_INT_FIELD(tuple->tdrefcount);
}

static void
_outCookedConstraint(StringInfo str, CookedConstraint *node)
{
	WRITE_NODE_TYPE("COOKEDCONSTRAINT");

	WRITE_ENUM_FIELD(contype,ConstrType);
	WRITE_STRING_FIELD(name);
	WRITE_INT_FIELD(attnum);
	WRITE_NODE_FIELD(expr);
	WRITE_BOOL_FIELD(is_local);
	WRITE_INT_FIELD(inhcount);
	WRITE_BOOL_FIELD(is_no_inherit);
}

static void
_outAlterEnumStmt(StringInfo str, AlterEnumStmt *node)
{
	WRITE_NODE_TYPE("ALTERENUMSTMT");

	WRITE_NODE_FIELD(typeName);
	WRITE_STRING_FIELD(oldVal);
	WRITE_STRING_FIELD(newVal);
	WRITE_STRING_FIELD(newValNeighbor);
	WRITE_BOOL_FIELD(newValIsAfter);
	WRITE_BOOL_FIELD(skipIfNewValExists);
}

static void
_outCreateFdwStmt(StringInfo str, CreateFdwStmt *node)
{
	WRITE_NODE_TYPE("CREATEFDWSTMT");

	WRITE_STRING_FIELD(fdwname);
	WRITE_NODE_FIELD(func_options);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterFdwStmt(StringInfo str, AlterFdwStmt *node)
{
	WRITE_NODE_TYPE("ALTERFDWSTMT");

	WRITE_STRING_FIELD(fdwname);
	WRITE_NODE_FIELD(func_options);
	WRITE_NODE_FIELD(options);
}

static void
_outCreateForeignServerStmt(StringInfo str, CreateForeignServerStmt *node)
{
	WRITE_NODE_TYPE("CREATEFOREIGNSERVERSTMT");

	WRITE_STRING_FIELD(servername);
	WRITE_STRING_FIELD(servertype);
	WRITE_STRING_FIELD(version);
	WRITE_STRING_FIELD(fdwname);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterForeignServerStmt(StringInfo str, AlterForeignServerStmt *node)
{
	WRITE_NODE_TYPE("ALTERFOREIGNSERVERSTMT");

	WRITE_STRING_FIELD(servername);
	WRITE_STRING_FIELD(version);
	WRITE_NODE_FIELD(options);
	WRITE_BOOL_FIELD(has_version);
}

static void
_outCreateUserMappingStmt(StringInfo str, CreateUserMappingStmt *node)
{
	WRITE_NODE_TYPE("CREATEUSERMAPPINGSTMT");

	WRITE_NODE_FIELD(user);
	WRITE_STRING_FIELD(servername);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterUserMappingStmt(StringInfo str, AlterUserMappingStmt *node)
{
	WRITE_NODE_TYPE("ALTERUSERMAPPINGSTMT");

	WRITE_NODE_FIELD(user);
	WRITE_STRING_FIELD(servername);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterObjectDependsStmt(StringInfo str, const AlterObjectDependsStmt *node)
{
	WRITE_NODE_TYPE("ALTEROBJECTDEPENDSSTMT");

	WRITE_ENUM_FIELD(objectType,ObjectType);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(extname);
}

static void
_outCustomScan(StringInfo str, const CustomScan *node)
{
	WRITE_NODE_TYPE("CUSTOMSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_UINT_FIELD(flags);
	WRITE_NODE_FIELD(custom_plans);
	WRITE_NODE_FIELD(custom_exprs);
	WRITE_NODE_FIELD(custom_private);
	WRITE_NODE_FIELD(custom_scan_tlist);
	WRITE_BITMAPSET_FIELD(custom_relids);
	WRITE_STRING_FIELD(methods->CustomName);	
}

static void
_outDropUserMappingStmt(StringInfo str, DropUserMappingStmt *node)
{
	WRITE_NODE_TYPE("DROPUSERMAPPINGSTMT");

	WRITE_NODE_FIELD(user);
	WRITE_STRING_FIELD(servername);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outAccessPriv(StringInfo str, AccessPriv *node)
{
	WRITE_NODE_TYPE("ACCESSPRIV");

	WRITE_STRING_FIELD(priv_name);
	WRITE_NODE_FIELD(cols);
}

static void
_outGpPolicy(StringInfo str, GpPolicy *node)
{
	WRITE_NODE_TYPE("GPPOLICY");

	WRITE_ENUM_FIELD(ptype, GpPolicyType);
	WRITE_INT_FIELD(numsegments);
	WRITE_INT_FIELD(nattrs);
	WRITE_ATTRNUMBER_ARRAY(attrs, node->nattrs);
	WRITE_OID_ARRAY(opclasses, node->nattrs);
}

static void
_outAlterTableMoveAllStmt(StringInfo str, AlterTableMoveAllStmt *node)
{
	WRITE_NODE_TYPE("ALTERTABLESPACEMOVESTMT");

	WRITE_STRING_FIELD(orig_tablespacename);
	WRITE_ENUM_FIELD(objtype, ObjectType);
	WRITE_NODE_FIELD(roles);
	WRITE_STRING_FIELD(new_tablespacename);
	WRITE_BOOL_FIELD(nowait);
}

static void
_outAlterTableSpaceOptionsStmt(StringInfo str, AlterTableSpaceOptionsStmt *node)
{
	WRITE_NODE_TYPE("ALTERTABLESPACEOPTIONS");

	WRITE_STRING_FIELD(tablespacename);
	WRITE_NODE_FIELD(options);
	WRITE_BOOL_FIELD(isReset);
}

static void
_outCreateAmStmt(StringInfo str, const CreateAmStmt *node)
{
	WRITE_NODE_TYPE("CREATEAMSTMT");

	WRITE_STRING_FIELD(amname);
	WRITE_NODE_FIELD(handler_name);
	WRITE_INT_FIELD(amtype);
}
static void
_outAggExprId(StringInfo str, const AggExprId *node)
{
	WRITE_NODE_TYPE("AGGEXPRID");
}
static void
_outRowIdExpr(StringInfo str, const RowIdExpr *node)
{
	WRITE_NODE_TYPE("ROWIDEXPR");

	WRITE_INT_FIELD(rowidexpr_id);
}

/*
 * _outNode -
 *	  converts a Node into binary string and append it to 'str'
 */
static void
_outNode(StringInfo str, void *obj)
{
	if (obj == NULL)
	{
		int16 tg = 0;
		appendBinaryStringInfo(str, (const char *)&tg, sizeof(int16));
	}
	else if (IsA(obj, List) ||IsA(obj, IntList) || IsA(obj, OidList))
		_outList(str, obj);
	else if (IsA(obj, Integer) ||
			 IsA(obj, Float) ||
			 IsA(obj, String) ||
			 IsA(obj, Null) ||
			 IsA(obj, BitString))
	{
		_outValue(str, obj);
	}
	else
	{
		switch (nodeTag(obj))
		{
			case T_PlannedStmt:
				_outPlannedStmt(str,obj);
				break;
			case T_QueryDispatchDesc:
				_outQueryDispatchDesc(str,obj);
				break;
			case T_OidAssignment:
				_outOidAssignment(str,obj);
				break;
			case T_Plan:
				_outPlan(str, obj);
				break;
			case T_Result:
				_outResult(str, obj);
				break;
			case T_ProjectSet:
				_outProjectSet(str, obj);
				break;
			case T_ModifyTable:
				_outModifyTable(str, obj);
				break;
			case T_Append:
				_outAppend(str, obj);
				break;
			case T_MergeAppend:
				_outMergeAppend(str, obj);
				break;
			case T_Sequence:
				_outSequence(str, obj);
				break;
			case T_RecursiveUnion:
				_outRecursiveUnion(str, obj);
				break;
			case T_BitmapAnd:
				_outBitmapAnd(str, obj);
				break;
			case T_BitmapOr:
				_outBitmapOr(str, obj);
				break;
			case T_Gather:
				_outGather(str, obj);
				break;
			case T_GatherMerge:
				_outGatherMerge(str, obj);
				break;
			case T_Scan:
				_outScan(str, obj);
				break;
			case T_SeqScan:
				_outSeqScan(str, obj);
				break;
			case T_DynamicSeqScan:
				_outDynamicSeqScan(str, obj);
				break;
			case T_SampleScan:
				_outSampleScan(str, obj);
				break;
			case T_CteScan:
				_outCteScan(str, obj);
				break;
			case T_NamedTuplestoreScan:
				_outNamedTuplestoreScan(str, obj);
				break;
			case T_WorkTableScan:
				_outWorkTableScan(str, obj);
				break;
			case T_ForeignScan:
				_outForeignScan(str, obj);
				break;
			case T_CustomScan:
				_outCustomScan(str, obj);
				break;
			case T_ExternalScanInfo:
				_outExternalScanInfo(str, obj);
				break;
			case T_IndexScan:
				_outIndexScan(str, obj);
				break;
			case T_IndexOnlyScan:
				_outIndexOnlyScan(str, obj);
				break;
			case T_DynamicIndexScan:
				_outDynamicIndexScan(str, obj);
				break;
			case T_BitmapIndexScan:
				_outBitmapIndexScan(str, obj);
				break;
			case T_DynamicBitmapIndexScan:
				_outDynamicBitmapIndexScan(str, obj);
				break;
			case T_BitmapHeapScan:
				_outBitmapHeapScan(str, obj);
				break;
			case T_DynamicBitmapHeapScan:
				_outDynamicBitmapHeapScan(str, obj);
				break;
			case T_TidScan:
				_outTidScan(str, obj);
				break;
			case T_SubqueryScan:
				_outSubqueryScan(str, obj);
				break;
			case T_FunctionScan:
				_outFunctionScan(str, obj);
				break;
			case T_TableFuncScan:
				_outTableFuncScan(str, obj);
				break;
			case T_ValuesScan:
				_outValuesScan(str, obj);
				break;
			case T_Join:
				_outJoin(str, obj);
				break;
			case T_NestLoop:
				_outNestLoop(str, obj);
				break;
			case T_MergeJoin:
				_outMergeJoin(str, obj);
				break;
			case T_HashJoin:
				_outHashJoin(str, obj);
				break;
			case T_Agg:
				_outAgg(str, obj);
				break;
			case T_TupleSplit:
				_outTupleSplit(str, obj);
				break;
			case T_DQAExpr:
				_outDQAExpr(str, obj);
				break;
			case T_WindowAgg:
				_outWindowAgg(str, obj);
				break;
			case T_TableFunctionScan:
				_outTableFunctionScan(str, obj);
				break;
			case T_Material:
				_outMaterial(str, obj);
				break;
			case T_ShareInputScan:
				_outShareInputScan(str, obj);
				break;
			case T_Sort:
				_outSort(str, obj);
				break;
			case T_Unique:
				_outUnique(str, obj);
				break;
			case T_SetOp:
				_outSetOp(str, obj);
				break;
			case T_LockRows:
				_outLockRows(str, obj);
				break;
			case T_Limit:
				_outLimit(str, obj);
				break;
			case T_NestLoopParam:
				_outNestLoopParam(str, obj);
				break;
			case T_PlanRowMark:
				_outPlanRowMark(str, obj);
				break;
			case T_PartitionPruneInfo:
				_outPartitionPruneInfo(str, obj);
				break;
			case T_PartitionedRelPruneInfo:
				_outPartitionedRelPruneInfo(str, obj);
				break;
			case T_PartitionPruneStepOp:
				_outPartitionPruneStepOp(str, obj);
				break;
			case T_PartitionPruneStepCombine:
				_outPartitionPruneStepCombine(str, obj);
				break;
			case T_Hash:
				_outHash(str, obj);
				break;
			case T_Motion:
				_outMotion(str, obj);
				break;
			case T_SplitUpdate:
				_outSplitUpdate(str, obj);
				break;
			case T_AssertOp:
				_outAssertOp(str, obj);
				break;
			case T_PartitionSelector:
				_outPartitionSelector(str, obj);
				break;
			case T_Alias:
				_outAlias(str, obj);
				break;
			case T_RangeVar:
				_outRangeVar(str, obj);
				break;
			case T_TableFunc:
				_outTableFunc(str, obj);
				break;
			case T_IntoClause:
				_outIntoClause(str, obj);
				break;
			case T_CopyIntoClause:
				_outCopyIntoClause(str, obj);
				break;
			case T_RefreshClause:
				_outRefreshClause(str, obj);
				break;
			case T_Var:
				_outVar(str, obj);
				break;
			case T_Const:
				_outConst(str, obj);
				break;
			case T_Param:
				_outParam(str, obj);
				break;
			case T_Aggref:
				_outAggref(str, obj);
				break;
			case T_GroupingFunc:
				_outGroupingFunc(str, obj);
				break;
			case T_GroupId:
				_outGroupId(str, obj);
				break;
			case T_GroupingSetId:
				_outGroupingSetId(str, obj);
				break;
			case T_WindowFunc:
				_outWindowFunc(str, obj);
				break;
			case T_SubscriptingRef:
				_outSubscriptingRef(str, obj);
				break;
			case T_FuncExpr:
				_outFuncExpr(str, obj);
				break;
			case T_NamedArgExpr:
				_outNamedArgExpr(str, obj);
				break;
			case T_OpExpr:
				_outOpExpr(str, obj);
				break;
			case T_DistinctExpr:
				_outDistinctExpr(str, obj);
				break;
			case T_ScalarArrayOpExpr:
				_outScalarArrayOpExpr(str, obj);
				break;
			case T_BoolExpr:
				_outBoolExpr(str, obj);
				break;
			case T_SubLink:
				_outSubLink(str, obj);
				break;
			case T_SubPlan:
				_outSubPlan(str, obj);
				break;
			case T_AlternativeSubPlan:
				_outAlternativeSubPlan(str, obj);
				break;
			case T_FieldSelect:
				_outFieldSelect(str, obj);
				break;
			case T_FieldStore:
				_outFieldStore(str, obj);
				break;
			case T_RelabelType:
				_outRelabelType(str, obj);
				break;
			case T_CoerceViaIO:
				_outCoerceViaIO(str, obj);
				break;
			case T_ArrayCoerceExpr:
				_outArrayCoerceExpr(str, obj);
				break;
			case T_ConvertRowtypeExpr:
				_outConvertRowtypeExpr(str, obj);
				break;
			case T_CollateExpr:
				_outCollateExpr(str, obj);
				break;
			case T_CaseExpr:
				_outCaseExpr(str, obj);
				break;
			case T_CaseWhen:
				_outCaseWhen(str, obj);
				break;
			case T_CaseTestExpr:
				_outCaseTestExpr(str, obj);
				break;
			case T_ArrayExpr:
				_outArrayExpr(str, obj);
				break;
			case T_RowExpr:
				_outRowExpr(str, obj);
				break;
			case T_RowCompareExpr:
				_outRowCompareExpr(str, obj);
				break;
			case T_CoalesceExpr:
				_outCoalesceExpr(str, obj);
				break;
			case T_MinMaxExpr:
				_outMinMaxExpr(str, obj);
				break;
			case T_NullIfExpr:
				_outNullIfExpr(str, obj);
				break;
			case T_NullTest:
				_outNullTest(str, obj);
				break;
			case T_BooleanTest:
				_outBooleanTest(str, obj);
				break;
			case T_SQLValueFunction:
				_outSQLValueFunction(str, obj);
				break;
			case T_XmlExpr:
				_outXmlExpr(str, obj);
				break;
			case T_CoerceToDomain:
				_outCoerceToDomain(str, obj);
				break;
			case T_CoerceToDomainValue:
				_outCoerceToDomainValue(str, obj);
				break;
			case T_SetToDefault:
				_outSetToDefault(str, obj);
				break;
			case T_CurrentOfExpr:
				_outCurrentOfExpr(str, obj);
				break;
			case T_NextValueExpr:
				_outNextValueExpr(str, obj);
				break;
			case T_InferenceElem:
				_outInferenceElem(str, obj);
				break;
			case T_TargetEntry:
				_outTargetEntry(str, obj);
				break;
			case T_RangeTblRef:
				_outRangeTblRef(str, obj);
				break;
			case T_JoinExpr:
				_outJoinExpr(str, obj);
				break;
			case T_FromExpr:
				_outFromExpr(str, obj);
				break;
			case T_OnConflictExpr:
				_outOnConflictExpr(str, obj);
				break;
			case T_CreateExtensionStmt:
				_outCreateExtensionStmt(str, obj);
				break;
			case T_GrantStmt:
				_outGrantStmt(str, obj);
				break;
			case T_AccessPriv:
				_outAccessPriv(str, obj);
				break;
			case T_ObjectWithArgs:
				_outObjectWithArgs(str, obj);
				break;
			case T_GrantRoleStmt:
				_outGrantRoleStmt(str, obj);
				break;
			case T_LockStmt:
				_outLockStmt(str, obj);
				break;

			case T_ExtensibleNode:
				_outExtensibleNode(str, obj);
				break;

			case T_CreateStmt:
				_outCreateStmt(str, obj);
				break;
			case T_CreateForeignTableStmt:
				_outCreateForeignTableStmt(str, obj);
				break;
			case T_DistributionKeyElem:
				_outDistributionKeyElem(str, obj);
				break;
			case T_ColumnReferenceStorageDirective:
				_outColumnReferenceStorageDirective(str, obj);
				break;
			case T_RoleSpec:
				_outRoleSpec(str, obj);
				break;

			case T_SegfileMapNode:
				_outSegfileMapNode(str, obj);
				break;

			case T_ExtTableTypeDesc:
				_outExtTableTypeDesc(str, obj);
				break;
            case T_CreateExternalStmt:
				_outCreateExternalStmt(str, obj);
				break;

			case T_IndexStmt:
				_outIndexStmt(str, obj);
				break;
			case T_ReindexStmt:
				_outReindexStmt(str, obj);
				break;

			case T_ConstraintsSetStmt:
				_outConstraintsSetStmt(str, obj);
				break;

			case T_CreateFunctionStmt:
				_outCreateFunctionStmt(str, obj);
				break;
			case T_FunctionParameter:
				_outFunctionParameter(str, obj);
				break;
			case T_AlterFunctionStmt:
				_outAlterFunctionStmt(str, obj);
				break;

			case T_AlterObjectDependsStmt:
				_outAlterObjectDependsStmt(str, obj);
				break;

			case T_DefineStmt:
				_outDefineStmt(str,obj);
				break;

			case T_CompositeTypeStmt:
				_outCompositeTypeStmt(str,obj);
				break;
			case T_CreateEnumStmt:
				_outCreateEnumStmt(str,obj);
				break;
			case T_CreateRangeStmt:
				_outCreateRangeStmt(str, obj);
				break;
			case T_AlterEnumStmt:
				_outAlterEnumStmt(str, obj);
				break;

			case T_CreateCastStmt:
				_outCreateCastStmt(str,obj);
				break;
			case T_CreateOpClassStmt:
				_outCreateOpClassStmt(str,obj);
				break;
			case T_CreateOpClassItem:
				_outCreateOpClassItem(str,obj);
				break;
			case T_CreateOpFamilyStmt:
				_outCreateOpFamilyStmt(str,obj);
				break;
			case T_AlterOpFamilyStmt:
				_outAlterOpFamilyStmt(str,obj);
				break;
			case T_CreateConversionStmt:
				_outCreateConversionStmt(str,obj);
				break;


			case T_ViewStmt:
				_outViewStmt(str, obj);
				break;
			case T_RuleStmt:
				_outRuleStmt(str, obj);
				break;
			case T_DropStmt:
				_outDropStmt(str, obj);
				break;
			case T_DropOwnedStmt:
				_outDropOwnedStmt(str, obj);
				break;
			case T_ReassignOwnedStmt:
				_outReassignOwnedStmt(str, obj);
				break;
			case T_TruncateStmt:
				_outTruncateStmt(str, obj);
				break;
			case T_ReplicaIdentityStmt:
				_outReplicaIdentityStmt(str, obj);
				break;
			case T_AlterTableStmt:
				_outAlterTableStmt(str, obj);
				break;
			case T_AlterTableCmd:
				_outAlterTableCmd(str, obj);
				break;
			case T_AlteredTableInfo:
				_outAlteredTableInfo(str, obj);
				break;
			case T_NewConstraint:
				_outNewConstraint(str, obj);
				break;
			case T_NewColumnValue:
				_outNewColumnValue(str, obj);
				break;

			case T_CreateRoleStmt:
				_outCreateRoleStmt(str, obj);
				break;
			case T_DropRoleStmt:
				_outDropRoleStmt(str, obj);
				break;
			case T_AlterRoleStmt:
				_outAlterRoleStmt(str, obj);
				break;
			case T_AlterRoleSetStmt:
				_outAlterRoleSetStmt(str, obj);
				break;

			case T_AlterObjectSchemaStmt:
				_outAlterObjectSchemaStmt(str, obj);
				break;

			case T_AlterOwnerStmt:
				_outAlterOwnerStmt(str, obj);
				break;

			case T_RenameStmt:
				_outRenameStmt(str, obj);
				break;

			case T_CreateSeqStmt:
				_outCreateSeqStmt(str, obj);
				break;
			case T_AlterSeqStmt:
				_outAlterSeqStmt(str, obj);
				break;
			case T_ClusterStmt:
				_outClusterStmt(str, obj);
				break;
			case T_CreatedbStmt:
				_outCreatedbStmt(str, obj);
				break;
			case T_DropdbStmt:
				_outDropdbStmt(str, obj);
				break;
			case T_CreateDomainStmt:
				_outCreateDomainStmt(str, obj);
				break;
			case T_AlterDomainStmt:
				_outAlterDomainStmt(str, obj);
				break;
			case T_AlterDefaultPrivilegesStmt:
				_outAlterDefaultPrivilegesStmt(str, obj);
				break;
			case T_TransactionStmt:
				_outTransactionStmt(str, obj);
				break;
			case T_CreateStatsStmt:
				_outCreateStatsStmt(str, obj);
				break;
			case T_NotifyStmt:
				_outNotifyStmt(str, obj);
				break;
			case T_DeclareCursorStmt:
				_outDeclareCursorStmt(str, obj);
				break;
			case T_SingleRowErrorDesc:
				_outSingleRowErrorDesc(str, obj);
				break;
			case T_CopyStmt:
				_outCopyStmt(str, obj);
				break;
			case T_SelectStmt:
				_outSelectStmt(str, obj);
				break;
			case T_InsertStmt:
				_outInsertStmt(str, obj);
				break;
			case T_DeleteStmt:
				_outDeleteStmt(str, obj);
				break;
			case T_UpdateStmt:
				_outUpdateStmt(str, obj);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj);
				break;
			case T_TypeName:
				_outTypeName(str, obj);
				break;
			case T_SortBy:
				_outSortBy(str, obj);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj);
				break;
			case T_CollateClause:
				_outCollateClause(str, obj);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj);
				break;
			case T_Query:
				_outQuery(str, obj);
				break;
			case T_WithCheckOption:
				_outWithCheckOption(str, obj);
				break;
			case T_SortGroupClause:
				_outSortGroupClause(str, obj);
				break;
			case T_GroupingSet:
				_outGroupingSet(str, obj);
				break;
			case T_WindowClause:
				_outWindowClause(str, obj);
				break;
			case T_RowMarkClause:
				_outRowMarkClause(str, obj);
				break;
			case T_WithClause:
				_outWithClause(str, obj);
				break;
			case T_CommonTableExpr:
				_outCommonTableExpr(str, obj);
				break;
			case T_SetOperationStmt:
				_outSetOperationStmt(str, obj);
				break;
			case T_RangeTblEntry:
				_outRangeTblEntry(str, obj);
				break;
			case T_RangeTblFunction:
				_outRangeTblFunction(str, obj);
				break;
			case T_TableSampleClause:
				_outTableSampleClause(str, obj);
				break;
			case T_A_Expr:
				_outAExpr(str, obj);
				break;
			case T_ColumnRef:
				_outColumnRef(str, obj);
				break;
			case T_ParamRef:
				_outParamRef(str, obj);
				break;
			case T_RawStmt:
				_outRawStmt(str, obj);
				break;
			case T_A_Const:
				_outAConst(str, obj);
				break;
			case T_A_Star:
				_outA_Star(str, obj);
				break;
			case T_A_Indices:
				_outA_Indices(str, obj);
				break;
			case T_A_Indirection:
				_outA_Indirection(str, obj);
				break;
			case T_A_ArrayExpr:
				_outA_ArrayExpr(str,obj);
				break;
			case T_ResTarget:
				_outResTarget(str, obj);
				break;
			case T_MultiAssignRef:
				_outMultiAssignRef(str, obj);
				break;
			case T_Constraint:
				_outConstraint(str, obj);
				break;
			case T_FuncCall:
				_outFuncCall(str, obj);
				break;
			case T_DefElem:
				_outDefElem(str, obj);
				break;
			case T_TableLikeClause:
				_outTableLikeClause(str, obj);
				break;
			case T_LockingClause:
				_outLockingClause(str, obj);
				break;
			case T_XmlSerialize:
				_outXmlSerialize(str, obj);
				break;
			case T_TriggerTransition:
				_outTriggerTransition(str, obj);
				break;
			case T_PartitionElem:
				_outPartitionElem(str, obj);
				break;
			case T_PartitionSpec:
				_outPartitionSpec(str, obj);
				break;
			case T_PartitionBoundSpec:
				_outPartitionBoundSpec(str, obj);
				break;
			case T_PartitionRangeDatum:
				_outPartitionRangeDatum(str, obj);
				break;
			case T_PartitionCmd:
				_outPartitionCmd(str, obj);
				break;
			case T_GpAlterPartitionId:
				_outGpAlterPartitionId(str, obj);
				break;
			case T_GpDropPartitionCmd:
				_outGpDropPartitionCmd(str, obj);
				break;
			case T_GpAlterPartitionCmd:
				_outGpAlterPartitionCmd(str, obj);
				break;

			case T_CreateSchemaStmt:
				_outCreateSchemaStmt(str, obj);
				break;
			case T_CreatePLangStmt:
				_outCreatePLangStmt(str, obj);
				break;
			case T_VacuumStmt:
				_outVacuumStmt(str, obj);
				break;
			case T_VacuumRelation:
				_outVacuumRelation(str, obj);
				break;
			case T_CdbProcess:
				_outCdbProcess(str, obj);
				break;
			case T_SliceTable:
				_outSliceTable(str, obj);
				break;
			case T_CursorPosInfo:
				_outCursorPosInfo(str, obj);
				break;
			case T_VariableSetStmt:
				_outVariableSetStmt(str, obj);
				break;

			case T_DMLActionExpr:
				_outDMLActionExpr(str, obj);
				break;

			case T_CreateTrigStmt:
				_outCreateTrigStmt(str, obj);
				break;

			case T_CreateTableSpaceStmt:
				_outCreateTableSpaceStmt(str, obj);
				break;

			case T_DropTableSpaceStmt:
				_outDropTableSpaceStmt(str, obj);
				break;

			case T_CreateQueueStmt:
				_outCreateQueueStmt(str, obj);
				break;
			case T_AlterQueueStmt:
				_outAlterQueueStmt(str, obj);
				break;
			case T_DropQueueStmt:
				_outDropQueueStmt(str, obj);
				break;

			case T_CreateResourceGroupStmt:
				_outCreateResourceGroupStmt(str, obj);
				break;
			case T_DropResourceGroupStmt:
				_outDropResourceGroupStmt(str, obj);
				break;
			case T_AlterResourceGroupStmt:
				_outAlterResourceGroupStmt(str, obj);
				break;

            case T_CommentStmt:
                _outCommentStmt(str, obj);
                break;
			case T_TableValueExpr:
				_outTableValueExpr(str, obj);
                break;
			case T_DenyLoginInterval:
				_outDenyLoginInterval(str, obj);
				break;
			case T_DenyLoginPoint:
				_outDenyLoginPoint(str, obj);
				break;

			case T_AlterTypeStmt:
				_outAlterTypeStmt(str, obj);
				break;
			case T_AlterExtensionStmt:
				_outAlterExtensionStmt(str, obj);
				break;
			case T_AlterExtensionContentsStmt:
				_outAlterExtensionContentsStmt(str, obj);
				break;

			case T_TupleDescNode:
				_outTupleDescNode(str, obj);
				break;
			case T_SerializedParams:
				_outSerializedParams(str, obj);
				break;

			case T_AlterTSConfigurationStmt:
				_outAlterTSConfigurationStmt(str, obj);
				break;
			case T_AlterTSDictionaryStmt:
				_outAlterTSDictionaryStmt(str, obj);
				break;

			case T_CookedConstraint:
				_outCookedConstraint(str, obj);
				break;

			case T_DropUserMappingStmt:
				_outDropUserMappingStmt(str, obj);
				break;
			case T_AlterUserMappingStmt:
				_outAlterUserMappingStmt(str, obj);
				break;
			case T_CreateUserMappingStmt:
				_outCreateUserMappingStmt(str, obj);
				break;
			case T_AlterForeignServerStmt:
				_outAlterForeignServerStmt(str, obj);
				break;
			case T_CreateForeignServerStmt:
				_outCreateForeignServerStmt(str, obj);
				break;
			case T_AlterFdwStmt:
				_outAlterFdwStmt(str, obj);
				break;
			case T_CreateFdwStmt:
				_outCreateFdwStmt(str, obj);
				break;
			case T_GpPolicy:
				_outGpPolicy(str, obj);
				break;
			case T_DistributedBy:
				_outDistributedBy(str, obj);
				break;
			case T_ImportForeignSchemaStmt:
				_outImportForeignSchemaStmt(str, obj);
				break;
			case T_AlterTableMoveAllStmt:
				_outAlterTableMoveAllStmt(str, obj);
				break;
			case T_AlterTableSpaceOptionsStmt:
				_outAlterTableSpaceOptionsStmt(str, obj);
				break;

			case T_CreatePublicationStmt:
				_outCreatePublicationStmt(str, obj);
				break;
			case T_AlterPublicationStmt:
				_outAlterPublicationStmt(str, obj);
				break;
			case T_CreateSubscriptionStmt:
				_outCreateSubscriptionStmt(str, obj);
				break;
			case T_DropSubscriptionStmt:
				_outDropSubscriptionStmt(str, obj);
				break;
			case T_AlterSubscriptionStmt:
				_outAlterSubscriptionStmt(str, obj);
				break;

			case T_CreatePolicyStmt:
				_outCreatePolicyStmt(str, obj);
				break;
			case T_AlterPolicyStmt:
				_outAlterPolicyStmt(str, obj);
				break;
			case T_CreateTransformStmt:
				_outCreateTransformStmt(str, obj);
				break;
			case T_CreateAmStmt:
				_outCreateAmStmt(str, obj);
				break;
			case T_AggExprId:
				_outAggExprId(str, obj);
				break;
			case T_RowIdExpr:
				_outRowIdExpr(str, obj);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj);
				break;

			default:
				elog(ERROR, "could not serialize unrecognized node type: %d",
						 (int) nodeTag(obj));
				break;
		}
	}
}

/*
 * nodeToBinaryStringFast -
 *	   returns a binary representation of the Node as a palloc'd string
 */
char *
nodeToBinaryStringFast(void *obj, int *length)
{
	StringInfoData str;
	int16 tg = (int16) 0xDEAD;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfoOfSize(&str, 4096);

	_outNode(&str, obj);

	/* Add something special at the end that we can check in readfast.c */
	appendBinaryStringInfo(&str, (const char *)&tg, sizeof(int16));

	*length = str.len;
	return str.data;
}
