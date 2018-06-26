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
		typedef CDXLNode * (CTranslatorScalarToDXL::*PfPdxln)(const Expr *pexpr, const CMappingVarColId* var_col_id_mapping);

		// shorthand for functions for translating DXL nodes to GPDB expressions
		typedef CDXLDatum * (PfPdxldatumFromDatum)(IMemoryPool *memory_pool, const IMDType *pmdtype, BOOL is_null, ULONG ulLen, Datum datum);

		private:

			// pair of node tag and translator function
			struct STranslatorElem
			{
				NodeTag ent;
				PfPdxln pf;
			};

			// memory pool
			IMemoryPool *m_memory_pool;

			// meta data accessor
			CMDAccessor *m_md_accessor;

			// counter for generating unique column ids
			CIdGenerator *m_pidgtorCol;

			// counter for generating unique CTE ids
			CIdGenerator *m_pidgtorCTE;

			// absolute level of query whose vars will be translated
			ULONG m_query_level;

			// does the currently translated scalar have distributed tables
			BOOL m_fHasDistributedTables;

			// is scalar being translated in query mode
			BOOL m_fQuery;

			// physical operator that created this translator
			EPlStmtPhysicalOpType m_eplsphoptype;

			// hash map that maintains the list of CTEs defined at a particular query level
			HMUlCTEListEntry *m_phmulCTEEntries;

			// list of CTE producers shared among the logical and scalar translators
			DXLNodeArray *m_pdrgpdxlnCTE;

			EdxlBoolExprType EdxlbooltypeFromGPDBBoolType(BoolExprType) const;

			// translate list elements and add them as children of the DXL node
			void TranslateScalarChildren
				(
				CDXLNode *dxlnode,
				List *plist,
				const CMappingVarColId* var_col_id_mapping,
				BOOL *pfHasDistributedTables = NULL
				);

			// create a DXL scalar distinct comparison node from a GPDB DistinctExpr
			CDXLNode *PdxlnScDistCmpFromDistExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar boolean expression node from a GPDB qual list
			CDXLNode *PdxlnScCondFromQual
				(
				List *plQual,
				const CMappingVarColId* var_col_id_mapping,
				BOOL *pfHasDistributedTables
				);

			// create a DXL scalar comparison node from a GPDB op expression
			CDXLNode *PdxlnScCmpFromOpExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar opexpr node from a GPDB expression
			CDXLNode *PdxlnScOpExprFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// translate an array expression
			CDXLNode *PdxlnArrayOpExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar array comparison node from a GPDB expression
			CDXLNode *PdxlnScArrayCompFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar Const node from a GPDB expression
			CDXLNode *PdxlnScConstFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL node for a scalar nullif from a GPDB Expr
			CDXLNode *PdxlnScNullIfFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar boolexpr node from a GPDB expression
			CDXLNode *PdxlnScBoolExprFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar boolean test node from a GPDB expression
			CDXLNode *PdxlnScBooleanTestFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar nulltest node from a GPDB expression
			CDXLNode *PdxlnScNullTestFromNullTest
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar case statement node from a GPDB expression
			CDXLNode *PdxlnScCaseStmtFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar if statement node from a GPDB case expression
			CDXLNode *PdxlnScIfStmtFromCaseExpr
				(
				const CaseExpr *pcaseexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar switch node from a GPDB case expression
			CDXLNode *PdxlnScSwitchFromCaseExpr
				(
				const CaseExpr *pcaseexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL node for a case test from a GPDB Expr.
			CDXLNode *PdxlnScCaseTestFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar coalesce node from a GPDB expression
			CDXLNode *PdxlnScCoalesceFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar minmax node from a GPDB expression
			CDXLNode *PdxlnScMinMaxFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar relabeltype node from a GPDB expression
			CDXLNode *PdxlnScCastFromRelabelType
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar coerce node from a GPDB expression
			CDXLNode *PdxlnScCoerceFromCoerce
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar coerceviaio node from a GPDB expression
			CDXLNode *PdxlnScCoerceFromCoerceViaIO
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);
		
			// create a DXL scalar array coerce expression node from a GPDB expression
			CDXLNode *PdxlnScArrayCoerceExprFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar funcexpr node from a GPDB expression
			CDXLNode *PdxlnScFuncExprFromFuncExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar WindowFunc node from a GPDB expression
			CDXLNode *PdxlnScWindowFunc
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			// create a DXL scalar Aggref node from a GPDB expression
			CDXLNode *PdxlnScAggrefFromAggref
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnScIdFromVar
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnInitPlanFromParam(const Param *pparam) const;

			// create a DXL SubPlan node for a from a GPDB SubPlan
			CDXLNode *PdxlnSubPlanFromSubPlan
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnPlanFromParam
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnFromSublink
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnScSubqueryFromSublink
				(
				const SubLink *psublink,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnExistSubqueryFromSublink
				(
				const SubLink *psublink,
				const CMappingVarColId* var_col_id_mapping
				);

			CDXLNode *PdxlnQuantifiedSubqueryFromSublink
				(
				const SubLink *psublink,
				const CMappingVarColId* var_col_id_mapping
				);

			// translate an array expression
			CDXLNode *PdxlnArray(const Expr *pexpr, const CMappingVarColId* var_col_id_mapping);

			// translate an arrayref expression
			CDXLNode *PdxlnArrayRef(const Expr *pexpr, const CMappingVarColId* var_col_id_mapping);

			// add an indexlist to the given DXL arrayref node
			void AddArrayIndexList
				(
				CDXLNode *dxlnode,
				List *plist,
				CDXLScalarArrayRefIndexList::EIndexListBound eilb,
				const CMappingVarColId* var_col_id_mapping
				);

			// get the operator name
			const CWStringConst *GetDXLArrayCmpType(IMDId *pmdid) const;

			// translate the window frame edge, if the column used in the edge is a
			// computed column then add it to the project list
			CDXLNode *PdxlnWindowFrameEdgeVal
				(
				const Node *node,
				const CMappingVarColId* var_col_id_mapping,
				CDXLNode *pdxlnNewChildScPrL,
				BOOL *pfHasDistributedTables
				);

		public:

			// ctor
			CTranslatorScalarToDXL
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CIdGenerator *pulidgtorCol,
				CIdGenerator *pulidgtorCTE,
				ULONG query_level,
				BOOL fQuery,
				HMUlCTEListEntry *phmulCTEEntries,
				DXLNodeArray *cte_dxlnode_array
				);

			// set the caller type
			void SetCallingPhysicalOpType
					(
					EPlStmtPhysicalOpType plstmt_physical_op_type
					)
			{
				m_eplsphoptype = plstmt_physical_op_type;
			}

			// create a DXL datum from a GPDB const
			CDXLDatum *GetDatumVal(const Const *pconst) const;

			// return the current caller type
			EPlStmtPhysicalOpType Eplsphoptype() const
			{
				return m_eplsphoptype;
			}
			// create a DXL scalar operator node from a GPDB expression
			// and a table descriptor for looking up column descriptors
			CDXLNode *PdxlnScOpFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* var_col_id_mapping,
				BOOL *pfHasDistributedTables = NULL
				);

			// create a DXL scalar filter node from a GPDB qual list
			CDXLNode *PdxlnFilterFromQual
				(
				List *plQual,
				const CMappingVarColId* var_col_id_mapping,
				Edxlopid edxlopFilterType,
				BOOL *pfHasDistributedTables = NULL
				);

			// create a DXL WindowFrame node from a GPDB expression
			CDXLWindowFrame *GetWindowFrame
				(
				int frameOptions,
				const Node *startOffset,
				const Node *endOffset,
				const CMappingVarColId* var_col_id_mapping,
				CDXLNode *pdxlnNewChildScPrL,
				BOOL *pfHasDistributedTables = NULL
				);

			// translate GPDB Const to CDXLDatum
			static
			CDXLDatum *GetDatumVal
				(
				IMemoryPool *memory_pool,
				CMDAccessor *mda,
				const Const *pconst
				);

			// translate GPDB datum to CDXLDatum
			static
			CDXLDatum *GetDatumVal
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				INT type_modifier,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			// translate GPDB datum to IDatum
			static
			IDatum *Pdatum
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				Datum datum
				);

			// extract the byte array value of the datum
			static
			BYTE *GetByteArray
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			static
			CDouble DValue
				(
				IMDId *pmdid,
				BOOL is_null,
				BYTE *pba,
				Datum datum
				);

			// extract the long int value of a datum
			static
			LINT Value
				(
				IMDId *pmdid,
				BOOL is_null,
				BYTE *pba,
				ULONG ulLen
				);

			// pair of DXL datum type and translator function
			struct SDXLDatumTranslatorElem
			{
				IMDType::ETypeInfo type_info;
				PfPdxldatumFromDatum *pf;
			};

			// datum to oid CDXLDatum
			static
			CDXLDatum *PdxldatumOid
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			// datum to int2 CDXLDatum
			static
			CDXLDatum *PdxldatumInt2
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			// datum to int4 CDXLDatum
			static
			CDXLDatum *PdxldatumInt4
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			// datum to int8 CDXLDatum
			static
			CDXLDatum *PdxldatumInt8
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			// datum to bool CDXLDatum
			static
			CDXLDatum *PdxldatumBool
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);

			// datum to generic CDXLDatum
			static
			CDXLDatum *PdxldatumGeneric
				(
				IMemoryPool *memory_pool,
				const IMDType *pmdtype,
				INT type_modifier,
				BOOL is_null,
				ULONG ulLen,
				Datum datum
				);
	};
}
#endif // GPDXL_CTranslatorScalarToDXL_H

// EOF
