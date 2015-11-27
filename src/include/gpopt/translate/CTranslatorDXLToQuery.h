//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorDXLToQuery.h
//
//	@doc:
//		Class providing methods for translating from DXL tree to GPDB's Query.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorDXLToQuery_H
#define GPDXL_CTranslatorDXLToQuery_H
#define GPDXL_QUERY_LEVEL 0

#include "gpopt/translate/CMappingColIdVarQuery.h"
#include "gpopt/translate/CDXLTranslateContext.h"
#include "gpopt/translate/CTranslatorDXLToScalar.h"
#include "gpopt/translate/CDXLTranslateContextBaseTable.h"
#include "gpopt/translate/CStateDXLToQuery.h"

#include "gpos/base.h"

#include "naucrates/dxl/CIdGenerator.h"
#include "naucrates/dxl/operators/dxlops.h"

// fwd declarations
namespace gpopt
{
	class CMDAccessor;
}

struct JoinExpr;
struct Node;
struct Query;
struct RangeTblEntry;
struct RangeTblRef;
struct TargetEntry;
struct SortClause;

namespace gpdxl
{

	using namespace gpopt;

	// gpdb type
	typedef SortClause GroupClause;

	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorDXLToQuery
	//
	//	@doc:
	//		Class providing methods for translating from DXL tree to GPDB's Query.
	//
	//---------------------------------------------------------------------------
	class CTranslatorDXLToQuery
	{
		// shorthand for functions for translating DXL nodes to GPDB expressions
		typedef void (CTranslatorDXLToQuery::*PfPexpr)(const CDXLNode *pdxln, Query *pquery, CStateDXLToQuery *pstatedxltoquery, CMappingColIdVarQuery *pmapcidvarquery);

		private:

			// pair of DXL op id and translator function
			struct STranslatorElem
			{
				Edxlopid eopid;
				PfPexpr pf;
			};

			// memory pool
			IMemoryPool *m_pmp;

			// meta data accessor
			CMDAccessor *m_pmda;

			// counter for grouping and ordering columns
			ULONG m_ulSortgrouprefCounter;

			CTranslatorDXLToScalar *m_pdxlsctranslator;
			
			// number of segments
			ULONG m_ulSegments;

			// private copy ctor
			CTranslatorDXLToQuery(const CTranslatorDXLToQuery&);

			void SetSubqueryOutput
					(
					ULONG ulColId, // output column id
					Query *pquery, // the newly generated GPDB query object
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			void SetQueryOutput
					(
					const DrgPdxln *pdrgpdxlnQueryOutput, // list of output columns
					Query *pquery, // the newly generated GPDB query object
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate DXL operator node into a Query node
			void TranslateLogicalOp
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical TVF operator
			void TranslateTVF
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical get operator
			void TranslateGet
					(
					const CDXLNode *pdxl,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical select operator
			void TranslateSelect
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical group by operator
			void TranslateGroupBy
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical group by columns
			void TranslateGroupByColumns
					(
					const CDXLLogicalGroupBy *pdxlnlggrpby,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			void TranslateProject
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical limit by operator
			void TranslateLimit
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// translate a logical set operator
			void TranslateSetOp
					(
					const CDXLNode *pdxln,
					Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// resjunk unused target list entry of the set op child
			void MarkUnusedColumns
					(
					Query *pquery,
					RangeTblRef *prtref,
					CStateDXLToQuery *pstatedxltoquery,
					const DrgPul *pdrgpulColids
					);

			// translate a logical join operator
			void TranslateJoin
					(
					const CDXLNode *pdxl,
			 		Query *pquery,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery
					);

			// create range table entry from a table descriptor
			RangeTblEntry *PrteFromTblDescr
							(
			 				const CDXLTableDescr *,
							Index,
				 			CStateDXLToQuery *pstatedxltoquery,
							CMappingColIdVarQuery *pmapcidvarquery
							);

			// create range table reference for a logical get
			RangeTblRef *PrtrefFromDXLLgGet
							(
							const CDXLNode *pdxln,
							Query *pquery,
							CStateDXLToQuery *pstatedxltoquery,
							CMappingColIdVarQuery *pmapcidvarquery
							);

			// create range table reference for a logical TVF
			RangeTblRef *PrtrefFromDXLLgTVF
							(
							const CDXLNode *pdxlnFnGet,
							Query *pquery,
							CStateDXLToQuery *pstatedxltoquery,
							CMappingColIdVarQuery *pmapcidvarquery
							);

			// create a range table reference for CDXLNode representing a derived table
			RangeTblRef *PrtrefFromDXLLgOp
							(
							const CDXLNode *pdxln,
							Query *pquery,
				 			CStateDXLToQuery *pstatedxltoquery,
							CMappingColIdVarQuery *pmapcidvarquery
							);

			// create join expr for a logical join node
			JoinExpr *PjoinexprFromDXLLgJoin
						(
						const CDXLNode *pdxln,
						Query *pquery,
				 		CStateDXLToQuery *pstatedxltoquery,
						CMappingColIdVarQuery *pmapcidvarquery
						);

			// create a node from a child node of CDXLLogicalJoin
			Node *PnodeFromDXLLgJoinChild
				(
				const CDXLNode *pdxlnChild,
				Query *pquery,
				CStateDXLToQuery *pstatedxltoquery,
			 	CMappingColIdVarQuery *pmapcidvarquery
				);

			// translate the project list CDXLNode
			void TranslateProjList
					(
		 			const CDXLNode *pdxlnPrL,
					CStateDXLToQuery *pstatedxltoquery,
					CMappingColIdVarQuery *pmapcidvarquery,
					ULONG ulTargetEntryIndex
					);


		public:
			// ctor
			CTranslatorDXLToQuery(IMemoryPool *pmp, CMDAccessor *pmda, ULONG ulSegments);

			// dtor
			~CTranslatorDXLToQuery();

			// main translation routine for DXL tree -> Query
			Query *PqueryFromDXL
					(
					const CDXLNode *pdxln,
					const DrgPdxln *pdrgpdxlnQueryOutput, //  array of dxl nodes representing the list of output columns
					CStateDXLToQuery *pstatedxltoquery,
					TEMap *ptemap, // hash map storing the mapping of ColId->TE
					ULONG ulQueryLevel // the level of the query being translated
					);

			// main translation routine for DXL tree -> Query
			Query *PqueryFromDXLSubquery
					(
					const CDXLNode *pdxln,
					ULONG ulColId, // output column id
					CStateDXLToQuery *pstatedxltoquery,
					TEMap *ptemap, // hash map storing the mapping of ColId->Var
					ULONG ulQueryLevel // the level of the query being translated
					);
	};
}

#endif // !GPDXL_CTranslatorDXLToQuery_H

// EOF
