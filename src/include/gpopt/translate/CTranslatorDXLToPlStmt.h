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
		typedef Plan * (CTranslatorDXLToPlStmt::*PfPplan)(const CDXLNode *dxlnode, CDXLTranslateContext *output_context, DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings);

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
					const IMDIndex *md_index
					)
					:
					m_pmdrel(pmdrel),
					m_pmdindex(md_index)
				{
					GPOS_ASSERT(NULL != pmdrel);
					GPOS_ASSERT(NULL != index);
				}
			}; // SContextIndexVarAttno

			// memory pool
			IMemoryPool *m_memory_pool;

			// meta data accessor
			CMDAccessor *m_md_accessor;

			// DXL operator translators indexed by the operator id
			PfPplan m_rgpfTranslators[EdxlopSentinel];

			CContextDXLToPlStmt *m_dxl_to_plstmt_context;
			
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
			ULONG m_num_of_segments;

			// partition selector counter
			ULONG m_ulPartitionSelectorCounter;

			// private copy ctor
			CTranslatorDXLToPlStmt(const CTranslatorDXLToPlStmt&);

			// walker to set index var attno's
			static
			BOOL SetIndexVarAttnoWalker(Node *node, SContextIndexVarAttno *pctxtidxvarattno);

		public:
			// ctor
			CTranslatorDXLToPlStmt(IMemoryPool *memory_pool, CMDAccessor *md_accessor, CContextDXLToPlStmt *dxl_to_plstmt_context, ULONG ulSegments);

			// dtor
			~CTranslatorDXLToPlStmt();

			// translate DXL operator node into a Plan node
			Plan *PplFromDXL
				(
				const CDXLNode *dxlnode,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// main translation routine for DXL tree -> PlannedStmt
			PlannedStmt *GetPlannedStmtFromDXL(const CDXLNode *dxlnode, bool canSetTag);

			// translate the join types from its DXL representation to the GPDB one
			static JoinType GetGPDBJoinTypeFromDXLJoinType(EdxlJoinType join_type);

		private:

			// initialize index of operator translators
			void InitTranslators();

			// Set the bitmapset of a plan to the list of param_ids defined by the plan
			void SetParamIds(Plan *);

			// Set the qDispSliceId in the subplans defining an initplan
			void SetInitPlanSliceInformation(SubPlan *);

			// Set InitPlanVariable in PlannedStmt
			void SetInitPlanVariables(PlannedStmt *);

			// translate DXL table scan node into a SeqScan node
			Plan *TranslateDXLTblScan
				(
				const CDXLNode *pdxlnTblScan,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL index scan node into a IndexScan node
			Plan *TranslateDXLIndexScan
				(
				const CDXLNode *pdxlnIndexScan,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translates a DXL index scan node into a IndexScan node
			Plan *TranslateDXLIndexScan
				(
				const CDXLNode *pdxlnIndexScan,
				CDXLPhysicalIndexScan *pdxlopIndexScan,
				CDXLTranslateContext *output_context,
				BOOL fIndexOnlyScan,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL hash join into a HashJoin node
			Plan *PhjFromDXLHJ
				(
				const CDXLNode *TranslateDXLHashJoin,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL nested loop join into a NestLoop node
			Plan *TranslateDXLNLJoin
				(
				const CDXLNode *pdxlnNLJ,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL merge join into a MergeJoin node
			Plan *TranslateDXLMergeJoin
				(
				const CDXLNode *pdxlnMJ,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL motion node into GPDB Motion plan node
			Plan *TranslateDXLMotion
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL motion node
			Plan *PplanTranslateDXLMotion
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL duplicate sensitive redistribute motion node into 
			// GPDB result node with hash filters
			Plan *TranslateDXLRedistributeMotionToResultHashFilters
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL aggregate node into GPDB Agg plan node
			Plan *TranslateDXLAgg
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL window node into GPDB window node
			Plan *TranslateDXLWindow
				(
				const CDXLNode *pdxlnMotion,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL sort node into GPDB Sort plan node
			Plan *TranslateDXLSort
				(
				const CDXLNode *pdxlnSort,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a DXL node into a Hash node
			Plan *TranslateDXLHash
				(
				const CDXLNode *dxlnode,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL Limit node into a Limit node
			Plan *TranslateDXLLimit
				(
				const CDXLNode *pdxlnLimit,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate DXL TVF into a GPDB Function Scan node
			Plan *TranslateDXLTvf
				(
				const CDXLNode *pdxlnTVF,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *TranslateDXLSubQueryScan
				(
				const CDXLNode *pdxlnSubqScan,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *TranslateDXLResult
				(
				const CDXLNode *pdxlnResult,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *TranslateDXLAppend
				(
				const CDXLNode *pdxlnAppend,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *TranslateDXLMaterialize
				(
				const CDXLNode *pdxlnMaterialize,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			Plan *TranslateDXLSharedScan
				(
				const CDXLNode *pdxlnSharedScan,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a sequence operator
			Plan *TranslateDXLSequence
				(
				const CDXLNode *pdxlnSequence,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a dynamic table scan operator
			Plan *TranslateDXLDynTblScan
				(
				const CDXLNode *pdxlnDTS,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);	
			
			// translate a dynamic index scan operator
			Plan *TranslateDXLDynIdxScan
				(
				const CDXLNode *pdxlnDIS,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);
			
			// translate a DML operator
			Plan *TranslateDXLDml
				(
				const CDXLNode *pdxlnDML,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a Split operator
			Plan *TranslateDXLSplit
				(
				const CDXLNode *pdxlnSplit,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);
			
			// translate a row trigger operator
			Plan *TranslateDXLRowTrigger
				(
				const CDXLNode *pdxlnRowTrigger,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate an Assert operator
			Plan *TranslateDXLAssert
				(
				const CDXLNode *pdxlnAssert,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// Initialize spooling information
			void InitializeSpoolingInfo
				(
				Plan *pplan,
				ULONG ulShareId
				);

			// retrieve the flow of the shared input scan of the cte consumers
			Flow *GetFlowCTEConsumer(List *plshscanCTEConsumer);

			// translate a CTE producer into a GPDB share input scan
			Plan *TranslateDXLCTEProducerToSharedScan
				(
				const CDXLNode *pdxlnCTEProducer,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a CTE consumer into a GPDB share input scan
			Plan *TranslateDXLCTEConsumerToSharedScan
				(
				const CDXLNode *pdxlnCTEConsumer,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a (dynamic) bitmap table scan operator
			Plan *TranslateDXLBitmapTblScan
				(
				const CDXLNode *pdxlnBitmapScan,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a DXL PartitionSelector into a GPDB PartitionSelector
			Plan *TranslateDXLPartSelector
				(
				const CDXLNode *pdxlnPartitionSelector,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings // translation contexts of previous siblings
				);

			// translate a DXL Value Scan into GPDB Value Scan
			Plan *TranslateDXLValueScan
				(
				const CDXLNode *pdxlnValueScan,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings
				);

			// translate DXL filter list into GPDB filter list
			List *TranslateDXLFilterList
				(
				const CDXLNode *pdxlnFilterList,
				const CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *child_contexts,
				CDXLTranslateContext *output_context
				);

			// create range table entry from a CDXLPhysicalTVF node
			RangeTblEntry *TranslateDXLTvfToRangeTblEntry
				(
				const CDXLNode *pdxlnTVF,
				CDXLTranslateContext *output_context,
				CDXLTranslateContextBaseTable *base_table_context
				);

			// create range table entry from a CDXLPhysicalValueScan node
			RangeTblEntry *TranslateDXLValueScanToRangeTblEntry
				(
				const CDXLNode *pdxlnValueScan,
				CDXLTranslateContext *output_context,
				CDXLTranslateContextBaseTable *base_table_context
				);

			// create range table entry from a table descriptor
			RangeTblEntry *TranslateDXLTblDescrToRangeTblEntry
				(
				const CDXLTableDescr *table_descr,
				const CDXLIndexDescr *index_descr_dxl,
				Index iRel,
				CDXLTranslateContextBaseTable *pdxltrctxbtOut
				);

			// translate DXL projection list into a target list
			List *TranslateDXLProjList
				(
				const CDXLNode *project_list_dxl,
				const CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *child_contexts,
				CDXLTranslateContext *output_context
				);
			
			// insert NULL values for dropped attributes to construct the target list for a DML statement
			List *CreateTargetListWithNullsForDroppedCols(List *target_list, const IMDRelation *pmdrel);

			// create a target list containing column references for a hash node from the
			// project list of its child node
			List *TranslateDXLProjectListToHashTargetList
				(
				const CDXLNode *project_list_dxl,
				CDXLTranslateContext *pdxltrctxChild,
				CDXLTranslateContext *output_context
				);
			
			List *TranslateDXLFilterToQual
				(
				const CDXLNode *filter_dxlnode,
				const CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *child_contexts,
				CDXLTranslateContext *output_context
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
				const CDXLNode *project_list_dxl,
				const CDXLNode *filter_dxlnode,
				const CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *child_contexts,
				List **pplTargetListOut,
				List **pplQualOut,
				CDXLTranslateContext *output_context
				);

			// translate the hash expr list of a redistribute motion node
			void TranslateHashExprList
				(
				const CDXLNode *pdxlnHashExprList,
				const CDXLTranslateContext *pdxltrctxChild,
				List **pplHashExprOut,
				List **pplHashExprTypesOut,
				CDXLTranslateContext *output_context
				);

			// translate the tree of bitmap index operators that are under a (dynamic) bitmap table scan
			Plan *TranslateDXLBitmapAccessPath
				(
				const CDXLNode *pdxlnBitmapAccessPath,
				CDXLTranslateContext *output_context,
				const IMDRelation *pmdrel,
				const CDXLTableDescr *table_descr,
				CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings,
				BitmapTableScan *pdbts
				);

			// translate a bitmap bool op expression
			Plan *TranslateDXLBitmapBoolOp
				(
				const CDXLNode *pdxlnBitmapBoolOp,
				CDXLTranslateContext *output_context,
				const IMDRelation *pmdrel,
				const CDXLTableDescr *table_descr,
				CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings,
				BitmapTableScan *pdbts
				);
			
			// translate CDXLScalarBitmapIndexProbe into BitmapIndexScan or DynamicBitmapIndexScan
			Plan *TranslateDXLBitmapIndexProbe
				(
				const CDXLNode *pdxlnBitmapIndexProbe,
				CDXLTranslateContext *output_context,
				const IMDRelation *pmdrel,
				const CDXLTableDescr *table_descr,
				CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings,
				BitmapTableScan *pdbts
				);

			void TranslateSortCols
				(
				const CDXLNode *sort_col_list_dxl,
				const CDXLTranslateContext *pdxltrctxChild,
				AttrNumber *pattnoSortColIds,
				Oid *poidSortOpIds,
				Oid *poidSortCollations,
				bool *pboolNullsFirst
				);

			List *TranslateDXLScCondToQual
				(
				const CDXLNode *filter_dxlnode,
				const CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *child_contexts,
				CDXLTranslateContext *output_context
				);

			// parse string value into a Const
			static
			Cost CostFromStr(const CWStringBase *str);

			// check if the given operator is a DML operator on a distributed table
			BOOL IsTgtTblDistributed(CDXLOperator *dxlop);

			// add a target entry for the given colid to the given target list
			ULONG AddTargetEntryForColId
				(
				List **pplTargetList, 
				CDXLTranslateContext *pdxltrctx, 
				ULONG col_id, 
				BOOL fResjunk
				);
			
			// translate the index condition list in an Index scan
			void TranslateIndexConditions
				(
				CDXLNode *pdxlnIndexCondList,
				const CDXLTableDescr *pdxltd,
				BOOL fIndexOnlyScan,
				const IMDIndex *index,
				const IMDRelation *pmdrel,
				CDXLTranslateContext *output_context,
				CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings,
				List **pplIndexConditions,
				List **pplIndexOrigConditions,
				List **pplIndexStratgey,
				List **pplIndexSubtype
				);
			
			// translate the index filters
			List *TranslateDXLIndexFilter
				(
				CDXLNode *filter_dxlnode,
				CDXLTranslateContext *output_context,
				CDXLTranslateContextBaseTable *base_table_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings
				);
			
			// translate the assert constraints
			List *TranslateDXLAssertConstraints
				(
				CDXLNode *filter_dxlnode,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *child_contexts
				);

			// translate a CTAS operator
			Plan *TranslateDXLCtas
				(
				const CDXLNode *pdxlnDML,
				CDXLTranslateContext *output_context,
				DXLTranslationContextArr *pdrgpdxltrctxPrevSiblings = NULL // translation contexts of previous siblings
				);
			
			// sets the vartypmod fields in the target entries of the given target list
			static
			void SetVarTypMod(const CDXLPhysicalCTAS *dxlop, List *target_list);

			// translate the into clause for a DXL physical CTAS operator
			IntoClause *TranslateDXLPhyCtasToIntoClause(const CDXLPhysicalCTAS *dxlop);
			
			// translate the distribution policy for a DXL physical CTAS operator
			GpPolicy *TranslateDXLPhyCtasToDistrPolicy(const CDXLPhysicalCTAS *dxlop);

			// translate CTAS storage options
			List *TranslateDXLCtasStorageOptions(CDXLCtasStorageOptions::DXLCtasOptionArray *pdrgpctasopt);
			
			// compute directed dispatch segment ids
			List *TranslateDXLDirectDispatchInfo(CDXLDirectDispatchInfo *pdxlddinfo);
			
			// hash a DXL datum with GPDB's hash function
			ULONG GetDXLDatumGPDBHash(DXLDatumArray *pdrgpdxldatum);

	};
}

#endif // !GPDXL_CTranslatorDxlToPlStmt_H

// EOF
