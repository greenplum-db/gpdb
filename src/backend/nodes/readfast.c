/*-------------------------------------------------------------------------
 *
 * readfast.c
 *	  Binary Reader functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * These routines must be exactly the inverse of the routines in
 * outfast.c.
 *
 * For most node types, these routines are identical to the text reader
 * functions, in readfuncs.c. To avoid code duplication and merge hazards
 * (readfast.c is a Greenplum addon), most read routines borrow the source
 * definition from readfuncs.c, we just compile it with different READ_*
 * macros.
 *
 * The way that works is that readfast.c defines all the necessary macros,
 * as well as COMPILING_BINARY_FUNCS, and then #includes readfuncs.c. For
 * those node types where the binary and text functions are different,
 * the function in readfuncs.c is put in a #ifndef COMPILING_BINARY_FUNCS
 * block, and readfast.c provides the binary version of the function.
 * outfast.c and outfuncs.c have a similar relationship.
 *
 * By this, CDB could link only readfast.o (#includes readfuncs.c) to get all
 * the fast version deserializing functions, outfast.o likewise.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/readfuncs.h"
#include "catalog/aocatalog.h"
#include "catalog/pg_class.h"
#include "catalog/heap.h"
#include "cdb/cdbgang.h"

/*
 * Macros to simplify reading of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.
 */

#define READ_TEMP_LOCALS()

#define READ_LOCALS(nodeTypeName) \
	nodeTypeName *local_node = makeNode(nodeTypeName)

/*
 * no difference between READ_LOCALS and READ_LOCALS_NO_FIELDS in readfast.c,
 * but define both for compatibility.
 */
#define READ_LOCALS_NO_FIELDS(nodeTypeName) \
	READ_LOCALS(nodeTypeName)

/* Allocate non-node variable */
#define ALLOCATE_LOCAL(local, typeName, size) \
	local = (typeName *)palloc0(sizeof(typeName) * size)

/* Read an integer field  */
#define READ_INT_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(int));  read_str_ptr+=sizeof(int)

/* Read an int8 field  */
#define READ_INT8_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(int8));  read_str_ptr+=sizeof(int8)

/* Read an int16 field  */
#define READ_INT16_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(int16));  read_str_ptr+=sizeof(int16)

/* Read an unsigned integer field) */
#define READ_UINT_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(int)); read_str_ptr+=sizeof(int)

/* Read an uint64 field) */
#define READ_UINT64_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(uint64)); read_str_ptr+=sizeof(uint64)

/* Read an OID field (don't hard-wire assumption that OID is same as uint) */
#define READ_OID_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(Oid)); read_str_ptr+=sizeof(Oid)

/* Read a long-integer field  */
#define READ_LONG_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(long)); read_str_ptr+=sizeof(long)

/* Read a char field (ie, one ascii character) */
#define READ_CHAR_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, 1); read_str_ptr++

/* Read an enumerated-type field that was written as a short integer code */
#define READ_ENUM_FIELD(fldname, enumtype) \
	{ int16 ent; memcpy(&ent, read_str_ptr, sizeof(int16));  read_str_ptr+=sizeof(int16);local_node->fldname = (enumtype) ent; }

/* Read a float field */
#define READ_FLOAT_FIELD(fldname) \
	memcpy(&local_node->fldname, read_str_ptr, sizeof(double)); read_str_ptr+=sizeof(double)

/* Read a boolean field */
#define READ_BOOL_FIELD(fldname) \
	local_node->fldname = read_str_ptr[0] != 0;  Assert(read_str_ptr[0]==1 || read_str_ptr[0]==0); read_str_ptr++

/* Read a character-string variable */
#define READ_STRING_VAR(var) \
	{ int slen; char * nn = NULL; \
		memcpy(&slen, read_str_ptr, sizeof(int)); \
		read_str_ptr+=sizeof(int); \
		if (slen>0) { \
		    nn = palloc(slen+1); \
		    memcpy(nn,read_str_ptr,slen); \
		    read_str_ptr+=(slen); nn[slen]='\0'; \
		} \
		var = nn;  }

/* Read a character-string field */
#define READ_STRING_FIELD(fldname)  READ_STRING_VAR(local_node->fldname)

/* Read a parse location field (and throw away the value, per notes above) */
#define READ_LOCATION_FIELD(fldname) READ_INT_FIELD(fldname)

/* Read a Node field */
#ifdef GP_SERIALIZATION_DEBUG
#define READ_NODE_FIELD(fldname) \
	do { \
		char *xexpected = CppAsString(fldname); \
		char got[100]; \
		\
		memcpy(got, read_str_ptr, strlen(xexpected) + 1); \
		read_str_ptr += strlen(xexpected) + 1; \
		if (strcmp(xexpected, got) != 0) \
			elog(ERROR, "deserialization lost sync: %s vs %02x%02x%02x", xexpected, (unsigned char) got[0], (unsigned char) got[1], (unsigned char) got[2]); \
		\
		local_node->fldname = readNodeBinary(); \
	} while(0)
#else
#define READ_NODE_FIELD(fldname) \
	local_node->fldname = readNodeBinary()
#endif

/* Read a bitmapset field */
#define READ_BITMAPSET_FIELD(fldname) \
	 local_node->fldname = _readBitmapset()

/* Read in a binary field */
#define READ_BINARY_FIELD(fldname, sz) \
	memcpy(&local_node->fldname, read_str_ptr, (sz)); read_str_ptr += (sz)

/* Read a bytea field */
#define READ_BYTEA_FIELD(fldname) \
	local_node->fldname = (bytea *) DatumGetPointer(readDatumBinary(false))

/* Read a dummy field */
#define READ_DUMMY_FIELD(fldname,fldvalue) \
	{ local_node->fldname = (0); /*read_str_ptr += sizeof(int*);*/ }

/* Routine exit */
#define READ_DONE() \
	return local_node

/* Read an integer array  */
#define READ_ARRAY_OF_TYPE(fldname, count, Type) \
	if ( (count) > 0 ) \
	{ \
		int i; \
		local_node->fldname = (Type *)palloc((count) * sizeof(Type)); \
		for(i = 0; i < (count); i++) \
		{ \
			memcpy(&local_node->fldname[i], read_str_ptr, sizeof(Type)); read_str_ptr+=sizeof(Type); \
		} \
	}

/* Read a bool array  */
#define READ_BOOL_ARRAY(fldname, count) \
	if ( (count) > 0 ) \
	{ \
		int i; \
		local_node->fldname = (bool *) palloc((count) * sizeof(bool)); \
		for(i = 0; i < (count); i++) \
		{ \
			local_node->fldname[i] = *read_str_ptr ? true : false; \
			read_str_ptr++; \
		} \
	}

/* Read an attribute number array  */
#define READ_ATTRNUMBER_ARRAY(fldname, count) READ_ARRAY_OF_TYPE(fldname, count, AttrNumber)

/* Read an Oid array  */
#define READ_OID_ARRAY(fldname, count) READ_ARRAY_OF_TYPE(fldname, count, Oid)

/* Read an int array  */
#define READ_INT_ARRAY(fldname, count) READ_ARRAY_OF_TYPE(fldname, count, int)


static void *readNodeBinary(void);

static Datum readDatumBinary(bool typbyval);
#define readDatum(x) readDatumBinary(x)

static Bitmapset *_readBitmapset(void);

/*
 * Current position in the message that we are processing. We can keep
 * this in a global variable because readNodeFromBinaryString() is not
 * re-entrant. This is similar to the current position that pg_strtok()
 * keeps, used by the normal stringToNode() function.
 */
static const char *read_str_ptr;

/*
 * For most structs, we reuse the definitions from readfuncs.c. See comment
 * in readfuncs.c.
 */
#define COMPILING_BINARY_FUNCS
#include "readfuncs.c"

/*
 * For some structs, we have to provide a read functions because it differs
 * from the text version (or the text version doesn't exist at all).
 */

/*
 * _readQuery
 */
static Query *
_readQuery(void)
{
	READ_LOCALS(Query);

	READ_ENUM_FIELD(commandType, CmdType); Assert(local_node->commandType <= CMD_NOTHING);
	READ_ENUM_FIELD(querySource, QuerySource); 	Assert(local_node->querySource <= QSRC_PLANNER);
	READ_BOOL_FIELD(canSetTag);
	READ_NODE_FIELD(utilityStmt);
	READ_INT_FIELD(resultRelation);
	READ_BOOL_FIELD(hasAggs);
	READ_BOOL_FIELD(hasWindowFuncs);
	READ_BOOL_FIELD(hasSubLinks);
	READ_BOOL_FIELD(hasDynamicFunctions);
	READ_BOOL_FIELD(hasFuncsWithExecRestrictions);
	READ_BOOL_FIELD(hasDistinctOn);
	READ_BOOL_FIELD(hasRecursive);
	READ_BOOL_FIELD(hasModifyingCTE);
	READ_BOOL_FIELD(hasForUpdate);
	READ_BOOL_FIELD(hasRowSecurity);
	READ_BOOL_FIELD(canOptSelectLockingClause);
	READ_NODE_FIELD(cteList);
	READ_NODE_FIELD(rtable);
	READ_NODE_FIELD(jointree);
	READ_NODE_FIELD(targetList);
	READ_NODE_FIELD(withCheckOptions);
	READ_NODE_FIELD(onConflict);
	READ_NODE_FIELD(returningList);
	READ_NODE_FIELD(groupClause);
	READ_NODE_FIELD(groupingSets);
	READ_NODE_FIELD(havingQual);
	READ_NODE_FIELD(windowClause);
	READ_NODE_FIELD(distinctClause);
	READ_NODE_FIELD(sortClause);
	READ_NODE_FIELD(scatterClause);
	READ_BOOL_FIELD(isTableValueSelect);
	READ_NODE_FIELD(limitOffset);
	READ_NODE_FIELD(limitCount);
	READ_NODE_FIELD(rowMarks);
	READ_NODE_FIELD(setOperations);
	READ_NODE_FIELD(constraintDeps);
	READ_BOOL_FIELD(parentStmtType);

	/* policy not serialized */

	READ_DONE();
}

static DMLActionExpr *
_readDMLActionExpr(void)
{
	READ_LOCALS(DMLActionExpr);

	READ_DONE();
}

/*
 *	Stuff from primnodes.h.
 */

/*
 * _readConst
 */
static Const *
_readConst(void)
{
	READ_LOCALS(Const);

	READ_OID_FIELD(consttype);
	READ_INT_FIELD(consttypmod);
	READ_OID_FIELD(constcollid);
	READ_INT_FIELD(constlen);
	READ_BOOL_FIELD(constbyval);
	READ_BOOL_FIELD(constisnull);
	READ_LOCATION_FIELD(location);
	if (local_node->constisnull)
		local_node->constvalue = 0;
	else
		local_node->constvalue = readDatumBinary(local_node->constbyval);

	READ_DONE();
}

static ResTarget *
_readResTarget(void)
{
	READ_LOCALS(ResTarget);

	READ_STRING_FIELD(name);
	READ_NODE_FIELD(indirection);
	READ_NODE_FIELD(val);
	READ_LOCATION_FIELD(location);

	READ_DONE();
}

static MultiAssignRef *
_readMultiAssignRef(void)
{
	READ_LOCALS(MultiAssignRef);

	READ_NODE_FIELD(source);
	READ_INT_FIELD(colno);
	READ_INT_FIELD(ncolumns);

	READ_DONE();
}

static DropOwnedStmt *
_readDropOwnedStmt(void)
{
	READ_LOCALS(DropOwnedStmt);

	READ_NODE_FIELD(roles);
	READ_ENUM_FIELD(behavior, DropBehavior);

	READ_DONE();
}

static ReassignOwnedStmt *
_readReassignOwnedStmt(void)
{
	READ_LOCALS(ReassignOwnedStmt);

	READ_NODE_FIELD(roles);
	READ_NODE_FIELD(newrole);

	READ_DONE();
}

static AlterObjectDependsStmt *
_readAlterObjectDependsStmt(void)
{
	READ_LOCALS(AlterObjectDependsStmt);

	READ_ENUM_FIELD(objectType,ObjectType);
	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(object);
	READ_NODE_FIELD(extname);

	READ_DONE();
}

static SelectStmt *
_readSelectStmt(void)
{
	READ_LOCALS(SelectStmt);

	READ_NODE_FIELD(distinctClause);
	READ_NODE_FIELD(intoClause);
	READ_NODE_FIELD(targetList);
	READ_NODE_FIELD(fromClause);
	READ_NODE_FIELD(whereClause);
	READ_NODE_FIELD(groupClause);
	READ_NODE_FIELD(havingClause);
	READ_NODE_FIELD(windowClause);
	READ_NODE_FIELD(valuesLists);
	READ_NODE_FIELD(sortClause);
	READ_NODE_FIELD(scatterClause);
	READ_NODE_FIELD(limitOffset);
	READ_NODE_FIELD(limitCount);
	READ_NODE_FIELD(lockingClause);
	READ_NODE_FIELD(withClause);
	READ_ENUM_FIELD(op, SetOperation);
	READ_BOOL_FIELD(all);
	READ_NODE_FIELD(larg);
	READ_NODE_FIELD(rarg);
	READ_BOOL_FIELD(disableLockingOptimization);
	READ_DONE();
}

static InsertStmt *
_readInsertStmt(void)
{
	READ_LOCALS(InsertStmt);

	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(cols);
	READ_NODE_FIELD(selectStmt);
	READ_NODE_FIELD(returningList);
	READ_NODE_FIELD(withClause);
	READ_DONE();
}

static DeleteStmt *
_readDeleteStmt(void)
{
	READ_LOCALS(DeleteStmt);

	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(usingClause);
	READ_NODE_FIELD(whereClause);
	READ_NODE_FIELD(returningList);
	READ_NODE_FIELD(withClause);
	READ_DONE();
}

static UpdateStmt *
_readUpdateStmt(void)
{
	READ_LOCALS(UpdateStmt);

	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(targetList);
	READ_NODE_FIELD(whereClause);
	READ_NODE_FIELD(fromClause);
	READ_NODE_FIELD(returningList);
	READ_NODE_FIELD(withClause);
	READ_DONE();
}

static A_Const *
_readAConst(void)
{
	READ_LOCALS(A_Const);

	READ_ENUM_FIELD(val.type, NodeTag);

	switch (local_node->val.type)
	{
		case T_Integer:
			memcpy(&local_node->val.val.ival, read_str_ptr, sizeof(long)); read_str_ptr+=sizeof(long);
			break;
		case T_Float:
		case T_String:
		case T_BitString:
		{
			int slen; char * nn;
			memcpy(&slen, read_str_ptr, sizeof(int));
			read_str_ptr+=sizeof(int);
			nn = palloc(slen+1);
			memcpy(nn,read_str_ptr,slen);
			nn[slen] = '\0';
			local_node->val.val.str = nn; read_str_ptr+=slen;
		}
			break;
	 	case T_Null:
	 	default:
	 		break;
	}

    READ_LOCATION_FIELD(location);   /*CDB*/
	READ_DONE();
}

static A_Star *
_readA_Star(void)
{
	READ_LOCALS(A_Star);
	READ_DONE();
}


static A_Indices *
_readA_Indices(void)
{
	READ_LOCALS(A_Indices);
	READ_BOOL_FIELD(is_slice);
	READ_NODE_FIELD(lidx);
	READ_NODE_FIELD(uidx);
	READ_DONE();
}

static A_Indirection *
_readA_Indirection(void)
{
	READ_LOCALS(A_Indirection);
	READ_NODE_FIELD(arg);
	READ_NODE_FIELD(indirection);
	READ_DONE();
}

static RoleSpec *
_readRoleSpec(void)
{
	READ_LOCALS(RoleSpec);

	READ_ENUM_FIELD(roletype, RoleSpecType);
	READ_STRING_FIELD(rolename);
	READ_LOCATION_FIELD(location);

	READ_DONE();
}

static A_Expr *
_readAExpr(void)
{
	READ_LOCALS(A_Expr);

	READ_ENUM_FIELD(kind, A_Expr_Kind);

	Assert(local_node->kind <= AEXPR_PAREN);

	switch (local_node->kind)
	{
		case AEXPR_OP:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_OP_ANY:

			READ_NODE_FIELD(name);

			break;
		case AEXPR_OP_ALL:

			READ_NODE_FIELD(name);

			break;
		case AEXPR_DISTINCT:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_NULLIF:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_OF:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_IN:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_LIKE:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_ILIKE:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_SIMILAR:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_BETWEEN:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_NOT_BETWEEN:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_BETWEEN_SYM:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_NOT_BETWEEN_SYM:

			READ_NODE_FIELD(name);
			break;
		case AEXPR_PAREN:

			READ_NODE_FIELD(name);
			break;
		default:
			elog(ERROR,"Unable to understand A_Expr node ");
			break;
	}

	READ_NODE_FIELD(lexpr);
	READ_NODE_FIELD(rexpr);
	READ_LOCATION_FIELD(location);

	READ_DONE();
}

/*
 * _readOpExpr
 */
static OpExpr *
_readOpExpr(void)
{
	READ_LOCALS(OpExpr);

	READ_OID_FIELD(opno);
	READ_OID_FIELD(opfuncid);

	/*
	 * The opfuncid is stored in the textual format primarily for debugging
	 * and documentation reasons.  We want to always read it as zero to force
	 * it to be re-looked-up in the pg_operator entry.	This ensures that
	 * stored rules don't have hidden dependencies on operators' functions.
	 * (We don't currently support an ALTER OPERATOR command, but might
	 * someday.)
	 */
/*	local_node->opfuncid = InvalidOid; */

	READ_OID_FIELD(opresulttype);
	READ_BOOL_FIELD(opretset);
	READ_OID_FIELD(opcollid);
	READ_OID_FIELD(inputcollid);
	READ_NODE_FIELD(args);
	READ_LOCATION_FIELD(location);

	READ_DONE();
}

/*
 * _readBoolExpr
 */
static BoolExpr *
_readBoolExpr(void)
{
	READ_LOCALS(BoolExpr);

	READ_ENUM_FIELD(boolop, BoolExprType);

	READ_NODE_FIELD(args);
	READ_LOCATION_FIELD(location);

	READ_DONE();
}

/*
 *	Stuff from parsenodes.h.
 */


/*
 * _readCollateClause
 */
static CollateClause *
_readCollateClause(void)
{
	READ_LOCALS(CollateClause);

	READ_NODE_FIELD(arg);
	READ_NODE_FIELD(collname);
	READ_INT_FIELD(location);

	READ_DONE();
}

static ExtensibleNode *
_readExtensibleNode(void)
{
	const ExtensibleNodeMethods *methods;
	ExtensibleNode *local_node;
	const char *extnodename;

	char *str;
	const char *save_strtok = NULL;
	const char *save_begin = NULL;
	const char ** save_strtok_ptr = &save_strtok;
	const char ** save_begin_ptr = &save_begin;

	READ_STRING_VAR(extnodename);
	if (!extnodename)
		elog(ERROR, "extnodename has to be supplied");
	methods = GetExtensibleNodeMethods(extnodename, false);

	local_node = (ExtensibleNode *) newNode(methods->node_size,
											T_ExtensibleNode);
	local_node->extnodename = extnodename;

	READ_STRING_VAR(str);

	/*
	 * deserialize the private fields
	 */

	/* set the states for pg_strtok(), let methods->nodeRead() to process str */
	save_strtok_states(save_strtok_ptr, save_begin_ptr);
	set_strtok_states(str, str);

	/* do reading */
	methods->nodeRead(local_node);

	/* set the states for pg_strtok() back */
	set_strtok_states(save_strtok, save_begin);

	READ_DONE();
}

/*
 * Greenplum Database additions for serialization support
 */
#include "nodes/plannodes.h"

static CreateExtensionStmt *
_readCreateExtensionStmt(void)
{
	READ_LOCALS(CreateExtensionStmt);
	READ_STRING_FIELD(extname);
	READ_BOOL_FIELD(if_not_exists);
	READ_NODE_FIELD(options);
	READ_ENUM_FIELD(create_ext_state, CreateExtensionState);

	READ_DONE();
}

static void
_readCreateStmt_common(CreateStmt *local_node)
{
	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(tableElts);
	READ_NODE_FIELD(inhRelations);
	READ_NODE_FIELD(partspec);
	READ_NODE_FIELD(partbound);
	READ_NODE_FIELD(ofTypename);
	READ_NODE_FIELD(constraints);
	READ_NODE_FIELD(options);
	READ_ENUM_FIELD(oncommit,OnCommitAction);
	READ_STRING_FIELD(tablespacename);
	READ_STRING_FIELD(accessMethod);
	READ_BOOL_FIELD(if_not_exists);

	READ_NODE_FIELD(distributedBy);
	READ_NODE_FIELD(partitionBy);
	READ_CHAR_FIELD(relKind);
	READ_OID_FIELD(ownerid);
	READ_BOOL_FIELD(buildAoBlkdir);
	READ_NODE_FIELD(attr_encodings);
	READ_BOOL_FIELD(isCtas);

	READ_NODE_FIELD(part_idx_oids);
	READ_NODE_FIELD(part_idx_names);

	/*
	 * Some extra checks to make sure we didn't get lost
	 * during serialization/deserialization
	 */
	Assert(local_node->relKind == RELKIND_RELATION ||
		   local_node->relKind == RELKIND_PARTITIONED_TABLE ||
		   local_node->relKind == RELKIND_INDEX ||
		   local_node->relKind == RELKIND_SEQUENCE ||
		   local_node->relKind == RELKIND_TOASTVALUE ||
		   local_node->relKind == RELKIND_VIEW ||
		   local_node->relKind == RELKIND_COMPOSITE_TYPE ||
		   local_node->relKind == RELKIND_FOREIGN_TABLE ||
		   local_node->relKind == RELKIND_MATVIEW ||
		   IsAppendonlyMetadataRelkind(local_node->relKind));
	Assert(local_node->oncommit <= ONCOMMIT_DROP);
}

static CreateStmt *
_readCreateStmt(void)
{
	READ_LOCALS(CreateStmt);

	_readCreateStmt_common(local_node);

	READ_DONE();
}

static CreateRangeStmt *
_readCreateRangeStmt(void)
{
	READ_LOCALS(CreateRangeStmt);

	READ_NODE_FIELD(typeName);
	READ_NODE_FIELD(params);

	READ_DONE();
}

static CreateForeignTableStmt *
_readCreateForeignTableStmt(void)
{
	READ_LOCALS(CreateForeignTableStmt);

	_readCreateStmt_common(&local_node->base);

	READ_STRING_FIELD(servername);
	READ_NODE_FIELD(options);
	READ_NODE_FIELD(distributedBy);

	READ_DONE();
}

static ColumnReferenceStorageDirective *
_readColumnReferenceStorageDirective(void)
{
	READ_LOCALS(ColumnReferenceStorageDirective);

	READ_STRING_FIELD(column);
	READ_BOOL_FIELD(deflt);
	READ_NODE_FIELD(encoding);

	READ_DONE();
}

static AlterDomainStmt *
_readAlterDomainStmt(void)
{
	READ_LOCALS(AlterDomainStmt);

	READ_CHAR_FIELD(subtype);
	READ_NODE_FIELD(typeName);
	READ_STRING_FIELD(name);
	READ_NODE_FIELD(def);
	READ_ENUM_FIELD(behavior, DropBehavior); Assert(local_node->behavior <= DROP_CASCADE);
	READ_BOOL_FIELD(missing_ok);

	READ_DONE();
}

static AlterDefaultPrivilegesStmt *
_readAlterDefaultPrivilegesStmt(void)
{
	READ_LOCALS(AlterDefaultPrivilegesStmt);

	READ_NODE_FIELD(options);
	READ_NODE_FIELD(action);

	READ_DONE();
}

static CopyStmt *
_readCopyStmt(void)
{
	READ_LOCALS(CopyStmt);

	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(attlist);
	READ_BOOL_FIELD(is_from);
	READ_BOOL_FIELD(is_program);
	READ_STRING_FIELD(filename);
	READ_NODE_FIELD(options);
	READ_NODE_FIELD(sreh);

	READ_DONE();
}

static GrantRoleStmt *
_readGrantRoleStmt(void)
{
	READ_LOCALS(GrantRoleStmt);
	READ_NODE_FIELD(granted_roles);
	READ_NODE_FIELD(grantee_roles);
	READ_BOOL_FIELD(is_grant);
	READ_BOOL_FIELD(admin_opt);
	READ_NODE_FIELD(grantor);
	READ_ENUM_FIELD(behavior, DropBehavior); Assert(local_node->behavior <= DROP_CASCADE);

	READ_DONE();
}

static QueryDispatchDesc *
_readQueryDispatchDesc(void)
{
	READ_LOCALS(QueryDispatchDesc);

	READ_NODE_FIELD(intoCreateStmt);
	READ_NODE_FIELD(paramInfo);
	READ_NODE_FIELD(oidAssignments);
	READ_NODE_FIELD(sliceTable);
	READ_NODE_FIELD(cursorPositions);
	READ_BOOL_FIELD(useChangedAOOpts);
	READ_DONE();
}

static SerializedParams *
_readSerializedParams(void)
{
	READ_LOCALS(SerializedParams);

	READ_INT_FIELD(nExternParams);
	local_node->externParams = palloc0(local_node->nExternParams * sizeof(SerializedParamExternData));
	for (int i = 0; i < local_node->nExternParams; i++)
	{
		READ_BOOL_FIELD(externParams[i].isnull);
		READ_INT_FIELD(externParams[i].pflags);
		READ_OID_FIELD(externParams[i].ptype);
		READ_INT_FIELD(externParams[i].plen);
		READ_BOOL_FIELD(externParams[i].pbyval);

		if (!local_node->externParams[i].isnull)
			local_node->externParams[i].value = readDatum(local_node->externParams[i].pbyval);
	}

	READ_INT_FIELD(nExecParams);
	local_node->execParams = palloc0(local_node->nExecParams * sizeof(SerializedParamExecData));
	for (int i = 0; i < local_node->nExecParams; i++)
	{
		READ_BOOL_FIELD(execParams[i].isnull);
		READ_BOOL_FIELD(execParams[i].isvalid);
		READ_INT_FIELD(execParams[i].plen);
		READ_BOOL_FIELD(execParams[i].pbyval);

		if (local_node->execParams[i].isvalid && !local_node->execParams[i].isnull)
			local_node->execParams[i].value = readDatum(local_node->execParams[i].pbyval);
		READ_BOOL_FIELD(execParams[i].pbyval);
	}

	READ_NODE_FIELD(transientTypes);

	READ_DONE();
}

static OidAssignment *
_readOidAssignment(void)
{
	READ_LOCALS(OidAssignment);

	READ_OID_FIELD(catalog);
	READ_STRING_FIELD(objname);
	READ_OID_FIELD(namespaceOid);
	READ_OID_FIELD(keyOid1);
	READ_OID_FIELD(keyOid2);
	READ_OID_FIELD(oid);
	READ_DONE();
}

static Sequence *
_readSequence(void)
{
	READ_LOCALS(Sequence);
	ReadCommonPlan(&local_node->plan);
	READ_NODE_FIELD(subplans);
	READ_DONE();
}

static DynamicSeqScan *
_readDynamicSeqScan(void)
{
	READ_LOCALS(DynamicSeqScan);

	ReadCommonScan(&local_node->seqscan);
	READ_NODE_FIELD(partOids);

	READ_DONE();
}

/*
 * _readExternalScanInfo
 */
static ExternalScanInfo *
_readExternalScanInfo(void)
{
	READ_LOCALS(ExternalScanInfo);

	READ_NODE_FIELD(uriList);
	READ_CHAR_FIELD(fmtType);
	READ_BOOL_FIELD(isMasterOnly);
	READ_INT_FIELD(rejLimit);
	READ_BOOL_FIELD(rejLimitInRows);
	READ_CHAR_FIELD(logErrors);
	READ_INT_FIELD(encoding);
	READ_INT_FIELD(scancounter);
	READ_NODE_FIELD(extOptions);

	READ_DONE();
}

static CustomScan *
_readCustomScan(void)
{
	char	   *custom_name;
	const CustomScanMethods *methods;
	
	READ_LOCALS(CustomScan);

	ReadCommonScan(&local_node->scan);

	READ_UINT_FIELD(flags);
	READ_NODE_FIELD(custom_plans);
	READ_NODE_FIELD(custom_exprs);
	READ_NODE_FIELD(custom_private);
	READ_NODE_FIELD(custom_scan_tlist);
	READ_BITMAPSET_FIELD(custom_relids);
	READ_STRING_VAR(custom_name);
	/* find custom scan methods from hash table. */
	methods = GetCustomScanMethods(custom_name, false);
	local_node->methods = methods;

	READ_DONE();
}

/*
 * _readShareInputScan
 */
static ShareInputScan *
_readShareInputScan(void)
{
	READ_LOCALS(ShareInputScan);

	READ_BOOL_FIELD(cross_slice);
	READ_INT_FIELD(share_id);
	READ_INT_FIELD(producer_slice_id);
	READ_INT_FIELD(this_slice_id);
	READ_INT_FIELD(nconsumers);

	ReadCommonPlan(&local_node->scan.plan);

	READ_DONE();
}

/*
 * _readMotion
 */
static Motion *
_readMotion(void)
{
	READ_LOCALS(Motion);

	READ_INT_FIELD(motionID);
	READ_ENUM_FIELD(motionType, MotionType);

	Assert(local_node->motionType == MOTIONTYPE_GATHER ||
		   local_node->motionType == MOTIONTYPE_GATHER_SINGLE ||
		   local_node->motionType == MOTIONTYPE_HASH ||
		   local_node->motionType == MOTIONTYPE_BROADCAST ||
		   local_node->motionType == MOTIONTYPE_EXPLICIT);

	READ_BOOL_FIELD(sendSorted);

	READ_NODE_FIELD(hashExprs);
	READ_OID_ARRAY(hashFuncs, list_length(local_node->hashExprs));

	READ_INT_FIELD(numSortCols);
	READ_ATTRNUMBER_ARRAY(sortColIdx, local_node->numSortCols);
	READ_OID_ARRAY(sortOperators, local_node->numSortCols);
	READ_OID_ARRAY(collations, local_node->numSortCols);
	READ_BOOL_ARRAY(nullsFirst, local_node->numSortCols);

	READ_INT_FIELD(segidColIdx);
	READ_INT_FIELD(numHashSegments);

	ReadCommonPlan(&local_node->plan);

	READ_DONE();
}

/*
 * _readSplitUpdate
 */
static SplitUpdate *
_readSplitUpdate(void)
{
	READ_LOCALS(SplitUpdate);

	READ_INT_FIELD(actionColIdx);
	READ_INT_FIELD(tupleoidColIdx);
	READ_NODE_FIELD(insertColIdx);
	READ_NODE_FIELD(deleteColIdx);

	READ_INT_FIELD(numHashSegments);
	READ_INT_FIELD(numHashAttrs);
	READ_ATTRNUMBER_ARRAY(hashAttnos, local_node->numHashAttrs);
	READ_OID_ARRAY(hashFuncs, local_node->numHashAttrs);

	ReadCommonPlan(&local_node->plan);

	READ_DONE();
}

/*
 * _readAssertOp
 */
static AssertOp *
_readAssertOp(void)
{
	READ_LOCALS(AssertOp);

	READ_NODE_FIELD(errmessage);
	READ_INT_FIELD(errcode);

	ReadCommonPlan(&local_node->plan);

	READ_DONE();
}

/*
 * _readPartitionSelector
 */
static PartitionSelector *
_readPartitionSelector(void)
{
	READ_LOCALS(PartitionSelector);

	READ_INT_FIELD(paramid);
	READ_NODE_FIELD(part_prune_info);

	ReadCommonPlan(&local_node->plan);

	READ_DONE();
}

static Bitmapset *
_readBitmapset(void)
{
	Bitmapset  *bms = NULL;
	int			nwords;
	int			i;

	memcpy(&nwords, read_str_ptr, sizeof(int)); read_str_ptr+=sizeof(int);
	if (nwords==0)
		return bms;

	bms = palloc(offsetof(Bitmapset, words)+nwords*sizeof(bitmapword));
	bms->nwords = nwords;
	for (i = 0; i < nwords; i++)
	{
		memcpy(&bms->words[i], read_str_ptr, sizeof(bitmapword)); read_str_ptr+=sizeof(bitmapword);
	}

	return bms;
}

static CreateTrigStmt *
_readCreateTrigStmt(void)
{
	READ_LOCALS(CreateTrigStmt);

	READ_STRING_FIELD(trigname);
	READ_NODE_FIELD(relation);
	READ_NODE_FIELD(funcname);
	READ_NODE_FIELD(args);
	READ_BOOL_FIELD(row);
	READ_INT_FIELD(timing);
	READ_INT_FIELD(events);
	READ_NODE_FIELD(columns);
	READ_NODE_FIELD(whenClause);
	READ_BOOL_FIELD(isconstraint);
	READ_NODE_FIELD(transitionRels);
	READ_BOOL_FIELD(deferrable);
	READ_BOOL_FIELD(initdeferred);
	READ_NODE_FIELD(constrrel);

	READ_DONE();
}

static TriggerTransition *
_readTriggerTransition()
{
	READ_LOCALS(TriggerTransition);

	READ_STRING_FIELD(name);
	READ_BOOL_FIELD(isNew);
	READ_BOOL_FIELD(isTable);

	READ_DONE();
}

static CreateTableSpaceStmt *
_readCreateTableSpaceStmt(void)
{
	READ_LOCALS(CreateTableSpaceStmt);

	READ_STRING_FIELD(tablespacename);
	READ_NODE_FIELD(owner);
	READ_STRING_FIELD(location);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static CreateAmStmt *
_readCreateAmStmt()
{
	READ_LOCALS(CreateAmStmt);

	READ_STRING_FIELD(amname);
	READ_NODE_FIELD(handler_name);
	READ_INT_FIELD(amtype);

	READ_DONE();
}

static AlterTableMoveAllStmt *
_readAlterTableMoveAllStmt(void)
{
	READ_LOCALS(AlterTableMoveAllStmt);

	READ_STRING_FIELD(orig_tablespacename);
	READ_ENUM_FIELD(objtype, ObjectType);
	READ_NODE_FIELD(roles);
	READ_STRING_FIELD(new_tablespacename);
	READ_BOOL_FIELD(nowait);

	READ_DONE();
}

static AlterTableSpaceOptionsStmt *
_readAlterTableSpaceOptionsStmt(void)
{
	READ_LOCALS(AlterTableSpaceOptionsStmt);

	READ_STRING_FIELD(tablespacename);
	READ_NODE_FIELD(options);
	READ_BOOL_FIELD(isReset);

	READ_DONE();
}

static DropTableSpaceStmt *
_readDropTableSpaceStmt(void)
{
	READ_LOCALS(DropTableSpaceStmt);

	READ_STRING_FIELD(tablespacename);
	READ_BOOL_FIELD(missing_ok);

	READ_DONE();
}


static CreateQueueStmt *
_readCreateQueueStmt(void)
{
	READ_LOCALS(CreateQueueStmt);

	READ_STRING_FIELD(queue);
	READ_NODE_FIELD(options);

	READ_DONE();
}
static AlterQueueStmt *
_readAlterQueueStmt(void)
{
	READ_LOCALS(AlterQueueStmt);

	READ_STRING_FIELD(queue);
	READ_NODE_FIELD(options);

	READ_DONE();
}
static DropQueueStmt *
_readDropQueueStmt(void)
{
	READ_LOCALS(DropQueueStmt);

	READ_STRING_FIELD(queue);

	READ_DONE();
}

static CreateResourceGroupStmt *
_readCreateResourceGroupStmt(void)
{
	READ_LOCALS(CreateResourceGroupStmt);

	READ_STRING_FIELD(name);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static DropResourceGroupStmt *
_readDropResourceGroupStmt(void)
{
	READ_LOCALS(DropResourceGroupStmt);

	READ_STRING_FIELD(name);

	READ_DONE();
}

static AlterResourceGroupStmt *
_readAlterResourceGroupStmt(void)
{
	READ_LOCALS(AlterResourceGroupStmt);

	READ_STRING_FIELD(name);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static CommentStmt *
_readCommentStmt(void)
{
	READ_LOCALS(CommentStmt);

	READ_ENUM_FIELD(objtype, ObjectType);
	READ_NODE_FIELD(object);
	READ_STRING_FIELD(comment);

	READ_DONE();
}

static TupleDescNode *
_readTupleDescNode(void)
{
	READ_LOCALS(TupleDescNode);

	READ_INT_FIELD(natts);

	local_node->tuple = CreateTemplateTupleDesc(local_node->natts);

	READ_INT_FIELD(tuple->natts);
	if (local_node->tuple->natts > 0)
	{
		int i = 0;
		for (; i < local_node->tuple->natts; i++)
		{
			memcpy(&local_node->tuple->attrs[i], read_str_ptr, ATTRIBUTE_FIXED_PART_SIZE);
			read_str_ptr+=ATTRIBUTE_FIXED_PART_SIZE;
		}
	}

	READ_OID_FIELD(tuple->tdtypeid);
	READ_INT_FIELD(tuple->tdtypmod);
	READ_INT_FIELD(tuple->tdrefcount);

	// Transient type don't have constraint.
	local_node->tuple->constr = NULL;

	Assert(local_node->tuple->tdtypeid == RECORDOID);

	READ_DONE();
}

static AlterExtensionStmt *
_readAlterExtensionStmt(void)
{
	READ_LOCALS(AlterExtensionStmt);
	READ_STRING_FIELD(extname);
	READ_NODE_FIELD(options);
	READ_ENUM_FIELD(update_ext_state, UpdateExtensionState);
	READ_DONE();
}

static AlterExtensionContentsStmt *
_readAlterExtensionContentsStmt(void)
{
	READ_LOCALS(AlterExtensionContentsStmt);

	READ_STRING_FIELD(extname);
	READ_INT_FIELD(action);
	READ_ENUM_FIELD(objtype, ObjectType);
	READ_NODE_FIELD(object);

	READ_DONE();
}

static AlterTSConfigurationStmt *
_readAlterTSConfigurationStmt(void)
{
	READ_LOCALS(AlterTSConfigurationStmt);

	READ_NODE_FIELD(cfgname);
	READ_NODE_FIELD(tokentype);
	READ_NODE_FIELD(dicts);
	READ_BOOL_FIELD(override);
	READ_BOOL_FIELD(replace);
	READ_BOOL_FIELD(missing_ok);

	READ_DONE();
}

static AlterTSDictionaryStmt *
_readAlterTSDictionaryStmt(void)
{
	READ_LOCALS(AlterTSDictionaryStmt);

	READ_NODE_FIELD(dictname);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static CookedConstraint *
_readCookedConstraint(void)
{
	READ_LOCALS(CookedConstraint);

	READ_ENUM_FIELD(contype,ConstrType);
	READ_STRING_FIELD(name);
	READ_INT_FIELD(attnum);
	READ_NODE_FIELD(expr);
	READ_BOOL_FIELD(is_local);
	READ_INT_FIELD(inhcount);
	READ_BOOL_FIELD(is_no_inherit);

	READ_DONE();
}

static AlterEnumStmt *
_readAlterEnumStmt(void)
{
	READ_LOCALS(AlterEnumStmt);

	READ_NODE_FIELD(typeName);
	READ_STRING_FIELD(oldVal);
	READ_STRING_FIELD(newVal);
	READ_STRING_FIELD(newValNeighbor);
	READ_BOOL_FIELD(newValIsAfter);
	READ_BOOL_FIELD(skipIfNewValExists);

	READ_DONE();
}

static CreateFdwStmt *
_readCreateFdwStmt(void)
{
	READ_LOCALS(CreateFdwStmt);

	READ_STRING_FIELD(fdwname);
	READ_NODE_FIELD(func_options);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static DistributedBy*
_readDistributedBy(void)
{
	READ_LOCALS(DistributedBy);

	READ_ENUM_FIELD(ptype, GpPolicyType);
	READ_INT_FIELD(numsegments);
	READ_NODE_FIELD(keyCols);

	READ_DONE();
}

static ImportForeignSchemaStmt*
_readImportForeignSchemaStmt(void)
{
	READ_LOCALS(ImportForeignSchemaStmt);

	READ_STRING_FIELD(server_name);
	READ_STRING_FIELD(remote_schema);
	READ_STRING_FIELD(local_schema);
	READ_ENUM_FIELD(list_type, ImportForeignSchemaType);
	READ_NODE_FIELD(table_list);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static AlterFdwStmt *
_readAlterFdwStmt(void)
{
	READ_LOCALS(AlterFdwStmt);

	READ_STRING_FIELD(fdwname);
	READ_NODE_FIELD(func_options);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static CreateForeignServerStmt *
_readCreateForeignServerStmt(void)
{
	READ_LOCALS(CreateForeignServerStmt);

	READ_STRING_FIELD(servername);
	READ_STRING_FIELD(servertype);
	READ_STRING_FIELD(version);
	READ_STRING_FIELD(fdwname);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static AlterForeignServerStmt *
_readAlterForeignServerStmt(void)
{
	READ_LOCALS(AlterForeignServerStmt);

	READ_STRING_FIELD(servername);
	READ_STRING_FIELD(version);
	READ_NODE_FIELD(options);
	READ_BOOL_FIELD(has_version);

	READ_DONE();
}

static CreateUserMappingStmt *
_readCreateUserMappingStmt(void)
{
	READ_LOCALS(CreateUserMappingStmt);

	READ_NODE_FIELD(user);
	READ_STRING_FIELD(servername);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static AlterUserMappingStmt *
_readAlterUserMappingStmt(void)
{
	READ_LOCALS(AlterUserMappingStmt);

	READ_NODE_FIELD(user);
	READ_STRING_FIELD(servername);
	READ_NODE_FIELD(options);

	READ_DONE();
}

static DropUserMappingStmt *
_readDropUserMappingStmt(void)
{
	READ_LOCALS(DropUserMappingStmt);

	READ_NODE_FIELD(user);
	READ_STRING_FIELD(servername);
	READ_BOOL_FIELD(missing_ok);

	READ_DONE();
}

static AccessPriv *
_readAccessPriv(void)
{
	READ_LOCALS(AccessPriv);

	READ_STRING_FIELD(priv_name);
	READ_NODE_FIELD(cols);

	READ_DONE();
}

static LockingClause *
_readLockingClause(void)
{
	READ_LOCALS(LockingClause);

	READ_NODE_FIELD(lockedRels);
	READ_ENUM_FIELD(strength, LockClauseStrength);

	READ_DONE();
}

static AggExprId *
_readAggExprId(void)
{
	READ_LOCALS(AggExprId);
	READ_DONE();
}

static RowIdExpr *
_readRowIdExpr(void)
{
	READ_LOCALS(RowIdExpr);
	READ_INT_FIELD(rowidexpr_id);
	READ_DONE();
}

static Node *
_readValue(NodeTag nt)
{
	Node * result = NULL;
	if (nt == T_Integer)
	{
		long ival;
		memcpy(&ival, read_str_ptr, sizeof(long)); read_str_ptr+=sizeof(long);
		result = (Node *) makeInteger(ival);
	}
	else if (nt == T_Null)
	{
		Value *val = makeNode(Value);
		val->type = T_Null;
		result = (Node *)val;
	}
	else
	{
		int slen;
		char * nn = NULL;
		memcpy(&slen, read_str_ptr, sizeof(int));
		read_str_ptr+=sizeof(int);

		/*
		 * For the String case we want to create an empty string if slen is
		 * equal to zero, since otherwise we'll set the string to NULL, which
		 * has a different meaning and the NULL case is handed above.
		 */
		if (slen > 0 || nt == T_String)
		{
		    nn = palloc(slen + 1);

			if (slen > 0)
			    memcpy(nn, read_str_ptr, slen);

		    read_str_ptr += (slen);
			nn[slen] = '\0';
		}

		if (nt == T_Float)
			result = (Node *) makeFloat(nn);
		else if (nt == T_String)
			result = (Node *) makeString(nn);
		else if (nt == T_BitString)
			result = (Node *) makeBitString(nn);
		else
			elog(ERROR, "unknown Value node type %i", nt);
	}

	return result;

}

/*
 * _readGpPolicy
 */
static GpPolicy *
_readGpPolicy(void)
{
	READ_LOCALS(GpPolicy);

	READ_ENUM_FIELD(ptype, GpPolicyType);

	READ_INT_FIELD(numsegments);

	READ_INT_FIELD(nattrs);
	READ_ATTRNUMBER_ARRAY(attrs, local_node->nattrs);
	READ_OID_ARRAY(opclasses, local_node->nattrs);

	READ_DONE();
}


static void *
readNodeBinary(void)
{
	void	   *return_value;
	NodeTag 	nt;
	int16       ntt;

	memcpy(&ntt, read_str_ptr,sizeof(int16));
	read_str_ptr+=sizeof(int16);
	nt = (NodeTag) ntt;

	if (nt==0)
		return NULL;

	if (nt == T_List || nt == T_IntList || nt == T_OidList)
	{
		List	   *l = NIL;
		int listsize = 0;
		int i;

		memcpy(&listsize,read_str_ptr,sizeof(int));
		read_str_ptr+=sizeof(int);

		if (nt == T_IntList)
		{
			int val;
			for (i = 0; i < listsize; i++)
			{
				memcpy(&val,read_str_ptr,sizeof(int));
				read_str_ptr+=sizeof(int);
				l = lappend_int(l, val);
			}
		}
		else if (nt == T_OidList)
		{
			Oid val;
			for (i = 0; i < listsize; i++)
			{
				memcpy(&val,read_str_ptr,sizeof(Oid));
				read_str_ptr+=sizeof(Oid);
				l = lappend_oid(l, val);
			}
		}
		else
		{

			for (i = 0; i < listsize; i++)
			{
				l = lappend(l, readNodeBinary());
			}
		}
		Assert(l->length==listsize);

		return l;
	}

	if (nt == T_Integer || nt == T_Float || nt == T_String ||
	   	nt == T_BitString || nt == T_Null)
	{
		return _readValue(nt);
	}

	switch(nt)
	{
			case T_PlannedStmt:
				return_value = _readPlannedStmt();
				break;
			case T_QueryDispatchDesc:
				return_value = _readQueryDispatchDesc();
				break;
			case T_OidAssignment:
				return_value = _readOidAssignment();
				break;
			case T_Plan:
				return_value = _readPlan();
				break;
			case T_Result:
				return_value = _readResult();
				break;
			case T_ProjectSet:
				return_value = _readProjectSet();
				break;
			case T_Append:
				return_value = _readAppend();
				break;
			case T_MergeAppend:
				return_value = _readMergeAppend();
				break;
			case T_Sequence:
				return_value = _readSequence();
				break;
			case T_RecursiveUnion:
				return_value = _readRecursiveUnion();
				break;
			case T_BitmapAnd:
				return_value = _readBitmapAnd();
				break;
			case T_BitmapOr:
				return_value = _readBitmapOr();
				break;
			case T_Gather:
				return_value = _readGather();
				break;
			case T_GatherMerge:
				return_value = _readGatherMerge();
				break;
			case T_Scan:
				return_value = _readScan();
				break;
			case T_SeqScan:
				return_value = _readSeqScan();
				break;
			case T_DynamicSeqScan:
				return_value = _readDynamicSeqScan();
				break;
			case T_ExternalScanInfo:
				return_value = _readExternalScanInfo();
				break;
			case T_IndexScan:
				return_value = _readIndexScan();
				break;
			case T_IndexOnlyScan:
				return_value = _readIndexOnlyScan();
				break;
			case T_DynamicIndexScan:
				return_value = _readDynamicIndexScan();
				break;
			case T_BitmapIndexScan:
				return_value = _readBitmapIndexScan();
				break;
			case T_DynamicBitmapIndexScan:
				return_value = _readDynamicBitmapIndexScan();
				break;
			case T_BitmapHeapScan:
				return_value = _readBitmapHeapScan();
				break;
			case T_DynamicBitmapHeapScan:
				return_value = _readDynamicBitmapHeapScan();
				break;
			case T_CteScan:
				return_value = _readCteScan();
				break;
			case T_NamedTuplestoreScan:
				return_value = _readNamedTuplestoreScan();
				break;
			case T_WorkTableScan:
				return_value = _readWorkTableScan();
				break;
			case T_TidScan:
				return_value = _readTidScan();
				break;
			case T_SubqueryScan:
				return_value = _readSubqueryScan();
				break;
			case T_FunctionScan:
				return_value = _readFunctionScan();
				break;
			case T_TableFuncScan:
				return_value = _readTableFuncScan();
				break;
			case T_ValuesScan:
				return_value = _readValuesScan();
				break;
			case T_ForeignScan:
				return_value = _readForeignScan();
				break;
			case T_CustomScan:
				return_value = _readCustomScan();
				break;
			case T_SampleScan:
				return_value = _readSampleScan();
				break;
			case T_Join:
				return_value = _readJoin();
				break;
			case T_NestLoop:
				return_value = _readNestLoop();
				break;
			case T_MergeJoin:
				return_value = _readMergeJoin();
				break;
			case T_HashJoin:
				return_value = _readHashJoin();
				break;
			case T_Agg:
				return_value = _readAgg();
				break;
			case T_TupleSplit:
				return_value = _readTupleSplit();
				break;
			case T_DQAExpr:
				return_value = _readDQAExpr();
				break;
			case T_WindowAgg:
				return_value = _readWindowAgg();
				break;
			case T_TableFunctionScan:
				return_value = _readTableFunctionScan();
				break;
			case T_Material:
				return_value = _readMaterial();
				break;
			case T_ShareInputScan:
				return_value = _readShareInputScan();
				break;
			case T_Sort:
				return_value = _readSort();
				break;
			case T_Unique:
				return_value = _readUnique();
				break;
			case T_SetOp:
				return_value = _readSetOp();
				break;
			case T_Limit:
				return_value = _readLimit();
				break;
			case T_NestLoopParam:
				return_value = _readNestLoopParam();
				break;
			case T_PlanRowMark:
				return_value = _readPlanRowMark();
				break;
			case T_PartitionPruneInfo:
				return_value = _readPartitionPruneInfo();
				break;
			case T_PartitionedRelPruneInfo:
				return_value = _readPartitionedRelPruneInfo();
				break;
			case T_PartitionPruneStepOp:
				return_value = _readPartitionPruneStepOp();
				break;
			case T_PartitionPruneStepCombine:
				return_value = _readPartitionPruneStepCombine();
				break;
			case T_PlanInvalItem:
				return_value = _readPlanInvalItem();
				break;
			case T_Hash:
				return_value = _readHash();
				break;
			case T_Motion:
				return_value = _readMotion();
				break;
			case T_SplitUpdate:
				return_value = _readSplitUpdate();
				break;
			case T_AssertOp:
				return_value = _readAssertOp();
				break;
			case T_PartitionSelector:
				return_value = _readPartitionSelector();
				break;
			case T_Alias:
				return_value = _readAlias();
				break;
			case T_RangeVar:
				return_value = _readRangeVar();
				break;
			case T_TableFunc:
				return_value = _readTableFunc();
				break;
			case T_IntoClause:
				return_value = _readIntoClause();
				break;
			case T_CopyIntoClause:
				return_value = _readCopyIntoClause();
				break;
			case T_RefreshClause:
				return_value = _readRefreshClause();
				break;
			case T_Var:
				return_value = _readVar();
				break;
			case T_Const:
				return_value = _readConst();
				break;
			case T_Param:
				return_value = _readParam();
				break;
			case T_Aggref:
				return_value = _readAggref();
				break;
			case T_GroupingFunc:
				return_value = _readGroupingFunc();
				break;
			case T_GroupId:
				return_value = _readGroupId();
				break;
			case T_GroupingSetId:
				return_value = _readGroupingSetId();
				break;
			case T_WindowFunc:
				return_value = _readWindowFunc();
				break;
			case T_SubscriptingRef:
				return_value = _readSubscriptingRef();
				break;
			case T_FuncExpr:
				return_value = _readFuncExpr();
				break;
			case T_NamedArgExpr:
				return_value = _readNamedArgExpr();
				break;
			case T_OpExpr:
				return_value = _readOpExpr();
				break;
			case T_DistinctExpr:
				return_value = _readDistinctExpr();
				break;
			case T_ScalarArrayOpExpr:
				return_value = _readScalarArrayOpExpr();
				break;
			case T_BoolExpr:
				return_value = _readBoolExpr();
				break;
			case T_SubLink:
				return_value = _readSubLink();
				break;
			case T_SubPlan:
				return_value = _readSubPlan();
				break;
			case T_AlternativeSubPlan:
				return_value = _readAlternativeSubPlan();
				break;
			case T_FieldSelect:
				return_value = _readFieldSelect();
				break;
			case T_FieldStore:
				return_value = _readFieldStore();
				break;
			case T_RelabelType:
				return_value = _readRelabelType();
				break;
			case T_CoerceViaIO:
				return_value = _readCoerceViaIO();
				break;
			case T_ArrayCoerceExpr:
				return_value = _readArrayCoerceExpr();
				break;
			case T_ConvertRowtypeExpr:
				return_value = _readConvertRowtypeExpr();
				break;
			case T_CollateExpr:
				return_value = _readCollateExpr();
				break;
			case T_CaseExpr:
				return_value = _readCaseExpr();
				break;
			case T_CaseWhen:
				return_value = _readCaseWhen();
				break;
			case T_CaseTestExpr:
				return_value = _readCaseTestExpr();
				break;
			case T_ArrayExpr:
				return_value = _readArrayExpr();
				break;
			case T_A_ArrayExpr:
				return_value = _readA_ArrayExpr();
				break;
			case T_RowExpr:
				return_value = _readRowExpr();
				break;
			case T_RowCompareExpr:
				return_value = _readRowCompareExpr();
				break;
			case T_CoalesceExpr:
				return_value = _readCoalesceExpr();
				break;
			case T_MinMaxExpr:
				return_value = _readMinMaxExpr();
				break;
			case T_NullIfExpr:
				return_value = _readNullIfExpr();
				break;
			case T_NullTest:
				return_value = _readNullTest();
				break;
			case T_BooleanTest:
				return_value = _readBooleanTest();
				break;
			case T_SQLValueFunction:
				return_value = _readSQLValueFunction();
				break;
			case T_XmlExpr:
				return_value = _readXmlExpr();
				break;
			case T_CoerceToDomain:
				return_value = _readCoerceToDomain();
				break;
			case T_CoerceToDomainValue:
				return_value = _readCoerceToDomainValue();
				break;
			case T_SetToDefault:
				return_value = _readSetToDefault();
				break;
			case T_CurrentOfExpr:
				return_value = _readCurrentOfExpr();
				break;
			case T_NextValueExpr:
				return_value = _readNextValueExpr();
				break;
			case T_InferenceElem:
				return_value = _readInferenceElem();
				break;
			case T_TargetEntry:
				return_value = _readTargetEntry();
				break;
			case T_RangeTblRef:
				return_value = _readRangeTblRef();
				break;
			case T_RangeTblFunction:
				return_value = _readRangeTblFunction();
				break;
			case T_TableSampleClause:
				return_value = _readTableSampleClause();
				break;
			case T_JoinExpr:
				return_value = _readJoinExpr();
				break;
			case T_FromExpr:
				return_value = _readFromExpr();
				break;
			case T_OnConflictExpr:
				return_value = _readOnConflictExpr();
				break;
			case T_GrantStmt:
				return_value = _readGrantStmt();
				break;
			case T_AccessPriv:
				return_value = _readAccessPriv();
				break;
			case T_ObjectWithArgs:
				return_value = _readObjectWithArgs();
				break;
			case T_GrantRoleStmt:
				return_value = _readGrantRoleStmt();
				break;
			case T_LockStmt:
				return_value = _readLockStmt();
				break;

			case T_PartitionSpec:
				return_value = _readPartitionSpec();
				break;
			case T_PartitionElem:
				return_value = _readPartitionElem();
				break;
			case T_PartitionRangeDatum:
				return_value = _readPartitionRangeDatum();
				break;
			case T_PartitionCmd:
				return_value = _readPartitionCmd();
				break;
			case T_DistributionKeyElem:
				return_value = _readDistributionKeyElem();
				break;
			case T_PartitionBoundSpec:
				return_value = _readPartitionBoundSpec();
				break;
			case T_RestrictInfo:
				return_value = _readRestrictInfo();
				break;
			case T_ExtensibleNode:
				return_value = _readExtensibleNode();
				break;
			case T_CreateStmt:
				return_value = _readCreateStmt();
				break;
			case T_CreateForeignTableStmt:
				return_value = _readCreateForeignTableStmt();
				break;
			case T_ColumnReferenceStorageDirective:
				return_value = _readColumnReferenceStorageDirective();
				break;
			case T_SegfileMapNode:
				return_value = _readSegfileMapNode();
				break;
			case T_ExtTableTypeDesc:
				return_value = _readExtTableTypeDesc();
				break;
			case T_CreateExternalStmt:
				return_value = _readCreateExternalStmt();
				break;
			case T_CreateExtensionStmt:
				return_value = _readCreateExtensionStmt();
				break;
			case T_IndexStmt:
				return_value = _readIndexStmt();
				break;
			case T_ReindexStmt:
				return_value = _readReindexStmt();
				break;

			case T_ConstraintsSetStmt:
				return_value = _readConstraintsSetStmt();
				break;

			case T_CreateFunctionStmt:
				return_value = _readCreateFunctionStmt();
				break;
			case T_FunctionParameter:
				return_value = _readFunctionParameter();
				break;
			case T_AlterFunctionStmt:
				return_value = _readAlterFunctionStmt();
				break;

			case T_DefineStmt:
				return_value = _readDefineStmt();
				break;

			case T_CompositeTypeStmt:
				return_value = _readCompositeTypeStmt();
				break;
			case T_CreateEnumStmt:
				return_value = _readCreateEnumStmt();
				break;
			case T_CreateRangeStmt:
				return_value = _readCreateRangeStmt();
				break;
			case T_AlterEnumStmt:
				return_value = _readAlterEnumStmt();
				break;
			case T_CreateCastStmt:
				return_value = _readCreateCastStmt();
				break;
			case T_CreateOpClassStmt:
				return_value = _readCreateOpClassStmt();
				break;
			case T_CreateOpClassItem:
				return_value = _readCreateOpClassItem();
				break;
			case T_CreateOpFamilyStmt:
				return_value = _readCreateOpFamilyStmt();
				break;
			case T_AlterOpFamilyStmt:
				return_value = _readAlterOpFamilyStmt();
				break;
			case T_CreateConversionStmt:
				return_value = _readCreateConversionStmt();
				break;
			case T_ViewStmt:
				return_value = _readViewStmt();
				break;
			case T_RuleStmt:
				return_value = _readRuleStmt();
				break;
			case T_DropStmt:
				return_value = _readDropStmt();
				break;

			case T_DropOwnedStmt:
				return_value = _readDropOwnedStmt();
				break;
			case T_ReassignOwnedStmt:
				return_value = _readReassignOwnedStmt();
				break;

			case T_TruncateStmt:
				return_value = _readTruncateStmt();
				break;

			case T_ReplicaIdentityStmt:
				return_value = _readReplicaIdentityStmt();
				break;
			case T_AlterTableStmt:
				return_value = _readAlterTableStmt();
				break;
			case T_AlterTableCmd:
				return_value = _readAlterTableCmd();
				break;
			case T_AlteredTableInfo:
				return_value = _readAlteredTableInfo();
				break;
			case T_NewConstraint:
				return_value = _readNewConstraint();
				break;
			case T_NewColumnValue:
				return_value = _readNewColumnValue();
				break;

			case T_CreateRoleStmt:
				return_value = _readCreateRoleStmt();
				break;
			case T_DropRoleStmt:
				return_value = _readDropRoleStmt();
				break;
			case T_AlterRoleStmt:
				return_value = _readAlterRoleStmt();
				break;
			case T_AlterRoleSetStmt:
				return_value = _readAlterRoleSetStmt();
				break;

			case T_AlterObjectDependsStmt:
				return_value = _readAlterObjectDependsStmt();
				break;

			case T_AlterObjectSchemaStmt:
				return_value = _readAlterObjectSchemaStmt();
				break;

			case T_AlterOwnerStmt:
				return_value = _readAlterOwnerStmt();
				break;

			case T_RenameStmt:
				return_value = _readRenameStmt();
				break;

			case T_CreateSeqStmt:
				return_value = _readCreateSeqStmt();
				break;
			case T_AlterSeqStmt:
				return_value = _readAlterSeqStmt();
				break;
			case T_ClusterStmt:
				return_value = _readClusterStmt();
				break;
			case T_CreatedbStmt:
				return_value = _readCreatedbStmt();
				break;
			case T_DropdbStmt:
				return_value = _readDropdbStmt();
				break;
			case T_CreateDomainStmt:
				return_value = _readCreateDomainStmt();
				break;
			case T_AlterDomainStmt:
				return_value = _readAlterDomainStmt();
				break;
			case T_AlterDefaultPrivilegesStmt:
				return_value = _readAlterDefaultPrivilegesStmt();
				break;

			case T_NotifyStmt:
				return_value = _readNotifyStmt();
				break;
			case T_DeclareCursorStmt:
				return_value = _readDeclareCursorStmt();
				break;

			case T_SingleRowErrorDesc:
				return_value = _readSingleRowErrorDesc();
				break;
			case T_CopyStmt:
				return_value = _readCopyStmt();
				break;
			case T_SelectStmt:
				return_value = _readSelectStmt();
				break;
			case T_InsertStmt:
				return_value = _readInsertStmt();
				break;
			case T_DeleteStmt:
				return_value = _readDeleteStmt();
				break;
			case T_UpdateStmt:
				return_value = _readUpdateStmt();
				break;
			case T_ColumnDef:
				return_value = _readColumnDef();
				break;
			case T_TypeName:
				return_value = _readTypeName();
				break;
			case T_SortBy:
				return_value = _readSortBy();
				break;
			case T_TypeCast:
				return_value = _readTypeCast();
				break;
			case T_CollateClause:
				return_value = _readCollateClause();
				break;
			case T_IndexElem:
				return_value = _readIndexElem();
				break;
			case T_Query:
				return_value = _readQuery();
				break;
			case T_WithCheckOption:
				return_value = _readWithCheckOption();
				break;
			case T_SortGroupClause:
				return_value = _readSortGroupClause();
				break;
			case T_DMLActionExpr:
				return_value = _readDMLActionExpr();
				break;
			case T_GroupingSet:
				return_value = _readGroupingSet();
				break;
			case T_WindowClause:
				return_value = _readWindowClause();
				break;
			case T_RowMarkClause:
				return_value = _readRowMarkClause();
				break;
			case T_WithClause:
				return_value = _readWithClause();
				break;
			case T_CommonTableExpr:
				return_value = _readCommonTableExpr();
				break;
			case T_RoleSpec:
				return_value = _readRoleSpec();
				break;
			case T_SetOperationStmt:
				return_value = _readSetOperationStmt();
				break;
			case T_RangeTblEntry:
				return_value = _readRangeTblEntry();
				break;
			case T_A_Expr:
				return_value = _readAExpr();
				break;
			case T_ColumnRef:
				return_value = _readColumnRef();
				break;
			case T_A_Const:
				return_value = _readAConst();
				break;
			case T_A_Star:
				return_value = _readA_Star();
				break;
			case T_A_Indices:
				return_value = _readA_Indices();
				break;
			case T_A_Indirection:
				return_value = _readA_Indirection();
				break;
			case T_ResTarget:
				return_value = _readResTarget();
				break;
			case T_MultiAssignRef:
				return_value = _readMultiAssignRef();
				break;
			case T_Constraint:
				return_value = _readConstraint();
				break;
			case T_FuncCall:
				return_value = _readFuncCall();
				break;
			case T_DefElem:
				return_value = _readDefElem();
				break;
			case T_CreateSchemaStmt:
				return_value = _readCreateSchemaStmt();
				break;
			case T_CreatePLangStmt:
				return_value = _readCreatePLangStmt();
				break;
			case T_VacuumStmt:
				return_value = _readVacuumStmt();
				break;
			case T_VacuumRelation:
				return_value = _readVacuumRelation();
				break;
			case T_CdbProcess:
				return_value = _readCdbProcess();
				break;
			case T_SliceTable:
				return_value = _readSliceTable();
				break;
			case T_CursorPosInfo:
				return_value = _readCursorPosInfo();
				break;
			case T_VariableSetStmt:
				return_value = _readVariableSetStmt();
				break;
			case T_CreateTrigStmt:
				return_value = _readCreateTrigStmt();
				break;
			case T_TriggerTransition:
				return_value = _readTriggerTransition();
				break;

			case T_CreateTableSpaceStmt:
				return_value = _readCreateTableSpaceStmt();
				break;
			case T_AlterTableSpaceOptionsStmt:
				return_value = _readAlterTableSpaceOptionsStmt();
				break;
			case T_DropTableSpaceStmt:
				return_value = _readDropTableSpaceStmt();
				break;

			case T_CreateQueueStmt:
				return_value = _readCreateQueueStmt();
				break;
			case T_AlterQueueStmt:
				return_value = _readAlterQueueStmt();
				break;
			case T_DropQueueStmt:
				return_value = _readDropQueueStmt();
				break;

			case T_CreateResourceGroupStmt:
				return_value = _readCreateResourceGroupStmt();
				break;
			case T_DropResourceGroupStmt:
				return_value = _readDropResourceGroupStmt();
				break;
			case T_AlterResourceGroupStmt:
				return_value = _readAlterResourceGroupStmt();
				break;

			case T_CommentStmt:
				return_value = _readCommentStmt();
				break;
			case T_DenyLoginInterval:
				return_value = _readDenyLoginInterval();
				break;
			case T_DenyLoginPoint:
				return_value = _readDenyLoginPoint();
				break;

			case T_TableValueExpr:
				return_value = _readTableValueExpr();
				break;

			case T_AlterTypeStmt:
				return_value = _readAlterTypeStmt();
				break;
			case T_AlterExtensionStmt:
				return_value = _readAlterExtensionStmt();
				break;
			case T_AlterExtensionContentsStmt:
				return_value = _readAlterExtensionContentsStmt();
				break;

			case T_TupleDescNode:
				return_value = _readTupleDescNode();
				break;
			case T_SerializedParams:
				return_value = _readSerializedParams();
				break;

			case T_AlterTSConfigurationStmt:
				return_value = _readAlterTSConfigurationStmt();
				break;
			case T_AlterTSDictionaryStmt:
				return_value = _readAlterTSDictionaryStmt();
				break;

			case T_CookedConstraint:
				return_value = _readCookedConstraint();
				break;

			case T_DropUserMappingStmt:
				return_value = _readDropUserMappingStmt();
				break;
			case T_AlterUserMappingStmt:
				return_value = _readAlterUserMappingStmt();
				break;
			case T_CreateUserMappingStmt:
				return_value = _readCreateUserMappingStmt();
				break;
			case T_AlterForeignServerStmt:
				return_value = _readAlterForeignServerStmt();
				break;
			case T_CreateForeignServerStmt:
				return_value = _readCreateForeignServerStmt();
				break;
			case T_AlterFdwStmt:
				return_value = _readAlterFdwStmt();
				break;
			case T_CreateFdwStmt:
				return_value = _readCreateFdwStmt();
				break;
			case T_ModifyTable:
				return_value = _readModifyTable();
				break;
			case T_LockRows:
				return_value = _readLockRows();
				break;
			case T_GpPolicy:
				return_value = _readGpPolicy();
				break;
			case T_DistributedBy:
				return_value = _readDistributedBy();
				break;
			case T_ImportForeignSchemaStmt:
				return_value = _readImportForeignSchemaStmt();
				break;
			case T_AlterTableMoveAllStmt:
				return_value = _readAlterTableMoveAllStmt();
				break;

			case T_CreatePublicationStmt:
				return_value = _readCreatePublicationStmt();
				break;
			case T_AlterPublicationStmt:
				return_value = _readAlterPublicationStmt();
				break;
			case T_CreateSubscriptionStmt:
				return_value = _readCreateSubscriptionStmt();
				break;
			case T_DropSubscriptionStmt:
				return_value = _readDropSubscriptionStmt();
				break;
			case T_AlterSubscriptionStmt:
				return_value = _readAlterSubscriptionStmt();
				break;

			case T_CreatePolicyStmt:
				return_value = _readCreatePolicyStmt();
				break;
			case T_AlterPolicyStmt:
				return_value = _readAlterPolicyStmt();
				break;
			case T_CreateTransformStmt:
				return_value = _readCreateTransformStmt();
				break;
			case T_CreateAmStmt:
				return_value = _readCreateAmStmt();
				break;
			case T_LockingClause:
				return_value = _readLockingClause();
				break;
			case T_AggExprId:
				return_value = _readAggExprId();
				break;
			case T_RowIdExpr:
				return_value = _readRowIdExpr();
				break;
			default:
				return_value = NULL; /* keep the compiler silent */
				elog(ERROR, "could not deserialize unrecognized node type: %d",
						 (int) nt);
				break;
	}

	return (Node *)return_value;
}

Node *
readNodeFromBinaryString(const char *str_arg, int len pg_attribute_unused())
{
	Node	   *node;
	int16		tg;

	read_str_ptr = str_arg;

	node = readNodeBinary();

	memcpy(&tg, read_str_ptr, sizeof(int16));
	if (tg != (int16)0xDEAD)
		elog(ERROR,"Deserialization lost sync.");

	return node;

}
/*
 * readDatumBinary
 *
 * Like readDatum() in readfuncs.c.
 */
static Datum
readDatumBinary(bool typbyval)
{
	Size		length;

	Datum		res;
	char	   *s;

	if (typbyval)
	{
		memcpy(&res, read_str_ptr, sizeof(Datum)); read_str_ptr+=sizeof(Datum);
	}
	else
	{
		memcpy(&length, read_str_ptr, sizeof(Size)); read_str_ptr+=sizeof(Size);
	  	if (length <= 0)
			res = 0;
		else
		{
			s = (char *) palloc(length+1);
			memcpy(s, read_str_ptr, length); read_str_ptr+=length;
			s[length]='\0';
			res = PointerGetDatum(s);
		}

	}

	return res;
}
