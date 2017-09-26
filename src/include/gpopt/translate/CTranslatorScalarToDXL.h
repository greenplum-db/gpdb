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
		typedef CDXLNode * (CTranslatorScalarToDXL::*PfPdxln)(const Expr *pexpr, const CMappingVarColId* pmapvarcolid);

		// shorthand for functions for translating DXL nodes to GPDB expressions
		typedef CDXLDatum * (PfPdxldatumFromDatum)(IMemoryPool *pmp, const IMDType *pmdtype, BOOL fNull, ULONG ulLen, Datum datum);

		private:

			// pair of node tag and translator function
			struct STranslatorElem
			{
				NodeTag ent;
				PfPdxln pf;
			};

			// memory pool
			IMemoryPool *m_pmp;

			// meta data accessor
			CMDAccessor *m_pmda;

			// counter for generating unique column ids
			CIdGenerator *m_pidgtorCol;

			// counter for generating unique CTE ids
			CIdGenerator *m_pidgtorCTE;

			// absolute level of query whose vars will be translated
			ULONG m_ulQueryLevel;

			// does the currently translated scalar have distributed tables
			BOOL m_fHasDistributedTables;

			// is scalar being translated in query mode
			BOOL m_fQuery;

			// physical operator that created this translator
			EPlStmtPhysicalOpType m_eplsphoptype;

			// hash map that maintains the list of CTEs defined at a particular query level
			HMUlCTEListEntry *m_phmulCTEEntries;

			// list of CTE producers shared among the logical and scalar translators
			DrgPdxln *m_pdrgpdxlnCTE;

			EdxlBoolExprType EdxlbooltypeFromGPDBBoolType(BoolExprType) const;

			// translate list elements and add them as children of the DXL node
			void TranslateScalarChildren
				(
				CDXLNode *pdxln,
				List *plist,
				const CMappingVarColId* pmapvarcolid,
				BOOL *pfHasDistributedTables = NULL
				);

			// create a DXL scalar distinct comparison node from a GPDB DistinctExpr
			CDXLNode *PdxlnScDistCmpFromDistExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar boolean expression node from a GPDB qual list
			CDXLNode *PdxlnScCondFromQual
				(
				List *plQual,
				const CMappingVarColId* pmapvarcolid,
				BOOL *pfHasDistributedTables
				);

			// create a DXL scalar comparison node from a GPDB op expression
			CDXLNode *PdxlnScCmpFromOpExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar opexpr node from a GPDB expression
			CDXLNode *PdxlnScOpExprFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// translate an array expression
			CDXLNode *PdxlnArrayOpExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar array comparison node from a GPDB expression
			CDXLNode *PdxlnScArrayCompFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar Const node from a GPDB expression
			CDXLNode *PdxlnScConstFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL node for a scalar nullif from a GPDB Expr
			CDXLNode *PdxlnScNullIfFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar boolexpr node from a GPDB expression
			CDXLNode *PdxlnScBoolExprFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar boolean test node from a GPDB expression
			CDXLNode *PdxlnScBooleanTestFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar nulltest node from a GPDB expression
			CDXLNode *PdxlnScNullTestFromNullTest
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar case statement node from a GPDB expression
			CDXLNode *PdxlnScCaseStmtFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar if statement node from a GPDB case expression
			CDXLNode *PdxlnScIfStmtFromCaseExpr
				(
				const CaseExpr *pcaseexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar switch node from a GPDB case expression
			CDXLNode *PdxlnScSwitchFromCaseExpr
				(
				const CaseExpr *pcaseexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL node for a case test from a GPDB Expr.
			CDXLNode *PdxlnScCaseTestFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar coalesce node from a GPDB expression
			CDXLNode *PdxlnScCoalesceFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar minmax node from a GPDB expression
			CDXLNode *PdxlnScMinMaxFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar relabeltype node from a GPDB expression
			CDXLNode *PdxlnScCastFromRelabelType
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar coerce node from a GPDB expression
			CDXLNode *PdxlnScCoerceFromCoerce
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar coerceviaio node from a GPDB expression
			CDXLNode *PdxlnScCoerceFromCoerceViaIO
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);
		
			// create a DXL scalar array coerce expression node from a GPDB expression
			CDXLNode *PdxlnScArrayCoerceExprFromExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar funcexpr node from a GPDB expression
			CDXLNode *PdxlnScFuncExprFromFuncExpr
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar WindowFunc node from a GPDB expression
			CDXLNode *PdxlnScWindowFunc
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			// create a DXL scalar Aggref node from a GPDB expression
			CDXLNode *PdxlnScAggrefFromAggref
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnScIdFromVar
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnInitPlanFromParam(const Param *pparam) const;

			// create a DXL SubPlan node for a from a GPDB SubPlan
			CDXLNode *PdxlnSubPlanFromSubPlan
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnPlanFromParam
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnFromSublink
				(
				const Expr *pexpr,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnScSubqueryFromSublink
				(
				const SubLink *psublink,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnExistSubqueryFromSublink
				(
				const SubLink *psublink,
				const CMappingVarColId* pmapvarcolid
				);

			CDXLNode *PdxlnQuantifiedSubqueryFromSublink
				(
				const SubLink *psublink,
				const CMappingVarColId* pmapvarcolid
				);

			// translate an array expression
			CDXLNode *PdxlnArray(const Expr *pexpr, const CMappingVarColId* pmapvarcolid);

			// translate an arrayref expression
			CDXLNode *PdxlnArrayRef(const Expr *pexpr, const CMappingVarColId* pmapvarcolid);

			// add an indexlist to the given DXL arrayref node
			void AddArrayIndexList
				(
				CDXLNode *pdxln,
				List *plist,
				CDXLScalarArrayRefIndexList::EIndexListBound eilb,
				const CMappingVarColId* pmapvarcolid
				);

			// get the operator name
			const CWStringConst *PstrOpName(IMDId *pmdid) const;

			// translate the window frame edge, if the column used in the edge is a
			// computed column then add it to the project list
			CDXLNode *PdxlnWindowFrameEdgeVal
				(
				const Node *pnode,
				const CMappingVarColId* pmapvarcolid,
				CDXLNode *pdxlnNewChildScPrL,
				BOOL *pfHasDistributedTables
				);

		public:

			// ctor
			CTranslatorScalarToDXL
				(
				IMemoryPool *pmp,
				CMDAccessor *pmda,
				CIdGenerator *pulidgtorCol,
				CIdGenerator *pulidgtorCTE,
				ULONG ulQueryLevel,
				BOOL fQuery,
				HMUlCTEListEntry *phmulCTEEntries,
				DrgPdxln *pdrgpdxlnCTE
				);

			// set the caller type
			void SetCallingPhysicalOpType
					(
					EPlStmtPhysicalOpType eplsphoptype
					)
			{
				m_eplsphoptype = eplsphoptype;
			}

			// create a DXL datum from a GPDB const
			CDXLDatum *Pdxldatum(const Const *pconst) const;

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
				const CMappingVarColId* pmapvarcolid,
				BOOL *pfHasDistributedTables = NULL
				);

			// create a DXL scalar filter node from a GPDB qual list
			CDXLNode *PdxlnFilterFromQual
				(
				List *plQual,
				const CMappingVarColId* pmapvarcolid,
				Edxlopid edxlopFilterType,
				BOOL *pfHasDistributedTables = NULL
				);

			// create a DXL WindowFrame node from a GPDB expression
			CDXLWindowFrame *Pdxlwf
				(
				int frameOptions,
				const Node *startOffset,
				const Node *endOffset,
				const CMappingVarColId* pmapvarcolid,
				CDXLNode *pdxlnNewChildScPrL,
				BOOL *pfHasDistributedTables = NULL
				);

			// translate GPDB Const to CDXLDatum
			static
			CDXLDatum *Pdxldatum
				(
				IMemoryPool *pmp,
				CMDAccessor *mda,
				const Const *pconst
				);

			// translate GPDB datum to CDXLDatum
			static
			CDXLDatum *Pdxldatum
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			// translate GPDB datum to IDatum
			static
			IDatum *Pdatum
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				Datum datum
				);

			// extract the byte array value of the datum
			static
			BYTE *Pba
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			static
			CDouble DValue
				(
				IMDId *pmdid,
				BOOL fNull,
				BYTE *pba,
				Datum datum
				);

			// extract the long int value of a datum
			static
			LINT LValue
				(
				IMDId *pmdid,
				BOOL fNull,
				BYTE *pba,
				ULONG ulLen
				);

			// pair of DXL datum type and translator function
			struct SDXLDatumTranslatorElem
			{
				IMDType::ETypeInfo eti;
				PfPdxldatumFromDatum *pf;
			};

			// datum to oid CDXLDatum
			static
			CDXLDatum *PdxldatumOid
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			// datum to int2 CDXLDatum
			static
			CDXLDatum *PdxldatumInt2
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			// datum to int4 CDXLDatum
			static
			CDXLDatum *PdxldatumInt4
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			// datum to int8 CDXLDatum
			static
			CDXLDatum *PdxldatumInt8
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			// datum to bool CDXLDatum
			static
			CDXLDatum *PdxldatumBool
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);

			// datum to generic CDXLDatum
			static
			CDXLDatum *PdxldatumGeneric
				(
				IMemoryPool *pmp,
				const IMDType *pmdtype,
				BOOL fNull,
				ULONG ulLen,
				Datum datum
				);
	};
}
#endif // GPDXL_CTranslatorScalarToDXL_H

// EOF
