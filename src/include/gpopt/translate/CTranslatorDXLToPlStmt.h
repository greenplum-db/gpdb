//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CTranslatorDXLToPlStmt.h
//
//	@doc:
//		Class providing methods for translating from DXL tree to GPDB PlannedStmt
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorDxlToPlStmt_H
#define GPDXL_CTranslatorDxlToPlStmt_H

#include "postgres.h"
#include "gpopt/translate/CContextDXLToPlStmt.h"
#include "gpopt/translate/CDXLTranslateContext.h"
#include "gpopt/translate/CTranslatorDXLToScalar.h"
#include "gpopt/translate/CDXLTranslateContextBaseTable.h"
#include "gpopt/translate/CMappingColIdVarPlStmt.h"

#include "access/attnum.h"
#include "nodes/nodes.h"
#include "nodes/plannodes.h"

#include "gpos/base.h"

#include "naucrates/dxl/operators/dxlops.h"
#include "naucrates/dxl/CIdGenerator.h"
#include "naucrates/md/IMDRelationExternal.h"

// fwd declarations
namespace gpopt
{
	class CMDAccessor;
}

namespace gpmd
{
	class IMDRelation;
	class IMDIndex;
}

struct PlannedStmt;
struct Scan;
struct HashJoin;
struct NestLoop;
struct MergeJoin;
struct Hash;
struct RangeTblEntry;
struct Motion;
struct Limit;
struct Agg;
struct Append;
struct Sort;
struct SubqueryScan;
struct SubPlan;
struct Result;
struct Material;
struct ShareInputScan;
typedef Scan SeqScan;
typedef OpExpr DistinctExpr;
struct WindowFrame;
//struct Const;
//struct List;

namespace gpdxl
{

	using namespace gpopt;

	// fwd decl
	class CDXLNode;
	class CDXLPhysicalCTAS;
	class CDXLDirectDispatchInfo;

	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorDXLToPlStmt
	//
	//	@doc:
	//		Class providing methods for translating from DXL tree to GPDB PlannedStmt
	//
	//---------------------------------------------------------------------------
	class CTranslatorDXLToPlStmt
	{
		// shorthand for functions for translating DXL operator nodes into planner trees
		typedef Plan * (CTranslatorDXLToPlStmt::*PfPplan)(const CDXLNode *pdxln, CDXLTranslateContext *pdxltrctxOut, Plan *pplanParent, DrgPdxltrctx *pdrgpdxltrctxPrevSiblings);

		private:

			// pair of DXL operator type and the corresponding translator
			struct STranslatorMapping
			{
				// type
				Edxlopid edxlopid;

				// translator function pointer
				PfPplan pf;
			};

			// context for fixing index var attno
			struct SContextIndexVarAttno
			{
				// MD relation
				const IMDRelation *m_pmdrel;

				// MD index
				const IMDIndex *m_pmdindex;

				// ctor
				SContextIndexVarAttno
					(
					const IMDRelation *pmdrel,
					const IMDIndex *pmdindex
					)
					:
					m_pmdrel(pmdrel),
					m_pmdindex(pmdindex)
				{
					GPOS_ASSERT(NULL != pmdrel);
					GPOS_ASSERT(NULL != pmdindex);
				}
			}; // SContextIndexVarAttno

			// memory pool
			IMemoryPool *m_pmp;

			// meta data accessor
			CMDAccessor *m_pmda;

			// DXL operator translators indexed by the operator id
			PfPplan m_rgpfTranslators[EdxlopSentinel];

			CContextDXLToPlStmt *m_pctxdxltoplstmt;
			
			CTranslatorDXLToScalar *m_pdxlsctranslator;

			// command type
			CmdType m_cmdtype;
			
			// is target table distributed, false when in non DML statements
			BOOL m_fTargetTableDistributed;
			
			// list of result relations range table indexes for DML statements,
			// or NULL for select queries
			List *m_plResultRelations;
			
			// external scan counter
			ULONG m_ulExternalScanCounter;
			
			// number of segments
			ULONG m_ulSegments;

			// partition selector counter
			ULONG m_ulPartitionSelectorCounter;

			// private copy ctor
			CTranslatorDXLToPlStmt(const CTranslatorDXLToPlStmt&);

			// segment mapping for tables with LOCATION http:// or file://
			void MapLocationsFile
					(
					OID oidRel,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB
					);

			// segment mapping for tables with LOCATION gpfdist(s):// or custom protocol
			void MapLocationsFdist
					(
					OID oidRel,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB,
					Uri *pUri,
					const ULONG ulTotalPrimaries
					);

			// segment mapping for tables with EXECUTE 'cmd' ON.
			void MapLocationsExecute
					(
					OID oidRel,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB,
					const ULONG ulTotalPrimaries
					);

			// segment mapping for tables with EXECUTE 'cmd' on all segments
			void MapLocationsExecuteAllSegments
					(
					CHAR *szPrefixedCommand,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB
					);

			// segment mapping for tables with EXECUTE 'cmd' per host
			void MapLocationsExecutePerHost
					(
					CHAR *szPrefixedCommand,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB
					);

			// segment mapping for tables with EXECUTE 'cmd' on a given host
			void MapLocationsExecuteOneHost
					(
					CHAR *szHostName,
					CHAR *szPrefixedCommand,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB
					);

			//segment mapping for tables with EXECUTE 'cmd' on N random segments
			void MapLocationsExecuteRandomSegments
					(
					ULONG ulSegments,
					const ULONG ulTotalPrimaries,
					CHAR *szPrefixedCommand,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB
					);

			// segment mapping for tables with EXECUTE 'cmd' on a given segment
			void MapLocationsExecuteOneSegment
					(
					INT iTargetSegInd,
					CHAR *szPrefixedCommand,
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB
					);

			// segment mapping for tables with LOCATION gphdfs://
			void MapLocationsHdfs
					(
					char **rgszSegFileMap,
					CdbComponentDatabases *pcdbCompDB,
					CHAR *szFirstUri
					);

			// list of URIs for external scan
			List* PlExternalScanUriList(OID oidRel);

			// walker to set index var attno's
			static
			BOOL FSetIndexVarAttno(Node *pnode, SContextIndexVarAttno *pctxtidxvarattno);

		public:
			// ctor
			CTranslatorDXLToPlStmt(IMemoryPool *pmp, CMDAccessor *pmda, CContextDXLToPlStmt *pctxdxltoplstmt, ULONG ulSegments);

			// dtor
			~CTranslatorDXLToPlStmt();

			// translate DXL operator node into a Plan node
			Plan *PplFromDXL
				(
				const CDXLNode *pdxln,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// main translation routine for DXL tree -> PlannedStmt
			PlannedStmt *PplstmtFromDXL(const CDXLNode *pdxln, bool canSetTag);

			// translate the join types from its DXL representation to the GPDB one
			static JoinType JtFromEdxljt(EdxlJoinType edxljt);

		private:

			// initialize index of operator translators
			void InitTranslators();

			// Set the bitmapset of a plan to the list of param_ids defined by the plan
			void SetParamIds(Plan *);

			// Set the qDispSliceId in the subplans defining an initplan
			void SetInitPlanSliceInformation(SubPlan *);

			// Set InitPlanVariable in PlannedStmt
			void SetInitPlanVariables(PlannedStmt *);

			// Returns the id of the plan;
			INT IPlanId(Plan* pplparent);

			// translate DXL table scan node into a SeqScan node
			Plan *PtsFromDXLTblScan
				(
				const CDXLNode *pdxlnTblScan,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL index scan node into a IndexScan node
			Plan *PisFromDXLIndexScan
				(
				const CDXLNode *pdxlnIndexScan,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translates a DXL index scan node into a IndexScan node
			Plan *PisFromDXLIndexScan
				(
				const CDXLNode *pdxlnIndexScan,
				CDXLPhysicalIndexScan *pdxlopIndexScan,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				BOOL fIndexOnlyScan,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL hash join into a HashJoin node
			Plan *PhjFromDXLHJ
				(
				const CDXLNode *pdxlnHJ,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL nested loop join into a NestLoop node
			Plan *PnljFromDXLNLJ
				(
				const CDXLNode *pdxlnNLJ,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL merge join into a MergeJoin node
			Plan *PmjFromDXLMJ
				(
				const CDXLNode *pdxlnMJ,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL motion node into GPDB Motion plan node
			Plan *PplanMotionFromDXLMotion
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL motion node
			Plan *PplanTranslateDXLMotion
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL duplicate sensitive redistribute motion node into 
			// GPDB result node with hash filters
			Plan *PplanResultHashFilters
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL aggregate node into GPDB Agg plan node
			Plan *PaggFromDXLAgg
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL window node into GPDB window node
			Plan *PwindowFromDXLWindow
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate the DXL window frame into GPDB window frame node
			WindowFrame *Pwindowframe
				(
				const CDXLWindowFrame *pdxlwf,
				const CDXLTranslateContext *pdxltrctxChild,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplan
				);

			// translate DXL sort node into GPDB Sort plan node
			Plan *PsortFromDXLSort
				(
				const CDXLNode *pdxlnSort,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a DXL node into a Hash node
			Plan *PhhashFromDXL
				(
				const CDXLNode *pdxln,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL Limit node into a Limit node
			Plan *PlimitFromDXLLimit
				(
				const CDXLNode *pdxlnLimit,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL TVF into a GPDB Function Scan node
			Plan *PplanFunctionScanFromDXLTVF
				(
				const CDXLNode *pdxlnTVF,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *PsubqscanFromDXLSubqScan
				(
				const CDXLNode *pdxlnSubqScan,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *PresultFromDXLResult
				(
				const CDXLNode *pdxlnResult,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *PappendFromDXLAppend
				(
				const CDXLNode *pdxlnAppend,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *PmatFromDXLMaterialize
				(
				const CDXLNode *pdxlnMaterialize,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *PshscanFromDXLSharedScan
				(
				const CDXLNode *pdxlnSharedScan,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a sequence operator
			Plan *PplanSequence
				(
				const CDXLNode *pdxlnSequence,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a dynamic table scan operator
			Plan *PplanDTS
				(
				const CDXLNode *pdxlnDTS,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);	
			
			// translate a dynamic index scan operator
			Plan *PplanDIS
				(
				const CDXLNode *pdxlnDIS,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);
			
			// translate a DML operator
			Plan *PplanDML
				(
				const CDXLNode *pdxlnDML,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a Split operator
			Plan *PplanSplit
				(
				const CDXLNode *pdxlnSplit,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);
			
			// translate a row trigger operator
			Plan *PplanRowTrigger
				(
				const CDXLNode *pdxlnRowTrigger,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate an Assert operator
			Plan *PplanAssert
				(
				const CDXLNode *pdxlnAssert,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// Initialize spooling information
			void InitializeSpoolingInfo
				(
				Plan *pplan,
				ULONG ulShareId
				);

			// retrieve the flow of the shared input scan of the cte consumers
			Flow *PflowCTEConsumer(List *plshscanCTEConsumer);

			// translate a CTE producer into a GPDB share input scan
			Plan *PshscanFromDXLCTEProducer
				(
				const CDXLNode *pdxlnCTEProducer,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a CTE consumer into a GPDB share input scan
			Plan *PshscanFromDXLCTEConsumer
				(
				const CDXLNode *pdxlnCTEConsumer,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a (dynamic) bitmap table scan operator
			Plan *PplanBitmapTableScan
				(
				const CDXLNode *pdxlnBitmapScan,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a DXL PartitionSelector into a GPDB PartitionSelector
			Plan *PplanPartitionSelector
				(
				const CDXLNode *pdxlnPartitionSelector,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL filter list into GPDB filter list
			List *PlFilterList
				(
				const CDXLNode *pdxlnFilterList,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent
				);

			// create range table entry from a CDXLPhysicalTVF node
			RangeTblEntry *PrteFromDXLTVF
				(
				const CDXLNode *pdxlnTVF,
				CDXLTranslateContext *pdxltrctxOut,
				CDXLTranslateContextBaseTable *pdxltrctxbt,
				Plan *pplanParent
				);

			// create range table entry from a table descriptor
			RangeTblEntry *PrteFromTblDescr
				(
				const CDXLTableDescr *pdxltabdesc,
				const CDXLIndexDescr *pdxlid,
				Index iRel,
				CDXLTranslateContextBaseTable *pdxltrctxbtOut
				);

			// translate DXL projection list into a target list
			List *PlTargetListFromProjList
				(
				const CDXLNode *pdxlnPrL,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent
				);
			
			// insert NULL values for dropped attributes to construct the target list for a DML statement
			List *PlTargetListWithDroppedCols(List *plTargetList, const IMDRelation *pmdrel);

			// create a target list containing column references for a hash node from the
			// project list of its child node
			List *PlTargetListForHashNode
				(
				const CDXLNode *pdxlnPrL,
				CDXLTranslateContext *pdxltrctxChild,
				CDXLTranslateContext *pdxltrctxOut
				);
			
			List *PlQualFromFilter
				(
				const CDXLNode *pdxlnFilter,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent
				);


			// translate operator costs from the DXL cost structure into the types
			// used by GPDB
			void TranslatePlanCosts
				(
				const CDXLOperatorCost *pdxlopcost,
				Cost *pcostStartupOut,
				Cost *pcostTotalOut,
				Cost *pcostRowsOut,
				INT *piWidthOut
				);

			// shortcut for translating both the projection list and the filter
			void TranslateProjListAndFilter
				(
				const CDXLNode *pdxlnPrL,
				const CDXLNode *pdxlnFilter,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				List **pplTargetListOut,
				List **pplQualOut,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent
				);

			// translate the hash expr list of a redistribute motion node
			void TranslateHashExprList
				(
				const CDXLNode *pdxlnHashExprList,
				const CDXLTranslateContext *pdxltrctxChild,
				List **pplHashExprOut,
				List **pplHashExprTypesOut,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent
				);

			// translate the tree of bitmap index operators that are under a (dynamic) bitmap table scan
			Plan *PplanBitmapAccessPath
				(
				const CDXLNode *pdxlnBitmapAccessPath,
				CDXLTranslateContext *pdxltrctxOut,
				const IMDRelation *pmdrel,
				const CDXLTableDescr *pdxltabdesc,
				CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
				BitmapTableScan *pdbts
				);

			// translate a bitmap bool op expression
			Plan *PplanBitmapBoolOp
				(
				const CDXLNode *pdxlnBitmapBoolOp,
				CDXLTranslateContext *pdxltrctxOut,
				const IMDRelation *pmdrel,
				const CDXLTableDescr *pdxltabdesc,
				CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
				BitmapTableScan *pdbts
				);
			
			// translate CDXLScalarBitmapIndexProbe into BitmapIndexScan
			Plan *PplanBitmapIndexProbe
				(
				const CDXLNode *pdxlnBitmapIndexProbe,
				CDXLTranslateContext *pdxltrctxOut,
				const IMDRelation *pmdrel,
				const CDXLTableDescr *pdxltabdesc,
				CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
				BitmapTableScan *pdbts
				);

			void TranslateSortCols
				(
				const CDXLNode *pdxlnSortColList,
				const CDXLTranslateContext *pdxltrctxChild,
				AttrNumber *pattnoSortColIds,
				Oid *poidSortOpIds,
				bool *pboolNullsFirst
				);

			List *PlQualFromScalarCondNode
				(
				const CDXLNode *pdxlnFilter,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent
				);

			// parse string value into a Const
			static
			Cost CostFromStr(const CWStringBase *pstr);

			// check if the given operator is a DML operator on a distributed table
			BOOL FTargetTableDistributed(CDXLOperator *pdxlop);
			
			// add a target entry for the given colid to the given target list
			ULONG UlAddTargetEntryForColId
				(
				List **pplTargetList, 
				CDXLTranslateContext *pdxltrctx, 
				ULONG ulColId, 
				BOOL fResjunk
				);
			
			// translate the index condition list in an Index scan
			void TranslateIndexConditions
				(
				CDXLNode *pdxlnIndexCondList,
				const CDXLTableDescr *pdxltd,
				BOOL fIndexOnlyScan,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				CDXLTranslateContext *pdxltrctxOut,
				CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
				Plan *pplanParent,
				List **pplIndexConditions,
				List **pplIndexOrigConditions,
				List **pplIndexStratgey,
				List **pplIndexSubtype
				);
			
			// translate the index filters
			List *PlTranslateIndexFilter
				(
				CDXLNode *pdxlnFilter,
				CDXLTranslateContext *pdxltrctxOut,
				CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
				Plan *pplanParent
				);
			
			// translate the assert constraints
			List *PlTranslateAssertConstraints
				(
				CDXLNode *pdxlnFilter,
				CDXLTranslateContext *pdxltrctxOut,
				DrgPdxltrctx *pdrgpdxltrctx,
				Plan *pplanParent
				);

			// translate a CTAS operator
			Plan *PplanCTAS
				(
				const CDXLNode *pdxlnDML,
				CDXLTranslateContext *pdxltrctxOut,
				Plan *pplanParent,
				DrgPdxltrctx *pdrgpdxltrctxPrevSiblings = NULL // translation contexts of previous siblings
				);
			
			// sets the vartypmod fields in the target entries of the given target list
			static
			void SetVarTypMod(const CDXLPhysicalCTAS *pdxlop, List *plTargetList);

			// translate the into clause for a DXL physical CTAS operator
			IntoClause *PintoclFromCtas(const CDXLPhysicalCTAS *pdxlop);
			
			// translate the distribution policy for a DXL physical CTAS operator
			GpPolicy *PdistrpolicyFromCtas(const CDXLPhysicalCTAS *pdxlop);

			// translate CTAS storage options
			List *PlCtasOptions(CDXLCtasStorageOptions::DrgPctasOpt *pdrgpctasopt);
			
			// compute directed dispatch segment ids
			List *PlDirectDispatchSegIds(CDXLDirectDispatchInfo *pdxlddinfo);
			
			// hash a DXL datum with GPDB's hash function
			ULONG UlCdbHash(DrgPdxldatum *pdrgpdxldatum);

	};
}

#endif // !GPDXL_CTranslatorDxlToPlStmt_H

// EOF
