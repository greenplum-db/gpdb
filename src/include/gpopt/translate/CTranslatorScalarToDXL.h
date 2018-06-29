//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CTranslatorScalarToDXL.h
//
//	@doc:
//		Class providing methods for translating a GPDB Scalar Operation (in a Query / PlStmt object)
//		into a DXL tree.
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorScalarToDXL_H
#define GPDXL_CTranslatorScalarToDXL_H

#include "gpos/base.h"

#include "postgres.h"
#include "nodes/primnodes.h"

#include "gpopt/translate/CMappingVarColId.h"
#include "gpopt/translate/CCTEListEntry.h"

#include "naucrates/dxl/CIdGenerator.h"

#include "naucrates/base/IDatum.h"

#include "naucrates/md/IMDType.h"

// fwd declarations
namespace gpopt
{
	class CMDAccessor;
}

struct Aggref;
struct BoolExpr;
struct BooleanTest;
struct CaseExpr;
struct Expr;
struct FuncExpr;
struct NullTest;
struct OpExpr;
struct Param;
struct RelabelType;
struct ScalarArrayOpExpr;

namespace gpdxl
{
	using namespace gpopt;
	using namespace gpmd;

	// fwd decl
	class CIdGenerator;
	class CMappingVarColId;
	class CDXLDatum;

	class CTranslatorScalarToDXL
	{
		// shorthand for functions for translating GPDB expressions into DXL nodes
		typedef CDXLNode * (CTranslatorScalarToDXL::*ExprToDXLFn)(const Expr *expr, const CMappingVarColId* var_col_id_mapping);

		// shorthand for functions for translating DXL nodes to GPDB expressions
		typedef CDXLDatum * (DxlDatumFromDatum)(IMemoryPool *memory_pool, const IMDType *md_type, BOOL is_null, ULONG len, Datum datum);

		private:

			// pair of node tag and translator function
			struct STranslatorElem
			{
				NodeTag tag;
				ExprToDXLFn func_ptr;
			};

			// memory pool
			IMemoryPool *m_memory_pool;

			// meta data accessor
			CMDAccessor *m_md_accessor;

			// counter for generating unique column ids
			CIdGenerator *m_col_id_generator;

			// counter for generating unique CTE ids
			CIdGenerator *m_cte_id_generator;

			// absolute level of query whose vars will be translated
			ULONG m_query_level;

			// does the currently translated scalar have distributed tables
			BOOL m_has_distributed_tables;

			// is scalar being translated in query mode
			BOOL m_is_query_mode;

			// physical operator that created this translator
			EPlStmtPhysicalOpType m_op_type;

			// hash map that maintains the list of CTEs defined at a particular query level
			HMUlCTEListEntry *m_cte_entries;

			// list of CTE producers shared among the logical and scalar translators
			DXLNodeArray *m_cte_producers;

			EdxlBoolExprType EdxlbooltypeFromGPDBBoolType(BoolExprType) const;

			// translate list elements and add them as children of the DXL node
			void TranslateScalarChildren
				(
				CDXLNode *dxlnode,
				List *list,
				const CMappingVarColId* var_col_id_mapping,
				BOOL *has_distributed_tables = NULL
				);

			// create a DXL scalar distinct comparison node from a GPDB DistinctExpr
			CDXLNode *CreateScalarDistCmpFromDistExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar boolean expression node from a GPDB qual list
			CDXLNode *CreateScalarCondFromQual
				(
				List *quals,
				const CMappingVarColId* var_col_id_mapping,
				BOOL *has_distributed_tables
				);

			// create a DXL scalar comparison node from a GPDB op expression
			CDXLNode *CreateScalarCmpFromOpExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar opexpr node from a GPDB expression
			CDXLNode *CreateScalarOpExprFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// translate an array expression
			CDXLNode *TranslateArrayOpExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar array comparison node from a GPDB expression
			CDXLNode *CreateScalarArrayCompFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar Const node from a GPDB expression
			CDXLNode *CreateScalarConstFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL node for a scalar nullif from a GPDB Expr
			CDXLNode *CreateScalarNullIfFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar boolexpr node from a GPDB expression
			CDXLNode *CreateScalarBoolExprFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar boolean test node from a GPDB expression
			CDXLNode *CreateScalarBooleanTestFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar nulltest node from a GPDB expression
			CDXLNode *CreateScalarNullTestFromNullTest
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar case statement node from a GPDB expression
			CDXLNode *CreateScalarCaseStmtFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar if statement node from a GPDB case expression
			CDXLNode *CreateScalarIfStmtFromCaseExpr
				(
				const CaseExpr *case_expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar switch node from a GPDB case expression
			CDXLNode *CreateScalarSwitchFromCaseExpr
				(
				const CaseExpr *case_expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL node for a case test from a GPDB Expr.
			CDXLNode *CreateScalarCaseTestFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar coalesce node from a GPDB expression
			CDXLNode *CreateScalarCoalesceFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar minmax node from a GPDB expression
			CDXLNode *CreateScalarMinMaxFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar relabeltype node from a GPDB expression
			CDXLNode *CreateScalarCastFromRelabelType
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar coerce node from a GPDB expression
			CDXLNode *CreateScalarCoerceFromCoerce
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar coerceviaio node from a GPDB expression
			CDXLNode *CreateScalarCoerceFromCoerceViaIO
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);
		
			// create a DXL scalar array coerce expression node from a GPDB expression
			CDXLNode *CreateScalarArrayCoerceExprFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar funcexpr node from a GPDB expression
			CDXLNode *CreateScalarFuncExprFromFuncExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar WindowFunc node from a GPDB expression
			CDXLNode *CreateScalarWindowFunc
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar Aggref node from a GPDB expression
			CDXLNode *CreateScalarAggrefFromAggref
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreateScalarIdFromVar
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreateInitPlanFromParam(const Param *pparam) const;

			// create a DXL SubPlan node for a from a GPDB SubPlan
			CDXLNode *CreateSubPlanFromSubPlan
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreatePlanFromParam
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreateDXLNodeFromSublink
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreateScalarSubqueryFromSublink
				(
				const SubLink *sublink,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreateExistSubqueryFromSublink
				(
				const SubLink *sublink,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *CreateQuantifiedSubqueryFromSublink
				(
				const SubLink *sublink,
				const CMappingVarColId* var_col_id_mapping
				);

			// translate an array expression
			CDXLNode *TranslateArray(const Expr *expr, const CMappingVarColId* var_col_id_mapping);

			// translate an arrayref expression
			CDXLNode *TranslateArrayRef(const Expr *expr, const CMappingVarColId* var_col_id_mapping);

			// add an indexlist to the given DXL arrayref node
			void AddArrayIndexList
				(
				CDXLNode *dxlnode,
				List *list,
				CDXLScalarArrayRefIndexList::EIndexListBound index_list_bound,
				const CMappingVarColId* var_col_id_mapping
				);

			// get the operator name
			const CWStringConst *GetDXLArrayCmpType(IMDId *mdid) const;

			// translate the window frame edge, if the column used in the edge is a
			// computed column then add it to the project list
			CDXLNode *CreateDXLNodeFromWindowFrameEdgeVal
				(
				const Node *node,
				const CMappingVarColId* var_col_id_mapping,
				CDXLNode *new_scalar_proj_list,
				BOOL *has_distributed_tables
				);

		public:

			// ctor
			CTranslatorScalarToDXL
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CIdGenerator *col_id_generator,
				CIdGenerator *cte_id_generator,
				ULONG query_level,
				BOOL is_query_mode,
				HMUlCTEListEntry *cte_entries,
				DXLNodeArray *cte_dxlnode_array
				);

			// set the caller type
			void SetCallingPhysicalOpType
					(
					EPlStmtPhysicalOpType plstmt_physical_op_type
					)
			{
				m_op_type = plstmt_physical_op_type;
			}

			// create a DXL datum from a GPDB const
			CDXLDatum *GetDatumVal(const Const *constant) const;

			// return the current caller type
			EPlStmtPhysicalOpType GetPhysicalOpType() const
			{
				return m_op_type;
			}
			// create a DXL scalar operator node from a GPDB expression
			// and a table descriptor for looking up column descriptors
			CDXLNode *CreateScalarOpFromExpr
				(
				const Expr *expr,
				const CMappingVarColId* var_col_id_mapping,
				BOOL *has_distributed_tables = NULL
				);

			// create a DXL scalar filter node from a GPDB qual list
			CDXLNode *CreateFilterFromQual
				(
				List *quals,
				const CMappingVarColId* var_col_id_mapping,
				Edxlopid filter_type,
				BOOL *has_distributed_tables = NULL
				);

			// create a DXL WindowFrame node from a GPDB expression
			CDXLWindowFrame *GetWindowFrame
				(
				int frame_options,
				const Node *start_offset,
				const Node *end_offset,
				const CMappingVarColId* var_col_id_mapping,
				CDXLNode *new_scalar_proj_list,
				BOOL *has_distributed_tables = NULL
				);

			// translate GPDB Const to CDXLDatum
			static
			CDXLDatum *GetDatumVal
				(
				IMemoryPool *memory_pool,
				CMDAccessor *mda,
				const Const *constant
				);

			// translate GPDB datum to CDXLDatum
			static
			CDXLDatum *GetDatumVal
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				INT type_modifier,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			// translate GPDB datum to IDatum
			static
			IDatum *GetDatum
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				Datum datum
				);

			// extract the byte array value of the datum
			static
			BYTE *GetByteArray
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			static
			CDouble GetDoubleValue
				(
				IMDId *mdid,
				BOOL is_null,
				BYTE *bytes,
				Datum datum
				);

			// extract the long int value of a datum
			static
			LINT GetLintValue
				(
				IMDId *mdid,
				BOOL is_null,
				BYTE *bytes,
				ULONG len
				);

			// pair of DXL datum type and translator function
			struct SDXLDatumTranslatorElem
			{
				IMDType::ETypeInfo type_info;
				DxlDatumFromDatum *func_ptr;
			};

			// datum to oid CDXLDatum
			static
			CDXLDatum *CreateOidCDXLDatum
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			// datum to int2 CDXLDatum
			static
			CDXLDatum *PdxldatumInt2
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			// datum to int4 CDXLDatum
			static
			CDXLDatum *PdxldatumInt4
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			// datum to int8 CDXLDatum
			static
			CDXLDatum *PdxldatumInt8
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			// datum to bool CDXLDatum
			static
			CDXLDatum *CreateBoolCDXLDatum
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				BOOL is_null,
				ULONG len,
				Datum datum
				);

			// datum to generic CDXLDatum
			static
			CDXLDatum *CreateGenericCDXLDatum
				(
				IMemoryPool *memory_pool,
				const IMDType *md_type,
				INT type_modifier,
				BOOL is_null,
				ULONG len,
				Datum datum
				);
	};
}
#endif // GPDXL_CTranslatorScalarToDXL_H

// EOF
