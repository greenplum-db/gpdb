//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CTranslatorQueryToDXL.h
//
//	@doc:
//		Class providing methods for translating a GPDB Query object into a
//		DXL Tree
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorQueryToDXL_H
#define GPDXL_CTranslatorQueryToDXL_H

#define GPDXL_CTE_ID_START 1
#define GPDXL_COL_ID_START 1

#include "gpopt/translate/CMappingVarColId.h"
#include "gpopt/translate/CTranslatorScalarToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"

// fwd declarations
namespace gpopt
{
	class CMDAccessor;
}

struct Query;
struct RangeTblEntry;
struct Const;
struct List;
struct CommonTableExpr;

namespace gpdxl
{
	using namespace gpos;
	using namespace gpopt;

	typedef CHashMap<ULONG, BOOL, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
			CleanupDelete<ULONG>, CleanupDelete<BOOL> > HMUlF;

	typedef CHashMapIter<INT, ULONG, gpos::UlHash<INT>, gpos::FEqual<INT>,
			CleanupDelete<INT>, CleanupDelete<ULONG> > HMIUlIter;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorQueryToDXL
	//
	//	@doc:
	//		Class providing methods for translating a GPDB Query object into a
	//      DXL Tree.
	//
	//---------------------------------------------------------------------------
	class CTranslatorQueryToDXL
	{
		// shorthand for functions for translating DXL nodes to GPDB expressions
		typedef CDXLNode * (CTranslatorQueryToDXL::*PfPdxlnLogical)(const RangeTblEntry *prte, ULONG ulRTIndex, ULONG ulCurrQueryLevel);

		// mapping RTEKind to WCHARs
		struct SRTENameElem
		{
			RTEKind m_rtekind;
			const WCHAR *m_wsz;
		};

		// pair of RTEKind and its translators
		struct SRTETranslator
		{
			RTEKind m_rtekind;
			PfPdxlnLogical pf;
		};

		// mapping CmdType to WCHARs
		struct SCmdNameElem
		{
			CmdType m_cmdtype;
			const WCHAR *m_wsz;
		};

		// pair of unsupported node tag and feature name
		struct SUnsupportedFeature
		{
			NodeTag ent;
			const WCHAR *m_wsz;
		};
		
		private:
			// memory pool
			IMemoryPool *m_pmp;

			// source system id
			CSystemId m_sysid;

			// meta data accessor
			CMDAccessor *m_pmda;

			// counter for generating unique column ids
			CIdGenerator *m_pidgtorCol;

			// counter for generating unique CTE ids
			CIdGenerator *m_pidgtorCTE;

			// scalar translator used to convert scalar operation into DXL.
			CTranslatorScalarToDXL *m_psctranslator;

			// holds the var to col id information mapping
			CMappingVarColId *m_pmapvarcolid;

			// query being translated
			Query *m_pquery;

			// absolute level of query being translated
			ULONG m_ulQueryLevel;

			// does the query have distributed tables
			BOOL m_fHasDistributedTables;

			// top query is a DML
			BOOL m_fTopDMLQuery;

			// this is a CTAS query
			BOOL m_fCTASQuery;

			// hash map that maintains the list of CTEs defined at a particular query level
			HMUlCTEListEntry *m_phmulCTEEntries;

			// query output columns
			DrgPdxln *m_pdrgpdxlnQueryOutput;
			
			// list of CTE producers
			DrgPdxln *m_pdrgpdxlnCTE;
			
			// CTE producer IDs defined at the current query level
			HMUlF *m_phmulfCTEProducers;

			//ctor
			// private constructor, called from the public factory function PtrquerytodxlInstance
			CTranslatorQueryToDXL
				(
				IMemoryPool *pmp,
				CMDAccessor *pmda,
				CIdGenerator *pidgtorColId,
				CIdGenerator *pidgtorCTE,
				CMappingVarColId *pmapvarcolid,
				Query *pquery,
				ULONG ulQueryLevel,
				BOOL fTopDMLQuery,
				HMUlCTEListEntry *phmulCTEEntries  // hash map between query level -> list of CTEs defined at that level
				);

			// private copy ctor
			CTranslatorQueryToDXL(const CTranslatorQueryToDXL&);

			// check for unsupported node types, throws an exception if an unsupported
			// node is found
			void CheckUnsupportedNodeTypes(Query *pquery);

			// check for SIRV functions in the targetlist without a FROM clause and
			// throw an exception when found
			void CheckSirvFuncsWithoutFromClause(Query *pquery);

			// check for SIRV functions in the tree rooted at the given node
			BOOL FHasSirvFunctions (Node *pnode) const;

			// translate FromExpr (in the GPDB query) into a CDXLLogicalJoin or CDXLLogicalGet
			CDXLNode *PdxlnFromGPDBFromExpr(FromExpr *pfromexpr);

			// translate set operations
			CDXLNode *PdxlnFromSetOp(Node *pnodeSetOp, List *plTargetList, HMIUl *phmiulOutputCols);

			// create the set operation given its children, input and output columns
			CDXLNode *PdxlnSetOp
				(
				EdxlSetOpType edxlsetop,
				List *plTargetListOutput,
				DrgPul *pdrgpulOutput,
				DrgPdrgPul *pdrgpdrgulInputColIds,
				DrgPdxln *pdrgpdxlnChildren,
				BOOL fCastAcrossInput,
				BOOL fKeepResjunked
				)
				const;

			// check if the set operation need to cast any of its input columns
			BOOL FCast(List *plTargetList, DrgPmdid *pdrgpmdid) const;
			// translate a window operator
			CDXLNode *PdxlnWindow
				(
				CDXLNode *pdxlnChild,
				List *plTargetList,
				List *plWindowClause,
				List *plSortClause,
				HMIUl *phmiulSortColsColId,
				HMIUl *phmiulOutputCols
				);

			// translate window spec
			DrgPdxlws *Pdrgpdxlws(List *plWindowClause, HMIUl *phmiulSortColsColId, CDXLNode *pdxlnScPrL);

			// update window spec positions of LEAD/LAG functions
			void UpdateLeadLagWinSpecPos(CDXLNode *pdxlnPrL, DrgPdxlws *pdrgpdxlwinspec) const;

			// manufucture window frame for lead/lag functions
			CDXLWindowFrame *PdxlwfLeadLag(BOOL fLead, CDXLNode *pdxlnOffset) const;

			// translate the child of a set operation
			CDXLNode *PdxlnSetOpChild(Node *pnodeChild, DrgPul *pdrgpul, DrgPmdid *pdrgpmdid, List *plTargetList);

			// return a dummy const table get
			CDXLNode *PdxlnConstTableGet() const;

			// translate an Expr into CDXLNode
			CDXLNode *PdxlnScFromGPDBExpr(Expr *pexpr);

			// translate the JoinExpr (inside FromExpr) into a CDXLLogicalJoin node
			CDXLNode *PdxlnLgJoinFromGPDBJoinExpr(JoinExpr *pjoinexpr);

			// construct a group by node for a set of grouping columns
			CDXLNode *PdxlnSimpleGroupBy
				(
				List *plTargetList,
				List *plGroupClause,
				CBitSet *pbs,
				BOOL fHasAggs,
				BOOL fGroupingSets,				// is this GB part of a GS query
				CDXLNode *pdxlnChild,
				HMIUl *phmiulSortGrpColsColId,  // mapping sortgroupref -> ColId
				HMIUl *phmiulChild,				// mapping attno->colid in child node
				HMIUl *phmiulOutputCols			// mapping attno -> ColId for output columns
				);

			// check if the argument of a DQA has already being used by another DQA
			static
			BOOL FDuplicateDqaArg(List *plDQA, Aggref *paggref);

			// translate a query with grouping sets
			CDXLNode *PdxlnGroupingSets
				(
				FromExpr *pfromexpr,
				List *plTargetList,
				List *plGroupClause,
				BOOL fHasAggs,
				HMIUl *phmiulSortGrpColsColId,
				HMIUl *phmiulOutputCols
				);

			// expand the grouping sets into a union all operator
			CDXLNode *PdxlnUnionAllForGroupingSets
				(
				FromExpr *pfromexpr,
				List *plTargetList,
				List *plGroupClause,
				BOOL fHasAggs,
				DrgPbs *pdrgpbsGroupingSets,
				HMIUl *phmiulSortGrpColsColId,
				HMIUl *phmiulOutputCols,
				HMUlUl *phmululGrpColPos		// mapping pos->unique grouping columns for grouping func arguments
				);

			// construct a project node with NULL values for columns not included in the grouping set
			CDXLNode *PdxlnProjectNullsForGroupingSets
				(
				List *plTargetList, 
				CDXLNode *pdxlnChild, 
				CBitSet *pbs, 
				HMIUl *phmiulSortgrouprefCols, 
				HMIUl *phmiulOutputCols, 
				HMUlUl *phmululGrpColPos
				) 
				const;

			// construct a project node with appropriate values for the grouping funcs in the given target list
			CDXLNode *PdxlnProjectGroupingFuncs
				(
				List *plTargetList,
				CDXLNode *pdxlnChild,
				CBitSet *pbs,
				HMIUl *phmiulOutputCols,
				HMUlUl *phmululGrpColPos,
				HMIUl *phmiulSortgrouprefColId
				)
				const;

			// add sorting and grouping column into the hash map
			void AddSortingGroupingColumn(TargetEntry *pte, HMIUl *phmiulSortGrpColsColId, ULONG ulColId) const;

			// translate the list of sorting columns
			DrgPdxln *PdrgpdxlnSortCol(List *plSortCl, HMIUl *phmiulColColId) const;

			// translate the list of partition-by column identifiers
			DrgPul *PdrgpulPartCol(List *plSortCl, HMIUl *phmiulColColId) const;

			CDXLNode *PdxlnLgLimit
				(
				List *plsortcl, // list of sort clauses
				Node *pnodeLimitCount, // query node representing the limit count
				Node *pnodeLimitOffset, // query node representing the limit offset
				CDXLNode *pdxln, // the dxl node representing the subtree
				HMIUl *phmiulGrpColsColId // the mapping between the position in the TargetList to the ColId
				);

			// throws an exception when RTE kind not yet supported
			void UnsupportedRTEKind(RTEKind rtekind) const;

			// translate an entry of the from clause (this can either be FromExpr or JoinExpr)
			CDXLNode *PdxlnFromGPDBFromClauseEntry(Node *pnode);

			// translate the target list entries of the query into a logical project
			CDXLNode *PdxlnLgProjectFromGPDBTL
				(
				List *plTargetList,
				CDXLNode *pdxlnChild,
				HMIUl *	phmiulGrpcolColId,
				HMIUl *phmiulOutputCols,
				List *plGrpCl,
				BOOL fExpandAggrrefExpr = false
				);

			// translate a target list entry or a join alias entry into a project element
			CDXLNode *PdxlnPrEFromGPDBExpr(Expr *pexpr, const CHAR *szAliasName, BOOL fInsistNewColIds = false);

			// translate a CTE into a DXL logical CTE operator
			CDXLNode *PdxlnFromCTE
				(
				const RangeTblEntry *prte,
				ULONG ulRTIndex,
				ULONG ulCurrQueryLevel
				);

			// translate a base table range table entry into a logical get
			CDXLNode *PdxlnFromRelation
				(
				const RangeTblEntry *prte,
				ULONG ulRTIndex,
				ULONG //ulCurrQueryLevel
				);

			// generate a DXL node from column values, where each column value is
			// either a datum or scalar expression represented as a project element.
			CDXLNode *PdxlnFromColumnValues
			 	(
			 	DrgPdxldatum *pdrgpdxldatum,
			 	DrgPdxlcd *pdrgpdxlcdCTG,
			 	DrgPdxln *pdrgpdxlnPE
			    )
			    const;

			// translate a value scan range table entry
			CDXLNode *PdxlnFromValues
				(
				const RangeTblEntry *prte,
				ULONG ulRTIndex,
				ULONG //ulCurrQueryLevel
				);

			// create a dxl node from a array of datums and project elements
			CDXLNode *PdxlnFromTVF
				(
				const RangeTblEntry *prte,
				ULONG ulRTIndex,
				ULONG //ulCurrQueryLevel
				);

			// translate a derived table into a DXL logical operator
			CDXLNode *PdxlnFromDerivedTable
						(
						const RangeTblEntry *prte,
						ULONG ulRTIndex,
						ULONG ulCurrQueryLevel
						);

			// create a DXL node representing the scalar constant "true"
			CDXLNode *PdxlnScConstValueTrue();

			// store mapping attno->colid
			void StoreAttnoColIdMapping(HMIUl *phmiul, INT iAttno, ULONG ulColId) const;

			// construct an array of output columns
			DrgPdxln *PdrgpdxlnConstructOutputCols(List *plTargetList, HMIUl *phmiulAttnoColId) const;

			// check for support command types, throws an exception when command type not yet supported
			void CheckSupportedCmdType(Query *pquery);

			// translate a select-project-join expression into DXL
			CDXLNode *PdxlnSPJ(List *plTargetList, FromExpr *pfromexpr, HMIUl *phmiulSortGroupColsColId, HMIUl *phmiulOutputCols, List *plGroupClause);

			// translate a select-project-join expression into DXL and keep variables appearing
			// in aggregates and grouping columns in the output column map
			CDXLNode *PdxlnSPJForGroupingSets(List *plTargetList, FromExpr *pfromexpr, HMIUl *phmiulSortGroupColsColId, HMIUl *phmiulOutputCols, List *plGroupClause);
			
			// helper to check if OID is included in given array of OIDs
			static
			BOOL FOIDFound(OID oid, const OID rgOID[], ULONG ulSize);

			// check if given operator is lead() window function
			static
			BOOL FLeadWindowFunc(CDXLOperator *Pdxlop);

			// check if given operator is lag() window function
			static
			BOOL FLagWindowFunc(CDXLOperator *pdxlop);

		    // translate an insert query
			CDXLNode *PdxlnInsert();

			// translate a delete query
			CDXLNode *PdxlnDelete();

			// translate an update query
			CDXLNode *PdxlnUpdate();

		    // translate a CTAS query
			CDXLNode *PdxlnCTAS();

			// translate CTAS storage options
			CDXLCtasStorageOptions::DrgPctasOpt *Pdrgpctasopt(List *plOptions, IMDRelation::Erelstoragetype *perelstoragetype);
			
			// extract storage option value from defelem
			CWStringDynamic *PstrExtractOptionValue(DefElem *pdefelem);
			
			// return resno -> colId mapping of columns to be updated
			HMIUl *PhmiulUpdateCols();

			// obtain the ids of the ctid and segmentid columns for the target
			// table of a DML query
			void GetCtidAndSegmentId(ULONG *pulCtid, ULONG *pulSegmentId);
			
			// obtain the column id for the tuple oid column of the target table
			// of a DML statement
			ULONG UlTupleOidColId();

			// translate a grouping func expression
			CDXLNode *PdxlnGroupingFunc(const Expr *pexpr, CBitSet *pbs, HMUlUl *phmululGrpColPos) const;

			// construct a list of CTE producers from the query's CTE list
			void ConstructCTEProducerList(List *plCTE, ULONG ulQueryLevel);
			
			// construct a stack of CTE anchors for each CTE producer in the given array
			void ConstructCTEAnchors(DrgPdxln *pdrgpdxln, CDXLNode **ppdxlnCTEAnchorTop, CDXLNode **ppdxlnCTEAnchorBottom);
			
			// generate an array of new column ids of the given size
			DrgPul *PdrgpulGenerateColIds(IMemoryPool *pmp, ULONG ulSize) const;

			// extract an array of colids from the given column mapping
			DrgPul *PdrgpulExtractColIds(IMemoryPool *pmp, HMIUl *phmiul) const;
			
			// construct a new mapping based on the given one by replacing the colid in the "From" list
			// with the colid at the same position in the "To" list
			HMIUl *PhmiulRemapColIds(IMemoryPool *pmp, HMIUl *phmiul, DrgPul *pdrgpulFrom, DrgPul *pdrgpulTo) const;

			// true iff this query or one of its ancestors is a DML query
			BOOL FDMLQuery();

		public:
			// dtor
			~CTranslatorQueryToDXL();

			// query object
			const Query *Pquery() const
			{
				return m_pquery;
			}

			// does query have distributed tables
			BOOL FHasDistributedTables() const
			{
				return m_fHasDistributedTables;
			}

			// main translation routine for Query -> DXL tree
			CDXLNode *PdxlnFromQueryInternal();

			// main driver
			CDXLNode *PdxlnFromQuery();

			// return the list of output columns
			DrgPdxln *PdrgpdxlnQueryOutput() const;

			// return the list of CTEs
			DrgPdxln *PdrgpdxlnCTE() const;

			// factory function
			static
			CTranslatorQueryToDXL *PtrquerytodxlInstance
				(
				IMemoryPool *pmp,
				CMDAccessor *pmda,
				CIdGenerator *pidgtorColId,
				CIdGenerator *pidgtorCTE,
				CMappingVarColId *pmapvarcolid,
				Query *pquery,
				ULONG ulQueryLevel,
				HMUlCTEListEntry *phmulCTEEntries = NULL // hash map between query level -> list of CTEs defined at that level
				);
	};
}
#endif // GPDXL_CTranslatorQueryToDXL_H

//EOF
