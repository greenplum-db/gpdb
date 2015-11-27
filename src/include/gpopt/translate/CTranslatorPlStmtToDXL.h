//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CTranslatorPlStmtToDXL.h
//
//	@doc:
//		Class providing methods for translating GPDB's PlannedStmt into DXL Tree
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorPlStmtToDXL_H
#define GPDXL_CTranslatorPlStmtToDXL_H

#include "gpopt/translate/CTranslatorScalarToDXL.h"
#include "gpopt/translate/CMappingParamIdScalarId.h"

#include "gpos/base.h"

#include "naucrates/dxl/operators/dxlops.h"
#include "naucrates/dxl/CIdGenerator.h"

#include "naucrates/md/IMDIndex.h"


// fwd declarations
namespace gpopt
{
	class CMDAccessor;
}

struct Const;
struct List;
struct PlannedStmt;
struct Scan;
struct HashJoin;
struct Hash;
struct RangeTblEntry;
struct NestLoop;
struct MergeJoin;
struct Motion;
struct Limit;
struct Agg;
struct Append;
struct Sort;
struct SubqueryScan;
struct Result;
struct Unique;
struct Material;
struct ShareInputScan;
struct IndexScan;

typedef OpExpr DistintExpr;
typedef Scan SeqScan;

namespace gpdxl
{
	using namespace gpopt;

	class CDXLNode;
	class CMappingVarColId;

	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorPlStmtToDXL
	//
	//	@doc:
	//		Class providing methods for translating GPDB's PlannedStmt into DXL Tree.
	//
	//---------------------------------------------------------------------------
	class CTranslatorPlStmtToDXL
	{
		// shorthand for functions for translating GPDB expressions into DXL nodes
		typedef CDXLNode * (CTranslatorPlStmtToDXL::*PfPdxln)(const Plan *pplan);

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
			CIdGenerator *m_pidgtor;

			// planned stmt being translated
			PlannedStmt *m_pplstmt;

			// translator for scalar expressions
			CTranslatorScalarToDXL *m_psctranslator;

			// project lists of already translated shared scans indexed by the shared scan id
			HMUlPdxln *m_phmuldxlnSharedScanProjLists;

			// mapping from param id -> scalar id in subplans
			CMappingParamIdScalarId *m_pparammapping;

			// private copy ctor
			CTranslatorPlStmtToDXL(const CTranslatorPlStmtToDXL&);

			// create DXL table scan node from a scan plan node
			CDXLNode *PdxlnTblScanFromPlan(const Plan *pplan);

			// create DXL physical TVF node from a FunctionScan plan node
			CDXLNode *PdxlnFunctionScanFromPlan(const Plan *pplan);

			// create DXL physical result node from a folded function expression
			CDXLNode *PdxlnResultFromFoldedFuncExpr
				(
				const Expr *pexpr,
				CWStringDynamic *pstrAlias,
				CDXLPhysicalProperties *pdxlprop,
				CMappingVarColId *pmapvarcolid
				);

			// create DXL index scan node from an index plan node
			CDXLNode *PdxlnIndexScanFromPlan(const Plan *pplan);

			// create a DXL index (only) scan node from a GPDB index (only) scan node.
			CDXLNode *PdxlnIndexScanFromGPDBIndexScan(const IndexScan *pindexscan, BOOL fIndexOnlyScan);

			// create DXL hash join node from a hash join plan node
			CDXLNode *PdxlnHashjoinFromPlan(const Plan *pplan);

			// create DXL hash node from a hash plan node
			CDXLNode *PdxlnHashFromPlan(const Plan *pplan);

			// create DXL nested loop join node from a nested loop join plan node
			CDXLNode *PdxlnNLJoinFromPlan(const Plan *pplan);

			// create DXL merge join node from a merge join plan node
			CDXLNode *PdxlnMergeJoinFromPlan(const Plan *pplan);

			// create DXL result node from a result plan node
			CDXLNode *PdxlnResultFromPlan(const Plan *pplan);

			// create DXL Limit node from a Limit plan node
			CDXLNode *PdxlnLimitFromPlan(const Plan *pplan);

			// create DXL redistribute motion node from a motion plan node
			CDXLNode *PdxlnMotionFromPlan(const Plan *pplan);

			// create DXL aggregate node from an Agg plan node
			CDXLNode *PdxlnAggFromPlan(const Plan *pplan);

			// create DXL window node from an window plan node
			CDXLNode *PdxlnWindowFromPlan(const Plan *pplan);

            // create DXL aggregate node from a Unique plan node
            CDXLNode *PdxlnUniqueFromPlan(const Plan *pplan);

			// create DXL sort node from a Sort plan node
			CDXLNode *PdxlnSortFromPlan(const Plan *pplan);

			CDXLNode *PdxlnSubqueryScanFromPlan(const Plan *pplan);

			// create DXL append node from an Append plan node
			CDXLNode *PdxlnAppendFromPlan(const Plan *pplan);

			// create DXL shared scan node from a ShareInputScan plan node
			CDXLNode *PdxlnSharedScanFromPlan(const Plan *pplan);

			// create DXL materialize node from a Material plan node
			CDXLNode *PdxlnMaterializeFromPlan(const Plan *pplan);

			// create DXL sequence node from a Sequence plan node
			CDXLNode *PdxlnSequence(const Plan *pplan);

			// create DXL dynamic table scan node from the corresponding plan node
			CDXLNode *PdxlnDynamicTableScan(const Plan *pplan);

			// create a DXL table descriptor from a range table entry
			CDXLTableDescr *Pdxltabdesc(const RangeTblEntry *prte, const Scan *psscan);

			// create a DXL projection list of a scan node from GPDB target list
			CDXLNode *PdxlnPrLFromTL
				(
				List *plTargetList,
				CMappingVarColId *pmapvarcolid
				);

			// extract cost estimates from Plan structures and store them in a
			// DXL properties container
			CDXLPhysicalProperties *PdxlpropFromPlan(const Plan *pplan);

			// translate the hash expression list for a redistribute motion node into a DXL node
			CDXLNode *PdxlnHashExprLFromList(List *, List *, const CDXLNode *);

			// translate the sorting column list for a sort or motion node into a DXL node
			CDXLNode *PdxlnSortingColListFromPlan(AttrNumber *patnoSortColIds, OID *poidSortOpIds, ULONG ulNumCols, const CDXLNode *pdxlnPrL);

			// translate target and qual lists into DXL project list and filter,
			// respectively
			void TranslateTListAndQual
				(
				List *plTargetList,
				List *plQual,
				ULONG ulRTEIndex,
				const CDXLTableDescr *pdxltabdesc,
				const IMDIndex *pmdindex,
				const CDXLNode *pdxlnPrLLeft,
				const CDXLNode *pdxlnPrLRight,
				CDXLNode **ppdxlnPrLOut,
				CDXLNode **ppdxlnFilterOut
				);

			// populate the input and output segments id lists from a Motion node
			void TranslateMotionSegmentInfo
				(
				const Motion *pmotion,
				DrgPi *pdrgpiInputSegIds,
				DrgPi *pdrgpiOutputSegIds
				);

			// creates a DXL shared scan spool info structure from a GPDB shared scan node
			CDXLSpoolInfo *PspoolinfoFromSharedScan(const ShareInputScan *pshscan);

		public:
			// ctor/dtor
			CTranslatorPlStmtToDXL
				(
				IMemoryPool *pmp,
				CMDAccessor *pmda,
				CIdGenerator *pulIdGenerator,
				PlannedStmt *pplstmt,
				CMappingParamIdScalarId *pmapps
				);

			~CTranslatorPlStmtToDXL();

			// main translation routine for PlannedStmt -> DXL tree
			CDXLNode *PdxlnFromPlstmt();

			// translates a PlannedStmt node into a DXL tree
			CDXLNode *PdxlnFromPlan(const Plan *pplan);

	};
}

#endif // !GPDXL_CTranslatorPlStmtToDXL_H

// EOF
