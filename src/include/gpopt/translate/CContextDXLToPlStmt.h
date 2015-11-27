//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CContextDXLToPlStmt.h
//
//	@doc:
//		Class providing access to CIdGenerators (needed to number initplans, motion
//		nodes as well as params), list of RangeTableEntires and Subplans
//		generated so far during DXL-->PlStmt translation.
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CContextDXLToPlStmt_H
#define GPDXL_CContextDXLToPlStmt_H

#include "gpopt/translate/CDXLTranslateContext.h"
#include "gpopt/translate/CDXLTranslateContextBaseTable.h"
#include "gpos/base.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/CIdGenerator.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"

#include "gpopt/gpdbwrappers.h"

// fwd decl
struct RangeTblEntry;
struct Plan;

struct List;
struct Var;
struct ShareInputScan;
struct GpPolicy;

namespace gpdxl
{

	// fwd decl
	class CDXLTranslateContext;

	typedef CHashMap<ULONG, CDXLTranslateContext, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
			CleanupDelete<ULONG>, CleanupDelete<CDXLTranslateContext> > HMUlDxltrctx;

	//---------------------------------------------------------------------------
	//	@class:
	//		CContextDXLToPlStmt
	//
	//	@doc:
	//		Class providing access to CIdGenerators (needed to number initplans, motion
	//		nodes as well as params), list of RangeTableEntires and Subplans
	//		generated so far during DXL-->PlStmt translation.
	//
	//---------------------------------------------------------------------------
	class CContextDXLToPlStmt
	{
		private:

			// cte consumer information
			struct SCTEConsumerInfo
			{
				// list of ShareInputScan represent cte consumers
				List *m_plSis;

				// ctor
				SCTEConsumerInfo
					(
					List *plPlanCTE
					)
					:
					m_plSis(plPlanCTE)
					{}

				void AddCTEPlan
					(
					ShareInputScan *psis
					)
				{
					GPOS_ASSERT(NULL != psis);
					m_plSis = gpdb::PlAppendElement(m_plSis, psis);
				}

				~SCTEConsumerInfo()
				{
					gpdb::FreeList(m_plSis);
				}

			};

			// hash maps mapping ULONG -> SCTEConsumerInfo
			typedef CHashMap<ULONG, SCTEConsumerInfo, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
			CleanupDelete<ULONG>, CleanupDelete<SCTEConsumerInfo> > HMUlCTEConsumerInfo;

			IMemoryPool *m_pmp;

			// mappings for share scan nodes
			HMUlDxltrctx *m_phmuldxltrctxSharedScan;

			// counter for generating plan ids
			CIdGenerator *m_pidgtorPlan;

			// counter for generating motion ids
			CIdGenerator *m_pidgtorMotion;

			// counter for generating unique param ids
			CIdGenerator *m_pidgtorParam;

			// list of all rtable entries
			List **m_pplRTable;

			// list of oids of partitioned tables
			List *m_plPartitionTables;

			// number of partition selectors for each dynamic scan
			DrgPul *m_pdrgpulNumSelectors;

			// list of all subplan entries
			List **m_pplSubPlan;

			// index of the target relation in the rtable or 0 if not a DML statement
			ULONG m_ulResultRelation;

			// hash map of the cte identifiers and the cte consumers with the same cte identifier
			HMUlCTEConsumerInfo *m_phmulcteconsumerinfo;

			// into clause
			IntoClause *m_pintocl;
			
			// CTAS distribution policy
			GpPolicy  *m_pdistrpolicy;
			
		public:
			// ctor/dtor
			CContextDXLToPlStmt
						(
						IMemoryPool *pmp,
						CIdGenerator *pidgtorPlan,
						CIdGenerator *pidgtorMotion,
						CIdGenerator *pidgtorParam,
						List **plRTable,
						List **plSubPlan
						)
						;

			// dtor
			~CContextDXLToPlStmt();

			const CDXLTranslateContext *PdxltrctxForSharedScan(ULONG ulSpoolId);

			void AddSharedScanTranslationContext(ULONG ulSpoolId, CDXLTranslateContext *pdxltrctx);

			// retrieve the next plan id
			ULONG UlNextPlanId();

			// retrieve the current motion id
			ULONG UlCurrentMotionId();

			// retrieve the next motion id
			ULONG UlNextMotionId();

			// retrieve the current parameter id
			ULONG UlCurrentParamId();

			// retrieve the next parameter id
			ULONG UlNextParamId();

			// add a newly found CTE consumer
			void AddCTEConsumerInfo(ULONG ulCTEId, ShareInputScan *psis);

			// return the list of shared input scan plans representing the CTE consumers
			List *PshscanCTEConsumer(ULONG ulCteId) const;

			// return list of range table entries
			List *PlPrte();

			// return list of partitioned table indexes
			List *PlPartitionedTables() const
			{
				return m_plPartitionTables;
			}

			// return list containing number of partition selectors for every scan id
			List *PlNumPartitionSelectors() const;

			List *PlPplanSubplan();

			// index of result relation in the rtable
			ULONG UlResultRelation() const
			{
				return m_ulResultRelation;
			}

			// add a range table entry
			void AddRTE(RangeTblEntry *prte, BOOL fResultRelation = false);

			// add a partitioned table index
			void AddPartitionedTable(OID oid);

			// increment the number of partition selectors for the given scan id
			void IncrementPartitionSelectors(ULONG ulScanId);

			void AddSubplan(Plan * );
				
			// add CTAS information
			void AddCtasInfo(IntoClause *pintocl, GpPolicy *pdistrpolicy);
			
			// into clause
			IntoClause *Pintocl() const
			{
				return m_pintocl;
			}

			// CTAS distribution policy
			GpPolicy *Pdistrpolicy() const
			{
				return m_pdistrpolicy;
			}

	};

	}
#endif // !GPDXL_CContextDXLToPlStmt_H

//EOF
