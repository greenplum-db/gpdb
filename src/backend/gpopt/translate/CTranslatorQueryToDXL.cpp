//---------------------------------------------------------------------------
//  Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorQueryToDXL.cpp
//
//	@doc:
//		Implementation of the methods used to translate a query into DXL tree.
//		All translator methods allocate memory in the provided memory pool, and
//		the caller is responsible for freeing it
//
//	@test:
//
//---------------------------------------------------------------------------

#include "postgres.h"

#include "access/sysattr.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"
#include "optimizer/walkers.h"

#include "gpos/base.h"
#include "gpos/common/CAutoTimer.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/translate/CCTEListEntry.h"
#include "gpopt/translate/CQueryMutators.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CTranslatorQueryToDXL.h"
#include "gpopt/translate/CTranslatorDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"

#include "naucrates/exception.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/dxlops.h"
#include "naucrates/dxl/operators/CDXLScalarBooleanTest.h"
#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/IMDTypeInt8.h"
#include "naucrates/md/CMDIdGPDBCtas.h"

#include "naucrates/traceflags/traceflags.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpos;
using namespace gpopt;
using namespace gpnaucrates;
using namespace gpmd;

extern bool	optimizer_enable_ctas;
extern bool optimizer_enable_dml_triggers;
extern bool optimizer_enable_dml_constraints;
extern bool optimizer_enable_multiple_distinct_aggs;

// OIDs of variants of LEAD window function
static const OID rgOIDLead[] =
	{
	7011, 7074, 7075, 7310, 7312,
	7314, 7316, 7318,
	7320, 7322, 7324, 7326, 7328,
	7330, 7332, 7334, 7336, 7338,
	7340, 7342, 7344, 7346, 7348,
	7350, 7352, 7354, 7356, 7358,
	7360, 7362, 7364, 7366, 7368,
	7370, 7372, 7374, 7376, 7378,
	7380, 7382, 7384, 7386, 7388,
	7390, 7392, 7394, 7396, 7398,
	7400, 7402, 7404, 7406, 7408,
	7410, 7412, 7414, 7416, 7418,
	7420, 7422, 7424, 7426, 7428,
	7430, 7432, 7434, 7436, 7438,
	7440, 7442, 7444, 7446, 7448,
	7450, 7452, 7454, 7456, 7458,
	7460, 7462, 7464, 7466, 7468,
	7470, 7472, 7474, 7476, 7478,
	7480, 7482, 7484, 7486, 7488,
	7214, 7215, 7216, 7220, 7222,
	7224, 7244, 7246, 7248, 7260,
	7262, 7264
	};

// OIDs of variants of LAG window function
static const OID rgOIDLag[] =
	{
	7675, 7491, 7493, 7495, 7497, 7499,
	7501, 7503, 7505, 7507, 7509,
	7511, 7513, 7515, 7517, 7519,
	7521, 7523, 7525, 7527, 7529,
	7531, 7533, 7535, 7537, 7539,
	7541, 7543, 7545, 7547, 7549,
	7551, 7553, 7555, 7557, 7559,
	7561, 7563, 7565, 7567, 7569,
	7571, 7573, 7575, 7577, 7579,
	7581, 7583, 7585, 7587, 7589,
	7591, 7593, 7595, 7597, 7599,
	7601, 7603, 7605, 7607, 7609,
	7611, 7613, 7615, 7617, 7619,
	7621, 7623, 7625, 7627, 7629,
	7631, 7633, 7635, 7637, 7639,
	7641, 7643, 7645, 7647, 7649,
	7651, 7653, 7655, 7657, 7659,
	7661, 7663, 7665, 7667, 7669,
	7671, 7673,
	7211, 7212, 7213, 7226, 7228,
	7230, 7250, 7252, 7254, 7266,
	7268, 7270
	};

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CTranslatorQueryToDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CTranslatorQueryToDXL::CTranslatorQueryToDXL
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	CIdGenerator *pidgtorColId,
	CIdGenerator *pidgtorCTE,
	CMappingVarColId *pmapvarcolid,
	Query *pquery,
	ULONG ulQueryLevel,
	BOOL fTopDMLQuery,
	HMUlCTEListEntry *phmulCTEEntries
	)
	:
	m_pmp(pmp),
	m_sysid(IMDId::EmdidGPDB, GPMD_GPDB_SYSID),
	m_pmda(pmda),
	m_pidgtorCol(pidgtorColId),
	m_pidgtorCTE(pidgtorCTE),
	m_pmapvarcolid(pmapvarcolid),
	m_ulQueryLevel(ulQueryLevel),
	m_fHasDistributedTables(false),
	m_fTopDMLQuery(fTopDMLQuery),
	m_fCTASQuery(false),
	m_phmulCTEEntries(NULL),
	m_pdrgpdxlnQueryOutput(NULL),
	m_pdrgpdxlnCTE(NULL),
	m_phmulfCTEProducers(NULL)
{
	GPOS_ASSERT(NULL != pquery);
	CheckSupportedCmdType(pquery);
	
	m_phmulCTEEntries = GPOS_NEW(m_pmp) HMUlCTEListEntry(m_pmp);
	m_pdrgpdxlnCTE = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);
	m_phmulfCTEProducers = GPOS_NEW(m_pmp) HMUlF(m_pmp);
	
	if (NULL != phmulCTEEntries)
	{
		HMIterUlCTEListEntry hmiterullist(phmulCTEEntries);

		while (hmiterullist.FAdvance())
		{
			ULONG ulCTEQueryLevel = *(hmiterullist.Pk());

			CCTEListEntry *pctelistentry =  const_cast<CCTEListEntry *>(hmiterullist.Pt());

			// CTE's that have been defined before the m_ulQueryLevel
			// should only be inserted into the hash map
			// For example:
			// WITH ab as (SELECT a as a, b as b from foo)
			// SELECT *
			// FROM
			// 	(WITH aEq10 as (SELECT b from ab ab1 where ab1.a = 10)
			//  	SELECT *
			//  	FROM (WITH aEq20 as (SELECT b from ab ab2 where ab2.a = 20)
			// 		      SELECT * FROM aEq10 WHERE b > (SELECT min(b) from aEq20)
			// 		      ) dtInner
			// 	) dtOuter
			// When translating the from expression containing "aEq10" in the derived table "dtInner"
			// we have already seen three CTE namely: "ab", "aEq10" and "aEq20". BUT when we expand aEq10
			// in the dt1, we should only have access of CTE's defined prior to its level namely "ab".

			if (ulCTEQueryLevel < ulQueryLevel && NULL != pctelistentry)
			{
				pctelistentry->AddRef();
#ifdef GPOS_DEBUG
				BOOL fRes =
#endif
				m_phmulCTEEntries->FInsert(GPOS_NEW(pmp) ULONG(ulCTEQueryLevel), pctelistentry);
				GPOS_ASSERT(fRes);
			}
		}
	}

	// check if the query has any unsupported node types
	CheckUnsupportedNodeTypes(pquery);

	// check if the query has SIRV functions in the targetlist without a FROM clause
	CheckSirvFuncsWithoutFromClause(pquery);

	// first normalize the query
	m_pquery = CQueryMutators::PqueryNormalize(m_pmp, m_pmda, pquery, ulQueryLevel);

	if (NULL != m_pquery->cteList)
	{
		ConstructCTEProducerList(m_pquery->cteList, ulQueryLevel);
	}

	m_psctranslator = GPOS_NEW(m_pmp) CTranslatorScalarToDXL
									(
									m_pmp,
									m_pmda,
									m_pidgtorCol,
									m_pidgtorCTE,
									m_ulQueryLevel,
									true, /* m_fQuery */
									m_phmulCTEEntries,
									m_pdrgpdxlnCTE
									);

}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PtrquerytodxlInstance
//
//	@doc:
//		Factory function
//
//---------------------------------------------------------------------------
CTranslatorQueryToDXL *
CTranslatorQueryToDXL::PtrquerytodxlInstance
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	CIdGenerator *pidgtorColId,
	CIdGenerator *pidgtorCTE,
	CMappingVarColId *pmapvarcolid,
	Query *pquery,
	ULONG ulQueryLevel,
	HMUlCTEListEntry *phmulCTEEntries
	)
{
	return GPOS_NEW(pmp) CTranslatorQueryToDXL
		(
		pmp,
		pmda,
		pidgtorColId,
		pidgtorCTE,
		pmapvarcolid,
		pquery,
		ulQueryLevel,
		false,  // fTopDMLQuery
		phmulCTEEntries
		);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::~CTranslatorQueryToDXL
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CTranslatorQueryToDXL::~CTranslatorQueryToDXL()
{
	GPOS_DELETE(m_psctranslator);
	GPOS_DELETE(m_pmapvarcolid);
	gpdb::GPDBFree(m_pquery);
	m_phmulCTEEntries->Release();
	m_pdrgpdxlnCTE->Release();
	m_phmulfCTEProducers->Release();
	CRefCount::SafeRelease(m_pdrgpdxlnQueryOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CheckUnsupportedNodeTypes
//
//	@doc:
//		Check for unsupported node types, and throws an exception when found
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::CheckUnsupportedNodeTypes
	(
	Query *pquery
	)
{
	static const SUnsupportedFeature rgUnsupported[] =
	{
		{T_RowExpr, GPOS_WSZ_LIT("ROW EXPRESSION")},
		{T_RowCompareExpr, GPOS_WSZ_LIT("ROW COMPARE")},
		{T_FieldSelect, GPOS_WSZ_LIT("FIELDSELECT")},
		{T_FieldStore, GPOS_WSZ_LIT("FIELDSTORE")},
		{T_CoerceToDomainValue, GPOS_WSZ_LIT("COERCETODOMAINVALUE")},
		{T_GroupId, GPOS_WSZ_LIT("GROUPID")},
		{T_PercentileExpr, GPOS_WSZ_LIT("PERCENTILE")},
		{T_CurrentOfExpr, GPOS_WSZ_LIT("CURRENT OF")},
	};

	List *plUnsupported = NIL;
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgUnsupported); ul++)
	{
		plUnsupported = gpdb::PlAppendInt(plUnsupported, rgUnsupported[ul].ent);
	}

	INT iUnsupported = gpdb::IFindNodes((Node *) pquery, plUnsupported);
	gpdb::GPDBFree(plUnsupported);

	if (0 <= iUnsupported)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, rgUnsupported[iUnsupported].m_wsz);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CheckSirvFuncsWithoutFromClause
//
//	@doc:
//		Check for SIRV functions in the target list without a FROM clause, and
//		throw an exception when found
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::CheckSirvFuncsWithoutFromClause
	(
	Query *pquery
	)
{
	// if there is a FROM clause or if target list is empty, look no further
	if ((NULL != pquery->jointree && 0 < gpdb::UlListLength(pquery->jointree->fromlist))
		|| NIL == pquery->targetList)
	{
		return;
	}

	// see if we have SIRV functions in the target list
	if (FHasSirvFunctions((Node *) pquery->targetList))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::FHasSirvFunctions
//
//	@doc:
//		Check for SIRV functions in the tree rooted at the given node
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FHasSirvFunctions
	(
	Node *pnode
	)
	const
{
	GPOS_ASSERT(NULL != pnode);

	List *plFunctions =	gpdb::PlExtractNodesExpression(pnode, T_FuncExpr, true /*descendIntoSubqueries*/);
	ListCell *plc = NULL;

	BOOL fHasSirv = false;
	ForEach (plc, plFunctions)
	{
		FuncExpr *pfuncexpr = (FuncExpr *) lfirst(plc);
		if (CTranslatorUtils::FSirvFunc(m_pmp, m_pmda, pfuncexpr->funcid))
		{
			fHasSirv = true;
			break;
		}
	}
	gpdb::FreeList(plFunctions);

	return fHasSirv;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CheckSupportedCmdType
//
//	@doc:
//		Check for supported command types, throws an exception when command
//		type not yet supported
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::CheckSupportedCmdType
	(
	Query *pquery
	)
{
	if (NULL != pquery->utilityStmt)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("UTILITY command"));
	}

	if (CMD_SELECT == pquery->commandType)
	{		
		if (!optimizer_enable_ctas && NULL != pquery->intoClause)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("CTAS. Set optimizer_enable_ctas to on to enable CTAS with GPORCA"));
		}
		
		// supported: regular select or CTAS when it is enabled
		return;
	}

	static const SCmdNameElem rgStrMap[] =
		{
		{CMD_UTILITY, GPOS_WSZ_LIT("UTILITY command")}
		};

	const ULONG ulLen = GPOS_ARRAY_SIZE(rgStrMap);
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		SCmdNameElem mapelem = rgStrMap[ul];
		if (mapelem.m_cmdtype == pquery->commandType)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, mapelem.m_wsz);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpdxlnQueryOutput
//
//	@doc:
//		Return the list of query output columns
//
//---------------------------------------------------------------------------
DrgPdxln *
CTranslatorQueryToDXL::PdrgpdxlnQueryOutput() const
{
	return m_pdrgpdxlnQueryOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpdxlnCTE
//
//	@doc:
//		Return the list of CTEs
//
//---------------------------------------------------------------------------
DrgPdxln *
CTranslatorQueryToDXL::PdrgpdxlnCTE() const
{
	return m_pdrgpdxlnCTE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromQueryInternal
//
//	@doc:
//		Translates a Query into a DXL tree. The function allocates memory in
//		the translator memory pool, and caller is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromQueryInternal()
{
	// The parsed query contains an RTE for the view, which is maintained all the way through planned statement.
	// This entries is annotated as requiring SELECT permissions for the current user.
	// In Orca, we only keep range table entries for the base tables in the planned statement, but not for the view itself.
	// Since permissions are only checked during ExecutorStart, we lose track of the permissions required for the view and the select goes through successfully.
	// We therefore need to check permissions before we go into optimization for all RTEs, including the ones not explicitly referred in the query, e.g. views.
	CTranslatorUtils::CheckRTEPermissions(m_pquery->rtable);
	
	CDXLNode *pdxlnChild = NULL;
	HMIUl *phmiulSortGroupColsColId =  GPOS_NEW(m_pmp) HMIUl(m_pmp);
	HMIUl *phmiulOutputCols = GPOS_NEW(m_pmp) HMIUl(m_pmp);

	// construct CTEAnchor operators for the CTEs defined at the top level
	CDXLNode *pdxlnCTEAnchorTop = NULL;
	CDXLNode *pdxlnCTEAnchorBottom = NULL;
	ConstructCTEAnchors(m_pdrgpdxlnCTE, &pdxlnCTEAnchorTop, &pdxlnCTEAnchorBottom);
	GPOS_ASSERT_IMP(m_pdrgpdxlnCTE == NULL || 0 < m_pdrgpdxlnCTE->UlLength(),
					NULL != pdxlnCTEAnchorTop && NULL != pdxlnCTEAnchorBottom);
	
	GPOS_ASSERT_IMP(NULL != m_pquery->setOperations, 0 == gpdb::UlListLength(m_pquery->windowClause));
	if (NULL != m_pquery->setOperations)
	{
		List *plTargetList = m_pquery->targetList;
		// translate set operations
		pdxlnChild = PdxlnFromSetOp(m_pquery->setOperations, plTargetList, phmiulOutputCols);

		CDXLLogicalSetOp *pdxlop = CDXLLogicalSetOp::PdxlopConvert(pdxlnChild->Pdxlop());
		const DrgPdxlcd *pdrgpdxlcd = pdxlop->Pdrgpdxlcd();
		ListCell *plcTE = NULL;
		ULONG ulResNo = 1;
		ForEach (plcTE, plTargetList)
		{
			TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
			if (0 < pte->ressortgroupref)
			{
				ULONG ulColId = ((*pdrgpdxlcd)[ulResNo - 1])->UlID();
				AddSortingGroupingColumn(pte, phmiulSortGroupColsColId, ulColId);
			}
			ulResNo++;
		}
	}
	else if (0 != gpdb::UlListLength(m_pquery->windowClause)) // translate window clauses
	{
		CDXLNode *pdxln = PdxlnFromGPDBFromExpr(m_pquery->jointree);
		GPOS_ASSERT(NULL == m_pquery->groupClause);
		pdxlnChild = PdxlnWindow
						(
						pdxln,
						m_pquery->targetList,
						m_pquery->windowClause,
						m_pquery->sortClause,
						phmiulSortGroupColsColId,
						phmiulOutputCols
						);
	}
	else
	{
		pdxlnChild = PdxlnGroupingSets(m_pquery->jointree, m_pquery->targetList, m_pquery->groupClause, m_pquery->hasAggs, phmiulSortGroupColsColId, phmiulOutputCols);
	}

	// translate limit clause
	CDXLNode *pdxlnLimit = PdxlnLgLimit(m_pquery->sortClause, m_pquery->limitCount, m_pquery->limitOffset, pdxlnChild, phmiulSortGroupColsColId);


	if (NULL == m_pquery->targetList)
	{
		m_pdrgpdxlnQueryOutput = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);
	}
	else
	{
		m_pdrgpdxlnQueryOutput = PdrgpdxlnConstructOutputCols(m_pquery->targetList, phmiulOutputCols);
	}

	// cleanup
	CRefCount::SafeRelease(phmiulSortGroupColsColId);

	phmiulOutputCols->Release();
	
	// add CTE anchors if needed
	CDXLNode *pdxlnResult = pdxlnLimit;
	
	if (NULL != pdxlnCTEAnchorTop)
	{
		GPOS_ASSERT(NULL != pdxlnCTEAnchorBottom);
		pdxlnCTEAnchorBottom->AddChild(pdxlnResult);
		pdxlnResult = pdxlnCTEAnchorTop;
	}
	
	return pdxlnResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnSPJ
//
//	@doc:
//		Construct a DXL SPJ tree from the given query parts
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnSPJ
	(
	List *plTargetList,
	FromExpr *pfromexpr,
	HMIUl *phmiulSortGroupColsColId,
	HMIUl *phmiulOutputCols,
	List *plGroupClause
	)
{
	CDXLNode *pdxlnJoinTree = PdxlnFromGPDBFromExpr(pfromexpr);

	// translate target list entries into a logical project
	return PdxlnLgProjectFromGPDBTL(plTargetList, pdxlnJoinTree, phmiulSortGroupColsColId, phmiulOutputCols, plGroupClause);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnSPJForGroupingSets
//
//	@doc:
//		Construct a DXL SPJ tree from the given query parts, and keep variables
//		appearing in aggregates in the project list
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnSPJForGroupingSets
	(
	List *plTargetList,
	FromExpr *pfromexpr,
	HMIUl *phmiulSortGroupColsColId,
	HMIUl *phmiulOutputCols,
	List *plGroupClause
	)
{
	CDXLNode *pdxlnJoinTree = PdxlnFromGPDBFromExpr(pfromexpr);

	// translate target list entries into a logical project
	return PdxlnLgProjectFromGPDBTL(plTargetList, pdxlnJoinTree, phmiulSortGroupColsColId, phmiulOutputCols, plGroupClause, true /*fExpandAggrefExpr*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromQuery
//
//	@doc:
//		Main driver
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromQuery()
{
	CAutoTimer at("\n[OPT]: Query To DXL Translation Time", GPOS_FTRACE(EopttracePrintOptimizationStatistics));

	switch (m_pquery->commandType)
	{
		case CMD_SELECT:
			if (NULL == m_pquery->intoClause)
			{
				return PdxlnFromQueryInternal();
			}

			return PdxlnCTAS();
			
		case CMD_INSERT:
			return PdxlnInsert();

		case CMD_DELETE:
			return PdxlnDelete();

		case CMD_UPDATE:
			return PdxlnUpdate();

		default:
			GPOS_ASSERT(!"Statement type not supported");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnInsert
//
//	@doc:
//		Translate an insert stmt
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnInsert()
{
	GPOS_ASSERT(CMD_INSERT == m_pquery->commandType);
	GPOS_ASSERT(0 < m_pquery->resultRelation);

	CDXLNode *pdxlnQuery = PdxlnFromQueryInternal();
	const RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pquery->rtable, m_pquery->resultRelation - 1);

	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);
	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxltabdesc->Pmdid());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::FRelHasTriggers(m_pmp, m_pmda, pmdrel, Edxldmlinsert))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("INSERT with triggers"));
	}

	BOOL fRelHasConstraints = CTranslatorUtils::FRelHasConstraints(pmdrel);
	if (!optimizer_enable_dml_constraints && fRelHasConstraints)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("INSERT with constraints"));
	}
	
	const ULONG ulLenTblCols = CTranslatorUtils::UlNonSystemColumns(pmdrel);
	const ULONG ulLenTL = gpdb::UlListLength(m_pquery->targetList);
	GPOS_ASSERT(ulLenTblCols >= ulLenTL);
	GPOS_ASSERT(ulLenTL == m_pdrgpdxlnQueryOutput->UlLength());

	CDXLNode *pdxlnPrL = NULL;
	
	const ULONG ulSystemCols = pmdrel->UlColumns() - ulLenTblCols;
	const ULONG ulLenNonDroppedCols = pmdrel->UlNonDroppedCols() - ulSystemCols;
	if (ulLenNonDroppedCols > ulLenTL)
	{
		// missing target list entries
		pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));
	}

	DrgPul *pdrgpulSource = GPOS_NEW(m_pmp) DrgPul(m_pmp);

	ULONG ulPosTL = 0;
	for (ULONG ul = 0; ul < ulLenTblCols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->Pmdcol(ul);
		GPOS_ASSERT(!pmdcol->FSystemColumn());
		
		if (pmdcol->FDropped())
		{
			continue;
		}
		
		if (ulPosTL < ulLenTL)
		{
			INT iAttno = pmdcol->IAttno();
			
			TargetEntry *pte = (TargetEntry *) gpdb::PvListNth(m_pquery->targetList, ulPosTL);
			AttrNumber iResno = pte->resno;

			if (iAttno == iResno)
			{
				CDXLNode *pdxlnCol = (*m_pdrgpdxlnQueryOutput)[ulPosTL];
				CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::PdxlopConvert(pdxlnCol->Pdxlop());
				pdrgpulSource->Append(GPOS_NEW(m_pmp) ULONG(pdxlopIdent->Pdxlcr()->UlID()));
				ulPosTL++;
				continue;
			}
		}

		// target entry corresponding to the tables column not found, therefore
		// add a project element with null value scalar child
		CDXLNode *pdxlnPrE = CTranslatorUtils::PdxlnPrElNull(m_pmp, m_pmda, m_pidgtorCol, pmdcol);
		ULONG ulColId = CDXLScalarProjElem::PdxlopConvert(pdxlnPrE->Pdxlop())->UlId();
 	 	pdxlnPrL->AddChild(pdxlnPrE);
	 	pdrgpulSource->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
	}

	CDXLLogicalInsert *pdxlopInsert = GPOS_NEW(m_pmp) CDXLLogicalInsert(m_pmp, pdxltabdesc, pdrgpulSource);

	if (NULL != pdxlnPrL)
	{
		GPOS_ASSERT(0 < pdxlnPrL->UlArity());
		
		CDXLNode *pdxlnProject = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp));
		pdxlnProject->AddChild(pdxlnPrL);
		pdxlnProject->AddChild(pdxlnQuery);
		pdxlnQuery = pdxlnProject;
	}

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopInsert, pdxlnQuery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnCTAS
//
//	@doc:
//		Translate a CTAS
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnCTAS()
{
	GPOS_ASSERT(CMD_SELECT == m_pquery->commandType);
	GPOS_ASSERT(NULL != m_pquery->intoClause);

	m_fCTASQuery = true;
	CDXLNode *pdxlnQuery = PdxlnFromQueryInternal();
	
	IntoClause *pintocl = m_pquery->intoClause;
		
	CMDName *pmdnameRel = CDXLUtils::PmdnameFromSz(m_pmp, pintocl->rel->relname);
	
	DrgPdxlcd *pdrgpdxlcd = GPOS_NEW(m_pmp) DrgPdxlcd(m_pmp);
	
	const ULONG ulColumns = gpdb::UlListLength(m_pquery->targetList);

	DrgPul *pdrgpulSource = GPOS_NEW(m_pmp) DrgPul(m_pmp);
	DrgPi* pdrgpiVarTypMod = GPOS_NEW(m_pmp) DrgPi(m_pmp);
	
	List *plColnames = pintocl->colNames;
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		TargetEntry *pte = (TargetEntry *) gpdb::PvListNth(m_pquery->targetList, ul);
		if (pte->resjunk)
		{
			continue;
		}
		AttrNumber iResno = pte->resno;
		int iVarTypMod = gpdb::IExprTypeMod((Node*)pte->expr);
		pdrgpiVarTypMod->Append(GPOS_NEW(m_pmp) INT(iVarTypMod));

		CDXLNode *pdxlnCol = (*m_pdrgpdxlnQueryOutput)[ul];
		CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::PdxlopConvert(pdxlnCol->Pdxlop());
		pdrgpulSource->Append(GPOS_NEW(m_pmp) ULONG(pdxlopIdent->Pdxlcr()->UlID()));
		
		CMDName *pmdnameCol = NULL;
		if (NULL != plColnames && ul < gpdb::UlListLength(plColnames))
		{
			ColumnDef *pcoldef = (ColumnDef *) gpdb::PvListNth(plColnames, ul);
			pmdnameCol = CDXLUtils::PmdnameFromSz(m_pmp, pcoldef->colname);
		}
		else
		{
			pmdnameCol = GPOS_NEW(m_pmp) CMDName(m_pmp, pdxlopIdent->Pdxlcr()->Pmdname()->Pstr());
		}
		
		GPOS_ASSERT(NULL != pmdnameCol);
		IMDId *pmdid = pdxlopIdent->PmdidType();
		pmdid->AddRef();
		CDXLColDescr *pdxlcd = GPOS_NEW(m_pmp) CDXLColDescr
											(
											m_pmp,
											pmdnameCol,
											m_pidgtorCol->UlNextId(),
											iResno /* iAttno */,
											pmdid,
											false /* fDropped */
											);
		pdrgpdxlcd->Append(pdxlcd);
	}

	IMDRelation::Ereldistrpolicy ereldistrpolicy = IMDRelation::EreldistrRandom;
	DrgPul *pdrgpulDistr = NULL;
	
	if (NULL != m_pquery->intoPolicy)
	{
		ereldistrpolicy = CTranslatorRelcacheToDXL::Ereldistribution(m_pquery->intoPolicy);
		
		if (IMDRelation::EreldistrHash == ereldistrpolicy)
		{
			pdrgpulDistr = GPOS_NEW(m_pmp) DrgPul(m_pmp);

			for (ULONG ul = 0; ul < (ULONG) m_pquery->intoPolicy->nattrs; ul++)
			{
				AttrNumber attno = m_pquery->intoPolicy->attrs[ul];
				GPOS_ASSERT(0 < attno);
				pdrgpulDistr->Append(GPOS_NEW(m_pmp) ULONG(attno - 1));
			}
		}
	}
	else
	{
		GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION,
					   NOTICE,
					   "Table doesn't have 'DISTRIBUTED BY' clause. Creating a NULL policy entry.",
					   NULL);
	}
	
	GPOS_ASSERT(IMDRelation::EreldistrMasterOnly != ereldistrpolicy);
	m_fHasDistributedTables = true;

	// TODO: Mar 5, 2014; reserve an OID
	OID oid = 1;
	CMDIdGPDB *pmdid = GPOS_NEW(m_pmp) CMDIdGPDBCtas(oid);
	
	CMDName *pmdnameTableSpace = NULL;
	if (NULL != pintocl->tableSpaceName)
	{
		pmdnameTableSpace = CDXLUtils::PmdnameFromSz(m_pmp, pintocl->tableSpaceName);
	}
	
	CMDName *pmdnameSchema = NULL;
	if (NULL != pintocl->rel->schemaname)
	{
		pmdnameSchema = CDXLUtils::PmdnameFromSz(m_pmp, pintocl->rel->schemaname);
	}
	
	CDXLCtasStorageOptions::ECtasOnCommitAction ectascommit = (CDXLCtasStorageOptions::ECtasOnCommitAction) pintocl->onCommit;
	
	IMDRelation::Erelstoragetype erelstorage = IMDRelation::ErelstorageHeap;
	CDXLCtasStorageOptions::DrgPctasOpt *pdrgpctasopt = Pdrgpctasopt(pintocl->options, &erelstorage);
	
	BOOL fHasOids = gpdb::FInterpretOidsOption(pintocl->options);
	CDXLLogicalCTAS *pdxlopCTAS = GPOS_NEW(m_pmp) CDXLLogicalCTAS
									(
									m_pmp, 
									pmdid,
									pmdnameSchema,
									pmdnameRel, 
									pdrgpdxlcd, 
									GPOS_NEW(m_pmp) CDXLCtasStorageOptions(pmdnameTableSpace, ectascommit, pdrgpctasopt),
									ereldistrpolicy,
									pdrgpulDistr,  
									pintocl->rel->istemp, 
									fHasOids,
									erelstorage, 
									pdrgpulSource,
									pdrgpiVarTypMod
									);

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopCTAS, pdxlnQuery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::Pdrgpctasopt
//
//	@doc:
//		Translate CTAS storage options
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::DrgPctasOpt *
CTranslatorQueryToDXL::Pdrgpctasopt
	(
	List *plOptions,
	IMDRelation::Erelstoragetype *perelstoragetype // output parameter: storage type
	)
{
	if (NULL == plOptions)
	{
		return NULL;
	}

	GPOS_ASSERT(NULL != perelstoragetype);
	
	CDXLCtasStorageOptions::DrgPctasOpt *pdrgpctasopt = GPOS_NEW(m_pmp) CDXLCtasStorageOptions::DrgPctasOpt(m_pmp);
	ListCell *plc = NULL;
	BOOL fAO = false;
	BOOL fAOCO = false;
	BOOL fParquet = false;
	
	CWStringConst strAppendOnly(GPOS_WSZ_LIT("appendonly"));
	CWStringConst strOrientation(GPOS_WSZ_LIT("orientation"));
	CWStringConst strOrientationParquet(GPOS_WSZ_LIT("parquet"));
	CWStringConst strOrientationColumn(GPOS_WSZ_LIT("column"));
	
	ForEach (plc, plOptions)
	{
		DefElem *pdefelem = (DefElem *) lfirst(plc);
		CWStringDynamic *pstrName = CDXLUtils::PstrFromSz(m_pmp, pdefelem->defname);
		CWStringDynamic *pstrValue = NULL;
		
		BOOL fNullArg = (NULL == pdefelem->arg);

		// pdefelem->arg is NULL for queries of the form "create table t with (oids) as ... "
		if (fNullArg)
		{
			// we represent null options as an empty arg string and set the IsNull flag on
			pstrValue = GPOS_NEW(m_pmp) CWStringDynamic(m_pmp);
		}
		else
		{
			pstrValue = PstrExtractOptionValue(pdefelem);

			if (pstrName->FEquals(&strAppendOnly) && pstrValue->FEquals(CDXLTokens::PstrToken(EdxltokenTrue)))
			{
				fAO = true;
			}
			
			if (pstrName->FEquals(&strOrientation) && pstrValue->FEquals(&strOrientationColumn))
			{
				GPOS_ASSERT(!fParquet);
				fAOCO = true;
			}

			if (pstrName->FEquals(&strOrientation) && pstrValue->FEquals(&strOrientationParquet))
			{
				GPOS_ASSERT(!fAOCO);
				fParquet = true;
			}
		}

		NodeTag argType = T_Null;
		if (!fNullArg)
		{
			argType = pdefelem->arg->type;
		}

		CDXLCtasStorageOptions::CDXLCtasOption *pdxlctasopt =
				GPOS_NEW(m_pmp) CDXLCtasStorageOptions::CDXLCtasOption(argType, pstrName, pstrValue, fNullArg);
		pdrgpctasopt->Append(pdxlctasopt);
	}
	if (fAOCO)
	{
		*perelstoragetype = IMDRelation::ErelstorageAppendOnlyCols;
	}
	else if (fAO)
	{
		*perelstoragetype = IMDRelation::ErelstorageAppendOnlyRows;
	}
	else if (fParquet)
	{
		*perelstoragetype = IMDRelation::ErelstorageAppendOnlyParquet;
	}
	
	return pdrgpctasopt;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PstrExtractOptionValue
//
//	@doc:
//		Extract value for storage option
//
//---------------------------------------------------------------------------
CWStringDynamic *
CTranslatorQueryToDXL::PstrExtractOptionValue
	(
	DefElem *pdefelem
	)
{
	GPOS_ASSERT(NULL != pdefelem);

	CHAR *szValue = gpdb::SzDefGetString(pdefelem);

	CWStringDynamic *pstrResult = CDXLUtils::PstrFromSz(m_pmp, szValue);
	
	return pstrResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::GetCtidAndSegmentId
//
//	@doc:
//		Obtains the ids of the ctid and segmentid columns for the target
//		table of a DML query
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::GetCtidAndSegmentId
	(
	ULONG *pulCtid,
	ULONG *pulSegmentId
	)
{
	// ctid column id
	IMDId *pmdid = CTranslatorUtils::PmdidSystemColType(m_pmp, SelfItemPointerAttributeNumber);
	*pulCtid = CTranslatorUtils::UlColId(m_ulQueryLevel, m_pquery->resultRelation, SelfItemPointerAttributeNumber, pmdid, m_pmapvarcolid);
	pmdid->Release();

	// segmentid column id
	pmdid = CTranslatorUtils::PmdidSystemColType(m_pmp, GpSegmentIdAttributeNumber);
	*pulSegmentId = CTranslatorUtils::UlColId(m_ulQueryLevel, m_pquery->resultRelation, GpSegmentIdAttributeNumber, pmdid, m_pmapvarcolid);
	pmdid->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::UlTupleOidColId
//
//	@doc:
//		Obtains the id of the tuple oid column for the target table of a DML
//		update
//
//---------------------------------------------------------------------------
ULONG
CTranslatorQueryToDXL::UlTupleOidColId()
{
	IMDId *pmdid = CTranslatorUtils::PmdidSystemColType(m_pmp, ObjectIdAttributeNumber);
	ULONG ulTupleOidColId = CTranslatorUtils::UlColId(m_ulQueryLevel, m_pquery->resultRelation, ObjectIdAttributeNumber, pmdid, m_pmapvarcolid);
	pmdid->Release();
	return ulTupleOidColId;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnDelete
//
//	@doc:
//		Translate a delete stmt
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnDelete()
{
	GPOS_ASSERT(CMD_DELETE == m_pquery->commandType);
	GPOS_ASSERT(0 < m_pquery->resultRelation);

	CDXLNode *pdxlnQuery = PdxlnFromQueryInternal();
	const RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pquery->rtable, m_pquery->resultRelation - 1);

	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);
	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxltabdesc->Pmdid());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::FRelHasTriggers(m_pmp, m_pmda, pmdrel, Edxldmldelete))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("DELETE with triggers"));
	}

	ULONG ulCtid = 0;
	ULONG ulSegmentId = 0;
	GetCtidAndSegmentId(&ulCtid, &ulSegmentId);

	DrgPul *pdrgpulDelete = GPOS_NEW(m_pmp) DrgPul(m_pmp);

	const ULONG ulRelColumns = pmdrel->UlColumns();
	for (ULONG ul = 0; ul < ulRelColumns; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->Pmdcol(ul);
		if (pmdcol->FSystemColumn() || pmdcol->FDropped())
		{
			continue;
		}

		ULONG ulColId = CTranslatorUtils::UlColId(m_ulQueryLevel, m_pquery->resultRelation, pmdcol->IAttno(), pmdcol->PmdidType(), m_pmapvarcolid);
		pdrgpulDelete->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
	}

	CDXLLogicalDelete *pdxlopdelete = GPOS_NEW(m_pmp) CDXLLogicalDelete(m_pmp, pdxltabdesc, ulCtid, ulSegmentId, pdrgpulDelete);

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopdelete, pdxlnQuery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnUpdate
//
//	@doc:
//		Translate an update stmt
//
//---------------------------------------------------------------------------

CDXLNode *
CTranslatorQueryToDXL::PdxlnUpdate()
{
	GPOS_ASSERT(CMD_UPDATE == m_pquery->commandType);
	GPOS_ASSERT(0 < m_pquery->resultRelation);

	CDXLNode *pdxlnQuery = PdxlnFromQueryInternal();
	const RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pquery->rtable, m_pquery->resultRelation - 1);

	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);
	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxltabdesc->Pmdid());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::FRelHasTriggers(m_pmp, m_pmda, pmdrel, Edxldmlupdate))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("UPDATE with triggers"));
	}
	
	if (!optimizer_enable_dml_constraints && CTranslatorUtils::FRelHasConstraints(pmdrel))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("UPDATE with constraints"));
	}
	

	ULONG ulCtidColId = 0;
	ULONG ulSegmentIdColId = 0;
	GetCtidAndSegmentId(&ulCtidColId, &ulSegmentIdColId);
	
	ULONG ulTupleOidColId = 0;
	

	BOOL fHasOids = pmdrel->FHasOids();
	if (fHasOids)
	{
		ulTupleOidColId = UlTupleOidColId();
	}

	// get (resno -> colId) mapping of columns to be updated
	HMIUl *phmiulUpdateCols = PhmiulUpdateCols();

	const ULONG ulRelColumns = pmdrel->UlColumns();
	DrgPul *pdrgpulInsert = GPOS_NEW(m_pmp) DrgPul(m_pmp);
	DrgPul *pdrgpulDelete = GPOS_NEW(m_pmp) DrgPul(m_pmp);

	for (ULONG ul = 0; ul < ulRelColumns; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->Pmdcol(ul);
		if (pmdcol->FSystemColumn() || pmdcol->FDropped())
		{
			continue;
		}

		INT iAttno = pmdcol->IAttno();
		ULONG *pulColId = phmiulUpdateCols->PtLookup(&iAttno);

		ULONG ulColId = CTranslatorUtils::UlColId(m_ulQueryLevel, m_pquery->resultRelation, iAttno, pmdcol->PmdidType(), m_pmapvarcolid);

		// if the column is in the query outputs then use it
		// otherwise get the column id created by the child query
		if (NULL != pulColId)
		{
			pdrgpulInsert->Append(GPOS_NEW(m_pmp) ULONG(*pulColId));
		}
		else
		{
			pdrgpulInsert->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
		}

		pdrgpulDelete->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
	}

	phmiulUpdateCols->Release();
	CDXLLogicalUpdate *pdxlopupdate = GPOS_NEW(m_pmp) CDXLLogicalUpdate(m_pmp, pdxltabdesc, ulCtidColId, ulSegmentIdColId, pdrgpulDelete, pdrgpulInsert, fHasOids, ulTupleOidColId);

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopupdate, pdxlnQuery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PhmiulUpdateCols
//
//	@doc:
// 		Return resno -> colId mapping of columns to be updated
//
//---------------------------------------------------------------------------
HMIUl *
CTranslatorQueryToDXL::PhmiulUpdateCols()
{
	GPOS_ASSERT(gpdb::UlListLength(m_pquery->targetList) == m_pdrgpdxlnQueryOutput->UlLength());
	HMIUl *phmiulUpdateCols = GPOS_NEW(m_pmp) HMIUl(m_pmp);

	ListCell *plc = NULL;
	ULONG ul = 0;
	ForEach (plc, m_pquery->targetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plc);
		GPOS_ASSERT(IsA(pte, TargetEntry));
		ULONG ulResno = pte->resno;
		GPOS_ASSERT(0 < ulResno);

		CDXLNode *pdxlnCol = (*m_pdrgpdxlnQueryOutput)[ul];
		CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::PdxlopConvert(pdxlnCol->Pdxlop());
		ULONG ulColId = pdxlopIdent->Pdxlcr()->UlID();

		StoreAttnoColIdMapping(phmiulUpdateCols, ulResno, ulColId);
		ul++;
	}

	return phmiulUpdateCols;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::FOIDFound
//
//	@doc:
// 		Helper to check if OID is included in given array of OIDs
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FOIDFound
	(
	OID oid,
	const OID rgOID[],
	ULONG ulSize
	)
{
	BOOL fFound = false;
	for (ULONG ul = 0; !fFound && ul < ulSize; ul++)
	{
		fFound = (rgOID[ul] == oid);
	}

	return fFound;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::FLeadWindowFunc
//
//	@doc:
// 		Check if given operator is LEAD window function
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FLeadWindowFunc
	(
	CDXLOperator *pdxlop
	)
{
	BOOL fLead = false;
	if (EdxlopScalarWindowRef == pdxlop->Edxlop())
	{
		CDXLScalarWindowRef *pdxlopWinref = CDXLScalarWindowRef::PdxlopConvert(pdxlop);
		const CMDIdGPDB *pmdidgpdb = CMDIdGPDB::PmdidConvert(pdxlopWinref->PmdidFunc());
		OID oid = pmdidgpdb->OidObjectId();
		fLead =  FOIDFound(oid, rgOIDLead, GPOS_ARRAY_SIZE(rgOIDLead));
	}

	return fLead;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::FLagWindowFunc
//
//	@doc:
// 		Check if given operator is LAG window function
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FLagWindowFunc
	(
	CDXLOperator *pdxlop
	)
{
	BOOL fLag = false;
	if (EdxlopScalarWindowRef == pdxlop->Edxlop())
	{
		CDXLScalarWindowRef *pdxlopWinref = CDXLScalarWindowRef::PdxlopConvert(pdxlop);
		const CMDIdGPDB *pmdidgpdb = CMDIdGPDB::PmdidConvert(pdxlopWinref->PmdidFunc());
		OID oid = pmdidgpdb->OidObjectId();
		fLag =  FOIDFound(oid, rgOIDLag, GPOS_ARRAY_SIZE(rgOIDLag));
	}

	return fLag;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlwfLeadLag
//
//	@doc:
// 		Manufacture window frame for lead/lag functions
//
//---------------------------------------------------------------------------
CDXLWindowFrame *
CTranslatorQueryToDXL::PdxlwfLeadLag
	(
	BOOL fLead,
	CDXLNode *pdxlnOffset
	)
	const
{
	EdxlFrameBoundary edxlfbLead = EdxlfbBoundedFollowing;
	EdxlFrameBoundary edxlfbTrail = EdxlfbBoundedFollowing;
	if (!fLead)
	{
		edxlfbLead = EdxlfbBoundedPreceding;
		edxlfbTrail = EdxlfbBoundedPreceding;
	}

	CDXLNode *pdxlnLeadEdge = NULL;
	CDXLNode *pdxlnTrailEdge = NULL;
	if (NULL == pdxlnOffset)
	{
		pdxlnLeadEdge = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarWindowFrameEdge(m_pmp, true /* fLeading */, edxlfbLead));
		pdxlnTrailEdge = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarWindowFrameEdge(m_pmp, false /* fLeading */, edxlfbTrail));

		pdxlnLeadEdge->AddChild(CTranslatorUtils::PdxlnInt8Const(m_pmp, m_pmda, 1 /*iVal*/));
		pdxlnTrailEdge->AddChild(CTranslatorUtils::PdxlnInt8Const(m_pmp, m_pmda, 1 /*iVal*/));
	}
	else
	{
		// overwrite frame edge types based on specified offset type
		if (EdxlopScalarConstValue != pdxlnOffset->Pdxlop()->Edxlop())
		{
			if (fLead)
			{
				edxlfbLead = EdxlfbDelayedBoundedFollowing;
				edxlfbTrail = EdxlfbDelayedBoundedFollowing;
			}
			else
			{
				edxlfbLead = EdxlfbDelayedBoundedPreceding;
				edxlfbTrail = EdxlfbDelayedBoundedPreceding;
			}
		}
		pdxlnLeadEdge = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarWindowFrameEdge(m_pmp, true /* fLeading */, edxlfbLead));
		pdxlnTrailEdge = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarWindowFrameEdge(m_pmp, false /* fLeading */, edxlfbTrail));

		pdxlnOffset->AddRef();
		pdxlnLeadEdge->AddChild(pdxlnOffset);
		pdxlnOffset->AddRef();
		pdxlnTrailEdge->AddChild(pdxlnOffset);
	}

	// manufacture a frame for LEAD/LAG function
	return GPOS_NEW(m_pmp) CDXLWindowFrame
							(
							m_pmp,
							EdxlfsRow, // frame specification
							EdxlfesNulls, // frame exclusion strategy is set to exclude NULLs in GPDB
							pdxlnLeadEdge,
							pdxlnTrailEdge
							);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::UpdateLeadLagWinSpecPos
//
//	@doc:
// 		LEAD/LAG window functions need special frames to get executed correctly;
//		these frames are system-generated and cannot be specified in query text;
//		this function adds new entries to the list of window specs holding these
//		manufactured frames, and updates window spec references of LEAD/LAG
//		functions accordingly
//
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::UpdateLeadLagWinSpecPos
	(
	CDXLNode *pdxlnPrL, // project list holding WinRef nodes
	DrgPdxlws *pdrgpdxlws // original list of window spec
	)
	const
{
	GPOS_ASSERT(NULL != pdxlnPrL);
	GPOS_ASSERT(NULL != pdrgpdxlws);

	const ULONG ulArity = pdxlnPrL->UlArity();
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		CDXLNode *pdxlnChild = (*(*pdxlnPrL)[ul])[0];
		CDXLOperator *pdxlop = pdxlnChild->Pdxlop();
		BOOL fLead = FLeadWindowFunc(pdxlop);
		BOOL fLag = FLagWindowFunc(pdxlop);
		if (fLead || fLag)
		{
			CDXLScalarWindowRef *pdxlopWinref = CDXLScalarWindowRef::PdxlopConvert(pdxlop);
			CDXLWindowSpec *pdxlws = (*pdrgpdxlws)[pdxlopWinref->UlWinSpecPos()];
			CMDName *pmdname = NULL;
			if (NULL != pdxlws->Pmdname())
			{
				pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, pdxlws->Pmdname()->Pstr());
			}

			// find if an offset is specified
			CDXLNode *pdxlnOffset = NULL;
			if (1 < pdxlnChild->UlArity())
			{
				pdxlnOffset = (*pdxlnChild)[1];
			}

			// create LEAD/LAG frame
			CDXLWindowFrame *pdxlwf = PdxlwfLeadLag(fLead, pdxlnOffset);

			// create new window spec object
			pdxlws->PdrgulPartColList()->AddRef();
			pdxlws->PdxlnSortColList()->AddRef();
			CDXLWindowSpec *pdxlwsNew =
				GPOS_NEW(m_pmp) CDXLWindowSpec
					(
					m_pmp,
					pdxlws->PdrgulPartColList(),
					pmdname,
					pdxlws->PdxlnSortColList(),
					pdxlwf
					);
			pdrgpdxlws->Append(pdxlwsNew);

			// update win spec pos of LEAD/LAG function
			pdxlopWinref->SetWinSpecPos(pdrgpdxlws->UlLength() - 1);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::Pdrgpdxlws
//
//	@doc:
//		Translate window specs
//
//---------------------------------------------------------------------------
DrgPdxlws *
CTranslatorQueryToDXL::Pdrgpdxlws
	(
	List *plWindowClause,
	HMIUl *phmiulSortColsColId,
	CDXLNode *pdxlnScPrL
	)
{
	GPOS_ASSERT(NULL != plWindowClause);
	GPOS_ASSERT(NULL != phmiulSortColsColId);
	GPOS_ASSERT(NULL != pdxlnScPrL);

	DrgPdxlws *pdrgpdxlws = GPOS_NEW(m_pmp) DrgPdxlws(m_pmp);

	// translate window specification
	ListCell *plcWindowCl;
	ForEach (plcWindowCl, plWindowClause)
	{
		WindowClause *pwc = (WindowClause *) lfirst(plcWindowCl);
		DrgPul *pdrgppulPartCol = PdrgpulPartCol(pwc->partitionClause, phmiulSortColsColId);

		CDXLNode *pdxlnSortColList = NULL;
		CMDName *pmdname = NULL;
		CDXLWindowFrame *pdxlwf = NULL;

		if (NULL != pwc->name)
		{
			CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, pwc->name);
			pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
			GPOS_DELETE(pstrAlias);
		}

		if (0 < gpdb::UlListLength(pwc->orderClause))
		{
			// create a sorting col list
			pdxlnSortColList = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarSortColList(m_pmp));

			DrgPdxln *pdrgpdxlnSortCol = PdrgpdxlnSortCol(pwc->orderClause, phmiulSortColsColId);
			const ULONG ulSize = pdrgpdxlnSortCol->UlLength();
			for (ULONG ul = 0; ul < ulSize; ul++)
			{
				CDXLNode *pdxlnSortClause = (*pdrgpdxlnSortCol)[ul];
				pdxlnSortClause->AddRef();
				pdxlnSortColList->AddChild(pdxlnSortClause);
			}
			pdrgpdxlnSortCol->Release();
		}

		pdxlwf = m_psctranslator->Pdxlwf(pwc->frameOptions,
						 pwc->startOffset,
						 pwc->endOffset,
						 m_pmapvarcolid,
						 pdxlnScPrL,
						 &m_fHasDistributedTables);

		CDXLWindowSpec *pdxlws = GPOS_NEW(m_pmp) CDXLWindowSpec(m_pmp, pdrgppulPartCol, pmdname, pdxlnSortColList, pdxlwf);
		pdrgpdxlws->Append(pdxlws);
	}

	return pdrgpdxlws;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnWindow
//
//	@doc:
//		Translate a window operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnWindow
	(
	CDXLNode *pdxlnChild,
	List *plTargetList,
	List *plWindowClause,
	List *plSortClause,
	HMIUl *phmiulSortColsColId,
	HMIUl *phmiulOutputCols
	)
{
	if (0 == gpdb::UlListLength(plWindowClause))
	{
		return pdxlnChild;
	}

	// translate target list entries
	CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	CDXLNode *pdxlnNewChildScPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));
	ListCell *plcTE = NULL;
	ULONG ulResno = 1;

	// target entries that are result of flattening join alias and 
	// are equivalent to a defined Window specs target entry
	List *plTEOmitted = NIL; 
	List *plResno = NIL;
	
	ForEach (plcTE, plTargetList)
	{
		BOOL fInsertSortInfo = true;
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));

		// create the DXL node holding the target list entry
		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr(pte->expr, pte->resname);
		ULONG ulColId = CDXLScalarProjElem::PdxlopConvert(pdxlnPrEl->Pdxlop())->UlId();

		if (IsA(pte->expr, WindowFunc))
		{
			CTranslatorUtils::CheckAggregateWindowFn((Node*) pte->expr);
		}
		if (!pte->resjunk)
		{
			if (IsA(pte->expr, Var) || IsA(pte->expr, WindowFunc))
			{
				// add window functions and non-computed columns to the project list of the window operator
				pdxlnPrL->AddChild(pdxlnPrEl);

				StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);
			}
			else if (CTranslatorUtils::FWindowSpec(pte, plWindowClause))
			{
				// add computed column used in window specification needed in the output columns
				// to the child's project list
				pdxlnNewChildScPrL->AddChild(pdxlnPrEl);

				// construct a scalar identifier that points to the computed column and
				// add it to the project list of the window operator
				CMDName *pmdnameAlias = GPOS_NEW(m_pmp) CMDName
													(
													m_pmp,
													CDXLScalarProjElem::PdxlopConvert(pdxlnPrEl->Pdxlop())->PmdnameAlias()->Pstr()
													);
				CDXLNode *pdxlnPrElNew = GPOS_NEW(m_pmp) CDXLNode
													(
													m_pmp,
													GPOS_NEW(m_pmp) CDXLScalarProjElem(m_pmp, ulColId, pmdnameAlias)
													);
				CDXLNode *pdxlnPrElNewChild = GPOS_NEW(m_pmp) CDXLNode
															(
															m_pmp,
															GPOS_NEW(m_pmp) CDXLScalarIdent
																		(
																		m_pmp,
																		GPOS_NEW(m_pmp) CDXLColRef
																					(
																					m_pmp,
																					GPOS_NEW(m_pmp) CMDName(m_pmp, pmdnameAlias->Pstr()), ulColId
																					),
																		GPOS_NEW(m_pmp) CMDIdGPDB(gpdb::OidExprType((Node*) pte->expr))
																		)
															);
				pdxlnPrElNew->AddChild(pdxlnPrElNewChild);
				pdxlnPrL->AddChild(pdxlnPrElNew);

				StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);
			}
			else
			{
				fInsertSortInfo = false;
				plTEOmitted = gpdb::PlAppendElement(plTEOmitted, pte);
				plResno = gpdb::PlAppendInt(plResno, ulResno);

				pdxlnPrEl->Release();
			}
		}
		else if (IsA(pte->expr, WindowFunc))
		{
			// computed columns used in the order by clause
			pdxlnPrL->AddChild(pdxlnPrEl);
		}
		else if (!IsA(pte->expr, Var))
		{
			GPOS_ASSERT(CTranslatorUtils::FWindowSpec(pte, plWindowClause));
			// computed columns used in the window specification
			pdxlnNewChildScPrL->AddChild(pdxlnPrEl);
		}
		else
		{
			pdxlnPrEl->Release();
		}

		if (fInsertSortInfo)
		{
			AddSortingGroupingColumn(pte, phmiulSortColsColId, ulColId);
		}

		ulResno++;
	}

	plcTE = NULL;

	// process target entries that are a result of flattening join alias
	ListCell *plcResno = NULL;
	ForBoth (plcTE, plTEOmitted,
			plcResno, plResno)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		INT iResno = (INT) lfirst_int(plcResno);

		TargetEntry *pteWindowSpec = CTranslatorUtils::PteWindowSpec( (Node*) pte->expr, plWindowClause, plTargetList);
		if (NULL != pteWindowSpec)
		{
			const ULONG ulColId = CTranslatorUtils::UlColId( (INT) pteWindowSpec->ressortgroupref, phmiulSortColsColId);
			StoreAttnoColIdMapping(phmiulOutputCols, iResno, ulColId);
			AddSortingGroupingColumn(pte, phmiulSortColsColId, ulColId);
		}
	}
	if (NIL != plTEOmitted)
	{
		gpdb::GPDBFree(plTEOmitted);
	}

	// translate window spec
	DrgPdxlws *pdrgpdxlws = Pdrgpdxlws(plWindowClause, phmiulSortColsColId, pdxlnNewChildScPrL);

	CDXLNode *pdxlnNewChild = NULL;

	if (0 < pdxlnNewChildScPrL->UlArity())
	{
		// create a project list for the computed columns used in the window specification
		pdxlnNewChild = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp));
		pdxlnNewChild->AddChild(pdxlnNewChildScPrL);
		pdxlnNewChild->AddChild(pdxlnChild);
		pdxlnChild = pdxlnNewChild;
	}
	else
	{
		// clean up
		pdxlnNewChildScPrL->Release();
	}

	if (!CTranslatorUtils::FHasProjElem(pdxlnPrL, EdxlopScalarWindowRef))
	{
		pdxlnPrL->Release();
		pdrgpdxlws->Release();

		return pdxlnChild;
	}

	// update window spec positions of LEAD/LAG functions
	UpdateLeadLagWinSpecPos(pdxlnPrL, pdrgpdxlws);

	CDXLLogicalWindow *pdxlopWindow = GPOS_NEW(m_pmp) CDXLLogicalWindow(m_pmp, pdrgpdxlws);
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopWindow);

	pdxln->AddChild(pdxlnPrL);
	pdxln->AddChild(pdxlnChild);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpulPartCol
//
//	@doc:
//		Translate the list of partition-by column identifiers
//
//---------------------------------------------------------------------------
DrgPul *
CTranslatorQueryToDXL::PdrgpulPartCol
	(
	List *plPartCl,
	HMIUl *phmiulColColId
	)
	const
{
	DrgPul *pdrgpul = GPOS_NEW(m_pmp) DrgPul(m_pmp);

	ListCell *plcPartCl = NULL;
	ForEach (plcPartCl, plPartCl)
	{
		Node *pnodePartCl = (Node*) lfirst(plcPartCl);
		GPOS_ASSERT(NULL != pnodePartCl);

		GPOS_ASSERT(IsA(pnodePartCl, SortGroupClause));
		SortGroupClause *psortcl = (SortGroupClause *) pnodePartCl;

		// get the colid of the partition-by column
		ULONG ulColId = CTranslatorUtils::UlColId((INT) psortcl->tleSortGroupRef, phmiulColColId);

		pdrgpul->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
	}

	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpdxlnSortCol
//
//	@doc:
//		Translate the list of sorting columns
//
//---------------------------------------------------------------------------
DrgPdxln *
CTranslatorQueryToDXL::PdrgpdxlnSortCol
	(
	List *plSortCl,
	HMIUl *phmiulColColId
	)
	const
{
	DrgPdxln *pdrgpdxln = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);

	ListCell *plcSortCl = NULL;
	ForEach (plcSortCl, plSortCl)
	{
		Node *pnodeSortCl = (Node*) lfirst(plcSortCl);
		GPOS_ASSERT(NULL != pnodeSortCl);

		GPOS_ASSERT(IsA(pnodeSortCl, SortGroupClause));

		SortGroupClause *psortcl = (SortGroupClause *) pnodeSortCl;

		// get the colid of the sorting column
		const ULONG ulColId = CTranslatorUtils::UlColId((INT) psortcl->tleSortGroupRef, phmiulColColId);

		OID oid = psortcl->sortop;

		// get operator name
		CMDIdGPDB *pmdidScOp = GPOS_NEW(m_pmp) CMDIdGPDB(oid);
		const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pmdidScOp);

		const CWStringConst *pstr = pmdscop->Mdname().Pstr();
		GPOS_ASSERT(NULL != pstr);

		CDXLScalarSortCol *pdxlop = GPOS_NEW(m_pmp) CDXLScalarSortCol
												(
												m_pmp,
												ulColId,
												pmdidScOp,
												GPOS_NEW(m_pmp) CWStringConst(pstr->Wsz()),
												psortcl->nulls_first
												);

		// create the DXL node holding the sorting col
		CDXLNode *pdxlnSortCol = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

		pdrgpdxln->Append(pdxlnSortCol);
	}

	return pdrgpdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnLgLimit
//
//	@doc:
//		Translate the list of sorting columns, limit offset and limit count
//		into a CDXLLogicalGroupBy node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnLgLimit
	(
	List *plSortCl,
	Node *pnodeLimitCount,
	Node *pnodeLimitOffset,
	CDXLNode *pdxlnChild,
	HMIUl *phmiulGrpColsColId
	)
{
	if (0 == gpdb::UlListLength(plSortCl) && NULL == pnodeLimitCount && NULL == pnodeLimitOffset)
	{
		return pdxlnChild;
	}

	// do not remove limit if it is immediately under a DML (JIRA: GPSQL-2669)
	// otherwise we may increase the storage size because there are less opportunities for compression
	BOOL fTopLevelLimit = (m_fTopDMLQuery && 1 == m_ulQueryLevel) || (m_fCTASQuery && 0 == m_ulQueryLevel);
	CDXLNode *pdxlnLimit =
			GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalLimit(m_pmp, fTopLevelLimit));

	// create a sorting col list
	CDXLNode *pdxlnSortColList = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarSortColList(m_pmp));

	DrgPdxln *pdrgpdxlnSortCol = PdrgpdxlnSortCol(plSortCl, phmiulGrpColsColId);
	const ULONG ulSize = pdrgpdxlnSortCol->UlLength();
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		CDXLNode *pdxlnSortCol = (*pdrgpdxlnSortCol)[ul];
		pdxlnSortCol->AddRef();
		pdxlnSortColList->AddChild(pdxlnSortCol);
	}
	pdrgpdxlnSortCol->Release();

	// create limit count
	CDXLNode *pdxlnLimitCount = GPOS_NEW(m_pmp) CDXLNode
										(
										m_pmp,
										GPOS_NEW(m_pmp) CDXLScalarLimitCount(m_pmp)
										);

	if (NULL != pnodeLimitCount)
	{
		pdxlnLimitCount->AddChild(PdxlnScFromGPDBExpr((Expr*) pnodeLimitCount));
	}

	// create limit offset
	CDXLNode *pdxlnLimitOffset = GPOS_NEW(m_pmp) CDXLNode
										(
										m_pmp,
										GPOS_NEW(m_pmp) CDXLScalarLimitOffset(m_pmp)
										);

	if (NULL != pnodeLimitOffset)
	{
		pdxlnLimitOffset->AddChild(PdxlnScFromGPDBExpr((Expr*) pnodeLimitOffset));
	}

	pdxlnLimit->AddChild(pdxlnSortColList);
	pdxlnLimit->AddChild(pdxlnLimitCount);
	pdxlnLimit->AddChild(pdxlnLimitOffset);
	pdxlnLimit->AddChild(pdxlnChild);

	return pdxlnLimit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::AddSortingGroupingColumn
//
//	@doc:
//		Add sorting and grouping column into the hash map
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::AddSortingGroupingColumn
	(
	TargetEntry *pte,
	HMIUl *phmiulSortgrouprefColId,
	ULONG ulColId
	)
	const
{
	if (0 < pte->ressortgroupref)
	{
		INT *piKey = GPOS_NEW(m_pmp) INT(pte->ressortgroupref);
		ULONG *pulValue = GPOS_NEW(m_pmp) ULONG(ulColId);

		// insert idx-colid mapping in the hash map
#ifdef GPOS_DEBUG
		BOOL fRes =
#endif // GPOS_DEBUG
				phmiulSortgrouprefColId->FInsert(piKey, pulValue);

		GPOS_ASSERT(fRes);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnSimpleGroupBy
//
//	@doc:
//		Translate a query with grouping clause into a CDXLLogicalGroupBy node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnSimpleGroupBy
	(
	List *plTargetList,
	List *plGroupClause,
	CBitSet *pbsGroupByCols,
	BOOL fHasAggs,
	BOOL fGroupingSets,
	CDXLNode *pdxlnChild,
	HMIUl *phmiulSortgrouprefColId,
	HMIUl *phmiulChild,
	HMIUl *phmiulOutputCols
	)
{
	if (NULL == pbsGroupByCols)
	{ 
		GPOS_ASSERT(!fHasAggs);
		if (!fGroupingSets)
		{
			// no group by needed and not part of a grouping sets query: 
			// propagate child columns to output columns
			HMIUlIter mi(phmiulChild);
			while (mi.FAdvance())
			{
	#ifdef GPOS_DEBUG
				BOOL fResult =
	#endif // GPOS_DEBUG
				phmiulOutputCols->FInsert(GPOS_NEW(m_pmp) INT(*(mi.Pk())), GPOS_NEW(m_pmp) ULONG(*(mi.Pt())));
				GPOS_ASSERT(fResult);
			}
		}
		// else:
		// in queries with grouping sets we may generate a branch corresponding to GB grouping sets ();
		// in that case do not propagate the child columns to the output hash map, as later
		// processing may introduce NULLs for those

		return pdxlnChild;
	}

	List *plDQA = NIL;
	// construct the project list of the group-by operator
	CDXLNode *pdxlnPrLGrpBy = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	ListCell *plcTE = NULL;
	ULONG ulDQAs = 0;
	ForEach (plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));
		GPOS_ASSERT(0 < pte->resno);
		ULONG ulResNo = pte->resno;

		TargetEntry *pteEquivalent = CTranslatorUtils::PteGroupingColumn( (Node *) pte->expr, plGroupClause, plTargetList);

		BOOL fGroupingCol = pbsGroupByCols->FBit(pte->ressortgroupref) || (NULL != pteEquivalent && pbsGroupByCols->FBit(pteEquivalent->ressortgroupref));
		ULONG ulColId = 0;

		if (fGroupingCol)
		{
			// find colid for grouping column
			ulColId = CTranslatorUtils::UlColId(ulResNo, phmiulChild);
		}
		else if (IsA(pte->expr, Aggref) || IsA(pte->expr, PercentileExpr))
		{
			if (IsA(pte->expr, Aggref) && ((Aggref *) pte->expr)->aggdistinct && !FDuplicateDqaArg(plDQA, (Aggref *) pte->expr))
			{
				plDQA = gpdb::PlAppendElement(plDQA, gpdb::PvCopyObject(pte->expr));
				ulDQAs++;
			}

			// create a project element for aggregate
			CDXLNode *pdxlnPrEl = PdxlnPrEFromGPDBExpr(pte->expr, pte->resname);
			pdxlnPrLGrpBy->AddChild(pdxlnPrEl);
			ulColId = CDXLScalarProjElem::PdxlopConvert(pdxlnPrEl->Pdxlop())->UlId();
			AddSortingGroupingColumn(pte, phmiulSortgrouprefColId, ulColId);
		}

		if (fGroupingCol || IsA(pte->expr, Aggref) || IsA(pte->expr, PercentileExpr))
		{
			// add to the list of output columns
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, ulColId);
		}
		else if (0 == pbsGroupByCols->CElements() && !fGroupingSets && !fHasAggs)
		{
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, ulColId);
		}
	}

	if (1 < ulDQAs && !optimizer_enable_multiple_distinct_aggs)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple Distinct Qualified Aggregates are disabled in the optimizer"));
	}

	// initialize the array of grouping columns
	DrgPul *pdrgpul = CTranslatorUtils::PdrgpulGroupingCols(m_pmp, pbsGroupByCols, phmiulSortgrouprefColId);

	// clean up
	if (NIL != plDQA)
	{
		gpdb::FreeList(plDQA);
	}

	return GPOS_NEW(m_pmp) CDXLNode
						(
						m_pmp,
						GPOS_NEW(m_pmp) CDXLLogicalGroupBy(m_pmp, pdrgpul),
						pdxlnPrLGrpBy,
						pdxlnChild
						);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::FDuplicateDqaArg
//
//	@doc:
//		Check if the argument of a DQA has already being used by another DQA
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FDuplicateDqaArg
	(
	List *plDQA,
	Aggref *paggref
	)
{
	GPOS_ASSERT(NULL != paggref);

	if (NIL == plDQA || 0 == gpdb::UlListLength(plDQA))
	{
		return false;
	}

	ListCell *plc = NULL;
	ForEach (plc, plDQA)
	{
		Node *pnode = (Node *) lfirst(plc);
		GPOS_ASSERT(IsA(pnode, Aggref));

		if (gpdb::FEqual(paggref->args, ((Aggref *) pnode)->args))
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnGroupingSets
//
//	@doc:
//		Translate a query with grouping sets
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnGroupingSets
	(
	FromExpr *pfromexpr,
	List *plTargetList,
	List *plGroupClause,
	BOOL fHasAggs,
	HMIUl *phmiulSortgrouprefColId,
	HMIUl *phmiulOutputCols
	)
{
	const ULONG ulCols = gpdb::UlListLength(plTargetList) + 1;

	if (NULL == plGroupClause)
	{
		HMIUl *phmiulChild = GPOS_NEW(m_pmp) HMIUl(m_pmp);

		CDXLNode *pdxlnSPJ = PdxlnSPJ(plTargetList, pfromexpr, phmiulSortgrouprefColId, phmiulChild, plGroupClause);

		CBitSet *pbs = NULL;
		if (fHasAggs)
		{ 
			pbs = GPOS_NEW(m_pmp) CBitSet(m_pmp);
		}
		
		// in case of aggregates, construct a group by operator
		CDXLNode *pdxlnResult = PdxlnSimpleGroupBy
								(
								plTargetList,
								plGroupClause,
								pbs,
								fHasAggs,
								false, // fGroupingSets
								pdxlnSPJ,
								phmiulSortgrouprefColId,
								phmiulChild,
								phmiulOutputCols
								);

		// cleanup
		phmiulChild->Release();
		CRefCount::SafeRelease(pbs);
		return pdxlnResult;
	}

	// grouping functions refer to grouping col positions, so construct a map pos->grouping column
	// while processing the grouping clause
	HMUlUl *phmululGrpColPos = GPOS_NEW(m_pmp) HMUlUl(m_pmp);
	CBitSet *pbsUniqueueGrpCols = GPOS_NEW(m_pmp) CBitSet(m_pmp, ulCols);
	DrgPbs *pdrgpbs = CTranslatorUtils::PdrgpbsGroupBy(m_pmp, plGroupClause, ulCols, phmululGrpColPos, pbsUniqueueGrpCols);

	const ULONG ulGroupingSets = pdrgpbs->UlLength();

	if (1 == ulGroupingSets)
	{
		// simple group by
		HMIUl *phmiulChild = GPOS_NEW(m_pmp) HMIUl(m_pmp);
		CDXLNode *pdxlnSPJ = PdxlnSPJ(plTargetList, pfromexpr, phmiulSortgrouprefColId, phmiulChild, plGroupClause);

		// translate the groupby clauses into a logical group by operator
		CBitSet *pbs = (*pdrgpbs)[0];


		CDXLNode *pdxlnGroupBy = PdxlnSimpleGroupBy
								(
								plTargetList,
								plGroupClause,
								pbs,
								fHasAggs,
								false, // fGroupingSets
								pdxlnSPJ,
								phmiulSortgrouprefColId,
								phmiulChild,
								phmiulOutputCols
								);
		
		CDXLNode *pdxlnResult = PdxlnProjectGroupingFuncs
									(
									plTargetList,
									pdxlnGroupBy,
									pbs,
									phmiulOutputCols,
									phmululGrpColPos,
									phmiulSortgrouprefColId
									);

		phmiulChild->Release();
		pdrgpbs->Release();
		pbsUniqueueGrpCols->Release();
		phmululGrpColPos->Release();
		
		return pdxlnResult;
	}
	
	CDXLNode *pdxlnResult = PdxlnUnionAllForGroupingSets
			(
			pfromexpr,
			plTargetList,
			plGroupClause,
			fHasAggs,
			pdrgpbs,
			phmiulSortgrouprefColId,
			phmiulOutputCols,
			phmululGrpColPos
			);

	pbsUniqueueGrpCols->Release();
	phmululGrpColPos->Release();
	
	return pdxlnResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnUnionAllForGroupingSets
//
//	@doc:
//		Construct a union all for the given grouping sets
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnUnionAllForGroupingSets
	(
	FromExpr *pfromexpr,
	List *plTargetList,
	List *plGroupClause,
	BOOL fHasAggs,
	DrgPbs *pdrgpbs,
	HMIUl *phmiulSortgrouprefColId,
	HMIUl *phmiulOutputCols,
	HMUlUl *phmululGrpColPos		// mapping pos->unique grouping columns for grouping func arguments
	)
{
	GPOS_ASSERT(NULL != pdrgpbs);
	GPOS_ASSERT(1 < pdrgpbs->UlLength());

	const ULONG ulGroupingSets = pdrgpbs->UlLength();
	CDXLNode *pdxlnUnionAll = NULL;
	DrgPul *pdrgpulColIdsInner = NULL;

	const ULONG ulCTEId = m_pidgtorCTE->UlNextId();
	
	// construct a CTE producer on top of the SPJ query
	HMIUl *phmiulSPJ = GPOS_NEW(m_pmp) HMIUl(m_pmp);
	HMIUl *phmiulSortgrouprefColIdProducer = GPOS_NEW(m_pmp) HMIUl(m_pmp);
	CDXLNode *pdxlnSPJ = PdxlnSPJForGroupingSets(plTargetList, pfromexpr, phmiulSortgrouprefColIdProducer, phmiulSPJ, plGroupClause);

	// construct output colids
	DrgPul *pdrgpulCTEProducer = PdrgpulExtractColIds(m_pmp, phmiulSPJ);

	GPOS_ASSERT (NULL != m_pdrgpdxlnCTE);
	
	CDXLLogicalCTEProducer *pdxlopCTEProducer = GPOS_NEW(m_pmp) CDXLLogicalCTEProducer(m_pmp, ulCTEId, pdrgpulCTEProducer);
	CDXLNode *pdxlnCTEProducer = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopCTEProducer, pdxlnSPJ);
	m_pdrgpdxlnCTE->Append(pdxlnCTEProducer);
	
	CMappingVarColId *pmapvarcolidOriginal = m_pmapvarcolid->PmapvarcolidCopy(m_pmp);
	
	for (ULONG ul = 0; ul < ulGroupingSets; ul++)
	{
		CBitSet *pbsGroupingSet = (*pdrgpbs)[ul];

		// remap columns
		DrgPul *pdrgpulCTEConsumer = PdrgpulGenerateColIds(m_pmp, pdrgpulCTEProducer->UlLength());
		
		// reset col mapping with new consumer columns
		GPOS_DELETE(m_pmapvarcolid);
		m_pmapvarcolid = pmapvarcolidOriginal->PmapvarcolidRemap(m_pmp, pdrgpulCTEProducer, pdrgpulCTEConsumer);
		
		HMIUl *phmiulSPJConsumer = PhmiulRemapColIds(m_pmp, phmiulSPJ, pdrgpulCTEProducer, pdrgpulCTEConsumer);
		HMIUl *phmiulSortgrouprefColIdConsumer = PhmiulRemapColIds(m_pmp, phmiulSortgrouprefColIdProducer, pdrgpulCTEProducer, pdrgpulCTEConsumer);

		// construct a CTE consumer
		CDXLNode *pdxlnCTEConsumer = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalCTEConsumer(m_pmp, ulCTEId, pdrgpulCTEConsumer));

		HMIUl *phmiulGroupBy = GPOS_NEW(m_pmp) HMIUl(m_pmp);
		CDXLNode *pdxlnGroupBy = PdxlnSimpleGroupBy
					(
					plTargetList,
					plGroupClause,
					pbsGroupingSet,
					fHasAggs,
					true, // fGroupingSets
					pdxlnCTEConsumer,
					phmiulSortgrouprefColIdConsumer,
					phmiulSPJConsumer,
					phmiulGroupBy
					);

		// add a project list for the NULL values
		CDXLNode *pdxlnProject = PdxlnProjectNullsForGroupingSets(plTargetList, pdxlnGroupBy, pbsGroupingSet, phmiulSortgrouprefColIdConsumer, phmiulGroupBy, phmululGrpColPos);

		DrgPul *pdrgpulColIdsOuter = CTranslatorUtils::PdrgpulColIds(m_pmp, plTargetList, phmiulGroupBy);
		if (NULL != pdxlnUnionAll)
		{
			GPOS_ASSERT(NULL != pdrgpulColIdsInner);
			DrgPdxlcd *pdrgpdxlcd = CTranslatorUtils::Pdrgpdxlcd(m_pmp, plTargetList, pdrgpulColIdsOuter, true /* fKeepResjunked */);

			pdrgpulColIdsOuter->AddRef();

			DrgPdrgPul *pdrgpdrgulInputColIds = GPOS_NEW(m_pmp) DrgPdrgPul(m_pmp);
			pdrgpdrgulInputColIds->Append(pdrgpulColIdsOuter);
			pdrgpdrgulInputColIds->Append(pdrgpulColIdsInner);

			CDXLLogicalSetOp *pdxlopSetop = GPOS_NEW(m_pmp) CDXLLogicalSetOp(m_pmp, EdxlsetopUnionAll, pdrgpdxlcd, pdrgpdrgulInputColIds, false);
			pdxlnUnionAll = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopSetop, pdxlnProject, pdxlnUnionAll);
		}
		else
		{
			pdxlnUnionAll = pdxlnProject;
		}

		pdrgpulColIdsInner = pdrgpulColIdsOuter;
		
		if (ul == ulGroupingSets - 1)
		{
			// add the sortgroup columns to output map of the last column
			ULONG ulTargetEntryPos = 0;
			ListCell *plcTE = NULL;
			ForEach (plcTE, plTargetList)
			{
				TargetEntry *pte = (TargetEntry *) lfirst(plcTE);

				INT iSortGrpRef = INT (pte->ressortgroupref);
				if (0 < iSortGrpRef && NULL != phmiulSortgrouprefColIdConsumer->PtLookup(&iSortGrpRef))
				{
					// add the mapping information for sorting columns
					AddSortingGroupingColumn(pte, phmiulSortgrouprefColId, *(*pdrgpulColIdsInner)[ulTargetEntryPos]);
				}

				ulTargetEntryPos++;
			}
		}

		// cleanup
		phmiulGroupBy->Release();
		phmiulSPJConsumer->Release();
		phmiulSortgrouprefColIdConsumer->Release();
	}

	// cleanup
	phmiulSPJ->Release();
	phmiulSortgrouprefColIdProducer->Release();
	GPOS_DELETE(pmapvarcolidOriginal);
	pdrgpulColIdsInner->Release();

	// compute output columns
	CDXLLogicalSetOp *pdxlopUnion = CDXLLogicalSetOp::PdxlopConvert(pdxlnUnionAll->Pdxlop());

	ListCell *plcTE = NULL;
	ULONG ulOutputColIndex = 0;
	ForEach (plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));
		GPOS_ASSERT(0 < pte->resno);
		ULONG ulResNo = pte->resno;

		// note that all target list entries are kept in union all's output column
		// this is achieved by the fKeepResjunked flag in CTranslatorUtils::Pdrgpdxlcd
		const CDXLColDescr *pdxlcd = pdxlopUnion->Pdxlcd(ulOutputColIndex);
		const ULONG ulColId = pdxlcd->UlID();
		ulOutputColIndex++;

		if (!pte->resjunk)
		{
			// add non-resjunk columns to the hash map that maintains the output columns
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, ulColId);
		}
	}

	// cleanup
	pdrgpbs->Release();

	// construct a CTE anchor operator on top of the union all
	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalCTEAnchor(m_pmp, ulCTEId), pdxlnUnionAll);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnConstTableGet
//
//	@doc:
//		Create a dummy constant table get (CTG) with a boolean true value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnConstTableGet() const
{

	// construct the schema of the const table
	DrgPdxlcd *pdrgpdxlcd = GPOS_NEW(m_pmp) DrgPdxlcd(m_pmp);

	const CMDTypeBoolGPDB *pmdtypeBool = dynamic_cast<const CMDTypeBoolGPDB *>(m_pmda->PtMDType<IMDTypeBool>(m_sysid));
	const CMDIdGPDB *pmdid = CMDIdGPDB::PmdidConvert(pmdtypeBool->Pmdid());

	// empty column name
	CWStringConst strUnnamedCol(GPOS_WSZ_LIT(""));
	CMDName *pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol);
	CDXLColDescr *pdxlcd = GPOS_NEW(m_pmp) CDXLColDescr
										(
										m_pmp,
										pmdname,
										m_pidgtorCol->UlNextId(),
										1 /* iAttno */,
										GPOS_NEW(m_pmp) CMDIdGPDB(pmdid->OidObjectId()),
										false /* fDropped */
										);
	pdrgpdxlcd->Append(pdxlcd);

	// create the array of datum arrays
	DrgPdrgPdxldatum *pdrgpdrgpdxldatum = GPOS_NEW(m_pmp) DrgPdrgPdxldatum(m_pmp);
	
	// create a datum array
	DrgPdxldatum *pdrgpdxldatum = GPOS_NEW(m_pmp) DrgPdxldatum(m_pmp);

	Const *pconst = (Const*) gpdb::PnodeMakeBoolConst(true /*value*/, false /*isnull*/);
	CDXLDatum *pdxldatum = m_psctranslator->Pdxldatum(pconst);
	gpdb::GPDBFree(pconst);

	pdrgpdxldatum->Append(pdxldatum);
	pdrgpdrgpdxldatum->Append(pdrgpdxldatum);

	CDXLLogicalConstTable *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalConstTable(m_pmp, pdrgpdxlcd, pdrgpdrgpdxldatum);

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromSetOp
//
//	@doc:
//		Translate a set operation
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromSetOp
	(
	Node *pnodeSetOp,
	List *plTargetList,
	HMIUl *phmiulOutputCols
	)
{
	GPOS_ASSERT(IsA(pnodeSetOp, SetOperationStmt));
	SetOperationStmt *psetopstmt = (SetOperationStmt*) pnodeSetOp;
	GPOS_ASSERT(SETOP_NONE != psetopstmt->op);

	EdxlSetOpType edxlsetop = CTranslatorUtils::Edxlsetop(psetopstmt->op, psetopstmt->all);

	// translate the left and right child
	DrgPul *pdrgpulLeft = GPOS_NEW(m_pmp) DrgPul(m_pmp);
	DrgPul *pdrgpulRight = GPOS_NEW(m_pmp) DrgPul(m_pmp);
	DrgPmdid *pdrgpmdidLeft = GPOS_NEW(m_pmp) DrgPmdid(m_pmp);
	DrgPmdid *pdrgpmdidRight = GPOS_NEW(m_pmp) DrgPmdid(m_pmp);

	CDXLNode *pdxlnLeftChild = PdxlnSetOpChild(psetopstmt->larg, pdrgpulLeft, pdrgpmdidLeft, plTargetList);
	CDXLNode *pdxlnRightChild = PdxlnSetOpChild(psetopstmt->rarg, pdrgpulRight, pdrgpmdidRight, plTargetList);

	// mark outer references in input columns from left child
	ULONG *pulColId = GPOS_NEW_ARRAY(m_pmp, ULONG, pdrgpulLeft->UlLength());
	BOOL *pfOuterRef = GPOS_NEW_ARRAY(m_pmp, BOOL, pdrgpulLeft->UlLength());
	const ULONG ulSize = pdrgpulLeft->UlLength();
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		pulColId[ul] =  *(*pdrgpulLeft)[ul];
		pfOuterRef[ul] = true;
	}
	CTranslatorUtils::MarkOuterRefs(pulColId, pfOuterRef, ulSize, pdxlnLeftChild);

	DrgPdrgPul *pdrgpdrgulInputColIds = GPOS_NEW(m_pmp) DrgPdrgPul(m_pmp);
	pdrgpdrgulInputColIds->Append(pdrgpulLeft);
	pdrgpdrgulInputColIds->Append(pdrgpulRight);
	
	DrgPul *pdrgpulOutput =  CTranslatorUtils::PdrgpulGenerateColIds
												(
												m_pmp,
												plTargetList,
												pdrgpmdidLeft,
												pdrgpulLeft,
												pfOuterRef,
												m_pidgtorCol
												);
 	GPOS_ASSERT(pdrgpulOutput->UlLength() == pdrgpulLeft->UlLength());

 	GPOS_DELETE_ARRAY(pulColId);
 	GPOS_DELETE_ARRAY(pfOuterRef);

	BOOL fCastAcrossInput = FCast(plTargetList, pdrgpmdidLeft) || FCast(plTargetList, pdrgpmdidRight);
	
	DrgPdxln *pdrgpdxlnChildren  = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);
	pdrgpdxlnChildren->Append(pdxlnLeftChild);
	pdrgpdxlnChildren->Append(pdxlnRightChild);

	CDXLNode *pdxln = PdxlnSetOp
						(
						edxlsetop,
						plTargetList,
						pdrgpulOutput,
						pdrgpdrgulInputColIds,
						pdrgpdxlnChildren,
						fCastAcrossInput,
						false /* fKeepResjunked */
						);

	CDXLLogicalSetOp *pdxlop = CDXLLogicalSetOp::PdxlopConvert(pdxln->Pdxlop());
	const DrgPdxlcd *pdrgpdxlcd = pdxlop->Pdrgpdxlcd();

	ULONG ulOutputColIndex = 0;
	ListCell *plcTE = NULL;
	ForEach (plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));
		GPOS_ASSERT(0 < pte->resno);
		ULONG ulResNo = pte->resno;

		if (!pte->resjunk)
		{
			const CDXLColDescr *pdxlcdNew = (*pdrgpdxlcd)[ulOutputColIndex];
			ULONG ulColId = pdxlcdNew->UlID();
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, ulColId);
			ulOutputColIndex++;
		}
	}

	// clean up
	pdrgpulOutput->Release();
	pdrgpmdidLeft->Release();
	pdrgpmdidRight->Release();

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlSetOp
//
//	@doc:
//		Create a set op after adding dummy cast on input columns where needed
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnSetOp
	(
	EdxlSetOpType edxlsetop,
	List *plTargetListOutput,
	DrgPul *pdrgpulOutput,
	DrgPdrgPul *pdrgpdrgulInputColIds,
	DrgPdxln *pdrgpdxlnChildren,
	BOOL fCastAcrossInput,
	BOOL fKeepResjunked
	)
	const
{
	GPOS_ASSERT(NULL != plTargetListOutput);
	GPOS_ASSERT(NULL != pdrgpulOutput);
	GPOS_ASSERT(NULL != pdrgpdrgulInputColIds);
	GPOS_ASSERT(NULL != pdrgpdxlnChildren);
	GPOS_ASSERT(1 < pdrgpdrgulInputColIds->UlLength());
	GPOS_ASSERT(1 < pdrgpdxlnChildren->UlLength());

	// positions of output columns in the target list
	DrgPul *pdrgpulTLPos = CTranslatorUtils::PdrgpulPosInTargetList(m_pmp, plTargetListOutput, fKeepResjunked);

	const ULONG ulCols = pdrgpulOutput->UlLength();
	DrgPul *pdrgpulInputFirstChild = (*pdrgpdrgulInputColIds)[0];
	GPOS_ASSERT(ulCols == pdrgpulInputFirstChild->UlLength());
	GPOS_ASSERT(ulCols == pdrgpulOutput->UlLength());

	CBitSet *pbs = GPOS_NEW(m_pmp) CBitSet(m_pmp);

	// project list to maintain the casting of the duplicate input columns
	CDXLNode *pdxlnNewChildScPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	DrgPul *pdrgpulInputFirstChildNew = GPOS_NEW(m_pmp) DrgPul (m_pmp);
	DrgPdxlcd *pdrgpdxlcdOutput = GPOS_NEW(m_pmp) DrgPdxlcd(m_pmp);
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		ULONG ulColIdOutput = *(*pdrgpulOutput)[ul];
		ULONG ulColIdInput = *(*pdrgpulInputFirstChild)[ul];

		BOOL fColExists = pbs->FBit(ulColIdInput);
		BOOL fCastedCol = (ulColIdOutput != ulColIdInput);

		ULONG ulTLPos = *(*pdrgpulTLPos)[ul];
		TargetEntry *pte = (TargetEntry*) gpdb::PvListNth(plTargetListOutput, ulTLPos);
		GPOS_ASSERT(NULL != pte);

		CDXLColDescr *pdxlcdOutput = NULL;
		if (!fColExists)
		{
			pbs->FExchangeSet(ulColIdInput);
			pdrgpulInputFirstChildNew->Append(GPOS_NEW(m_pmp) ULONG(ulColIdInput));

			pdxlcdOutput = CTranslatorUtils::Pdxlcd(m_pmp, pte, ulColIdOutput, ul + 1);
		}
		else
		{
			// we add a dummy-cast to distinguish between the output columns of the union
			ULONG ulColIdNew = m_pidgtorCol->UlNextId();
			pdrgpulInputFirstChildNew->Append(GPOS_NEW(m_pmp) ULONG(ulColIdNew));

			ULONG ulColIdUnionOutput = ulColIdNew;
			if (fCastedCol)
			{
				// create new output column id since current colid denotes its duplicate
				ulColIdUnionOutput = m_pidgtorCol->UlNextId();
			}

			pdxlcdOutput = CTranslatorUtils::Pdxlcd(m_pmp, pte, ulColIdUnionOutput, ul + 1);
			CDXLNode *pdxlnPrEl = CTranslatorUtils::PdxlnDummyPrElem(m_pmp, ulColIdInput, ulColIdNew, pdxlcdOutput);

			pdxlnNewChildScPrL->AddChild(pdxlnPrEl);
		}

		pdrgpdxlcdOutput->Append(pdxlcdOutput);
	}

	pdrgpdrgulInputColIds->Replace(0, pdrgpulInputFirstChildNew);

	if (0 < pdxlnNewChildScPrL->UlArity())
	{
		// create a project node for the dummy casted columns
		CDXLNode *pdxlnFirstChild = (*pdrgpdxlnChildren)[0];
		pdxlnFirstChild->AddRef();
		CDXLNode *pdxlnNewChild = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp));
		pdxlnNewChild->AddChild(pdxlnNewChildScPrL);
		pdxlnNewChild->AddChild(pdxlnFirstChild);

		pdrgpdxlnChildren->Replace(0, pdxlnNewChild);
	}
	else
	{
		pdxlnNewChildScPrL->Release();
	}

	CDXLLogicalSetOp *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalSetOp
											(
											m_pmp,
											edxlsetop,
											pdrgpdxlcdOutput,
											pdrgpdrgulInputColIds,
											fCastAcrossInput
											);
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop, pdrgpdxlnChildren);

	pbs->Release();
	pdrgpulTLPos->Release();

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::FCast
//
//	@doc:
//		Check if the set operation need to cast any of its input columns
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FCast
	(
	List *plTargetList,
	DrgPmdid *pdrgpmdid
	)
	const
{
	GPOS_ASSERT(NULL != pdrgpmdid);
	GPOS_ASSERT(NULL != plTargetList);
	GPOS_ASSERT(pdrgpmdid->UlLength() <= gpdb::UlListLength(plTargetList)); // there may be resjunked columns

	ULONG ulColPos = 0;
	ListCell *plcTE = NULL;
	ForEach (plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		OID oidExprType = gpdb::OidExprType((Node*) pte->expr);
		if (!pte->resjunk)
		{
			IMDId *pmdid = (*pdrgpmdid)[ulColPos];
			if (CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId() != oidExprType)
			{
				return true;
			}
			ulColPos++;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnSetOpChild
//
//	@doc:
//		Translate the child of a set operation
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnSetOpChild
	(
	Node *pnodeChild,
	DrgPul *pdrgpul,
	DrgPmdid *pdrgpmdid,
	List *plTargetList
	)
{
	GPOS_ASSERT(NULL != pdrgpul);
	GPOS_ASSERT(NULL != pdrgpmdid);

	if (IsA(pnodeChild, RangeTblRef))
	{
		RangeTblRef *prtref = (RangeTblRef*) pnodeChild;
		const ULONG ulRTIndex = prtref->rtindex;
		const RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pquery->rtable, ulRTIndex - 1);

		if (RTE_SUBQUERY == prte->rtekind)
		{
			Query *pqueryDerTbl = CTranslatorUtils::PqueryFixUnknownTypeConstant(prte->subquery, plTargetList);
			GPOS_ASSERT(NULL != pqueryDerTbl);

			CMappingVarColId *pmapvarcolid = m_pmapvarcolid->PmapvarcolidCopy(m_pmp);
			CTranslatorQueryToDXL trquerytodxl
					(
					m_pmp,
					m_pmda,
					m_pidgtorCol,
					m_pidgtorCTE,
					pmapvarcolid,
					pqueryDerTbl,
					m_ulQueryLevel + 1,
					FDMLQuery(),
					m_phmulCTEEntries
					);

			// translate query representing the derived table to its DXL representation
			CDXLNode *pdxln = trquerytodxl.PdxlnFromQueryInternal();
			GPOS_ASSERT(NULL != pdxln);

			DrgPdxln *pdrgpdxlnCTE = trquerytodxl.PdrgpdxlnCTE();
			CUtils::AddRefAppend(m_pdrgpdxlnCTE, pdrgpdxlnCTE);
			m_fHasDistributedTables = m_fHasDistributedTables || trquerytodxl.FHasDistributedTables();

			// get the output columns of the derived table
			DrgPdxln *pdrgpdxln = trquerytodxl.PdrgpdxlnQueryOutput();
			GPOS_ASSERT(pdrgpdxln != NULL);
			const ULONG ulLen = pdrgpdxln->UlLength();
			for (ULONG ul = 0; ul < ulLen; ul++)
			{
				CDXLNode *pdxlnCurr = (*pdrgpdxln)[ul];
				CDXLScalarIdent *pdxlnIdent = CDXLScalarIdent::PdxlopConvert(pdxlnCurr->Pdxlop());
				ULONG *pulColId = GPOS_NEW(m_pmp) ULONG(pdxlnIdent->Pdxlcr()->UlID());
				pdrgpul->Append(pulColId);

				IMDId *pmdidCol = pdxlnIdent->PmdidType();
				GPOS_ASSERT(NULL != pmdidCol);
				pmdidCol->AddRef();
				pdrgpmdid->Append(pmdidCol);
			}

			return pdxln;
		}
	}
	else if (IsA(pnodeChild, SetOperationStmt))
	{
		HMIUl *phmiulOutputCols = GPOS_NEW(m_pmp) HMIUl(m_pmp);
		CDXLNode *pdxln = PdxlnFromSetOp(pnodeChild, plTargetList, phmiulOutputCols);

		// cleanup
		phmiulOutputCols->Release();

		const DrgPdxlcd *pdrgpdxlcd = CDXLLogicalSetOp::PdxlopConvert(pdxln->Pdxlop())->Pdrgpdxlcd();
		GPOS_ASSERT(NULL != pdrgpdxlcd);
		const ULONG ulLen = pdrgpdxlcd->UlLength();
		for (ULONG ul = 0; ul < ulLen; ul++)
		{
			const CDXLColDescr *pdxlcd = (*pdrgpdxlcd)[ul];
			ULONG *pulColId = GPOS_NEW(m_pmp) ULONG(pdxlcd->UlID());
			pdrgpul->Append(pulColId);

			IMDId *pmdidCol = pdxlcd->PmdidType();
			GPOS_ASSERT(NULL != pmdidCol);
			pmdidCol->AddRef();
			pdrgpmdid->Append(pmdidCol);
		}

		return pdxln;
	}

	CHAR *sz = (CHAR*) gpdb::SzNodeToString(const_cast<Node*>(pnodeChild));
	CWStringDynamic *pstr = CDXLUtils::PstrFromSz(m_pmp, sz);

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, pstr->Wsz());
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromGPDBFromExpr
//
//	@doc:
//		Translate the FromExpr on a GPDB query into either a CDXLLogicalJoin
//		or a CDXLLogicalGet
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromGPDBFromExpr
	(
	FromExpr *pfromexpr
	)
{
	CDXLNode *pdxln = NULL;

	if (0 == gpdb::UlListLength(pfromexpr->fromlist))
	{
		pdxln = PdxlnConstTableGet();
	}
	else
	{
		if (1 == gpdb::UlListLength(pfromexpr->fromlist))
		{
			Node *pnode = (Node*) gpdb::PvListNth(pfromexpr->fromlist, 0);
			GPOS_ASSERT(NULL != pnode);
			pdxln = PdxlnFromGPDBFromClauseEntry(pnode);
		}
		else
		{
			// In DXL, we represent an n-ary join (where n>2) by an inner join with condition true.
			// The join conditions represented in the FromExpr->quals is translated
			// into a CDXLLogicalSelect on top of the CDXLLogicalJoin

			pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalJoin(m_pmp, EdxljtInner));

			ListCell *plc = NULL;
			ForEach (plc, pfromexpr->fromlist)
			{
				Node *pnode = (Node*) lfirst(plc);
				CDXLNode *pdxlnChild = PdxlnFromGPDBFromClauseEntry(pnode);
				pdxln->AddChild(pdxlnChild);
			}
		}
	}

	// translate the quals
	Node *pnodeQuals = pfromexpr->quals;
	CDXLNode *pdxlnCond = NULL;
	if (NULL != pnodeQuals)
	{
		pdxlnCond = PdxlnScFromGPDBExpr( (Expr*) pnodeQuals);
	}

	if (1 >= gpdb::UlListLength(pfromexpr->fromlist))
	{
		if (NULL != pdxlnCond)
		{
			CDXLNode *pdxlnSelect = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalSelect(m_pmp));
			pdxlnSelect->AddChild(pdxlnCond);
			pdxlnSelect->AddChild(pdxln);

			pdxln = pdxlnSelect;
		}
	}
	else //n-ary joins
	{
		if (NULL == pdxlnCond)
		{
			// A cross join (the scalar condition is true)
			pdxlnCond = PdxlnScConstValueTrue();
		}

		pdxln->AddChild(pdxlnCond);
	}

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromGPDBFromClauseEntry
//
//	@doc:
//		Returns a CDXLNode representing a from clause entry which can either be
//		(1) a fromlist entry in the FromExpr or (2) left/right child of a JoinExpr
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromGPDBFromClauseEntry
	(
	Node *pnode
	)
{
	GPOS_ASSERT(NULL != pnode);

	if (IsA(pnode, RangeTblRef))
	{
		RangeTblRef *prtref = (RangeTblRef *) pnode;
		ULONG ulRTIndex = prtref->rtindex ;
		const RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pquery->rtable, ulRTIndex - 1);
		GPOS_ASSERT(NULL != prte);

		if (prte->forceDistRandom)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("gp_dist_random"));
		}

		static const SRTETranslator rgTranslators[] =
		{
			{RTE_RELATION, &CTranslatorQueryToDXL::PdxlnFromRelation},
			{RTE_VALUES, &CTranslatorQueryToDXL::PdxlnFromValues},
			{RTE_CTE, &CTranslatorQueryToDXL::PdxlnFromCTE},
			{RTE_SUBQUERY, &CTranslatorQueryToDXL::PdxlnFromDerivedTable},
			{RTE_FUNCTION, &CTranslatorQueryToDXL::PdxlnFromTVF},
		};
		
		const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);
		
		// find translator for the rtekind
		PfPdxlnLogical pf = NULL;
		for (ULONG ul = 0; ul < ulTranslators; ul++)
		{
			SRTETranslator elem = rgTranslators[ul];
			if (prte->rtekind == elem.m_rtekind)
			{
				pf = elem.pf;
				break;
			}
		}
		
		if (NULL == pf)
		{
			UnsupportedRTEKind(prte->rtekind);

			return NULL;
		}
		
		return (this->*pf)(prte, ulRTIndex, m_ulQueryLevel);
	}

	if (IsA(pnode, JoinExpr))
	{
		return PdxlnLgJoinFromGPDBJoinExpr((JoinExpr*) pnode);
	}

	CHAR *sz = (CHAR*) gpdb::SzNodeToString(const_cast<Node*>(pnode));
	CWStringDynamic *pstr = CDXLUtils::PstrFromSz(m_pmp, sz);

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, pstr->Wsz());

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::UnsupportedRTEKind
//
//	@doc:
//		Raise exception for unsupported RangeTblEntries of a particular kind
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::UnsupportedRTEKind
	(
    RTEKind rtekind
	)
	const
{
	GPOS_ASSERT(!(RTE_RELATION == rtekind || RTE_CTE == rtekind
				|| RTE_FUNCTION == rtekind || RTE_SUBQUERY == rtekind
				|| RTE_VALUES == rtekind));

	static const SRTENameElem rgStrMap[] =
		{
		{RTE_JOIN, GPOS_WSZ_LIT("RangeTableEntry of type Join")},
		{RTE_SPECIAL, GPOS_WSZ_LIT("RangeTableEntry of type Special")},
		{RTE_VOID, GPOS_WSZ_LIT("RangeTableEntry of type Void")},
		{RTE_TABLEFUNCTION, GPOS_WSZ_LIT("RangeTableEntry of type Table Function")}
		};

	const ULONG ulLen = GPOS_ARRAY_SIZE(rgStrMap);
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		SRTENameElem mapelem = rgStrMap[ul];

		if (mapelem.m_rtekind == rtekind)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, mapelem.m_wsz);
		}
	}

	GPOS_ASSERT(!"Unrecognized RTE kind");
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromRelation
//
//	@doc:
//		Returns a CDXLNode representing a from relation range table entry
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromRelation
	(
	const RangeTblEntry *prte,
	ULONG ulRTIndex,
	ULONG //ulCurrQueryLevel 
	)
{
	// construct table descriptor for the scan node from the range table entry
	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);

	CDXLLogicalGet *pdxlop = NULL;
	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxltabdesc->Pmdid());
	if (IMDRelation::ErelstorageExternal == pmdrel->Erelstorage())
	{
		pdxlop = GPOS_NEW(m_pmp) CDXLLogicalExternalGet(m_pmp, pdxltabdesc);
	}
	else
	{
		pdxlop = GPOS_NEW(m_pmp) CDXLLogicalGet(m_pmp, pdxltabdesc);
	}

	CDXLNode *pdxlnGet = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// make note of new columns from base relation
	m_pmapvarcolid->LoadTblColumns(m_ulQueryLevel, ulRTIndex, pdxltabdesc);

	return pdxlnGet;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromValues
//
//	@doc:
//		Returns a CDXLNode representing a range table entry of values
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromValues
	(
	const RangeTblEntry *prte,
	ULONG ulRTIndex,
	ULONG ulCurrQueryLevel
	)
{
	List *plTuples = prte->values_lists;
	GPOS_ASSERT(NULL != plTuples);

	const ULONG ulValues = gpdb::UlListLength(plTuples);
	GPOS_ASSERT(0 < ulValues);

	// children of the UNION ALL
	DrgPdxln *pdrgpdxln = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);

	// array of datum arrays for Values
	DrgPdrgPdxldatum *pdrgpdrgpdxldatumValues = GPOS_NEW(m_pmp) DrgPdrgPdxldatum(m_pmp);

	// array of input colid arrays
	DrgPdrgPul *pdrgpdrgulInputColIds = GPOS_NEW(m_pmp) DrgPdrgPul(m_pmp);

	// array of column descriptor for the UNION ALL operator
	DrgPdxlcd *pdrgpdxlcd = GPOS_NEW(m_pmp) DrgPdxlcd(m_pmp);

	// translate the tuples in the value scan
	ULONG ulTuplePos = 0;
	ListCell *plcTuple = NULL;
	GPOS_ASSERT(NULL != prte->eref);

	// flag for checking value list has only constants. For all constants --> VALUESCAN operator else retain UnionAll
	BOOL fAllConstant = true;
	ForEach (plcTuple, plTuples)
	{
		List *plTuple = (List *) lfirst(plcTuple);
		GPOS_ASSERT(IsA(plTuple, List));

		// array of column colids  
		DrgPul *pdrgpulColIds = GPOS_NEW(m_pmp) DrgPul(m_pmp);

		// array of project elements (for expression elements)
		DrgPdxln *pdrgpdxlnPrEl = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);

		// array of datum (for datum constant values)
		DrgPdxldatum *pdrgpdxldatum = GPOS_NEW(m_pmp) DrgPdxldatum(m_pmp);

		// array of column descriptors for the CTG containing the datum array
		DrgPdxlcd *pdrgpdxlcdCTG = GPOS_NEW(m_pmp) DrgPdxlcd(m_pmp);

		List *plColnames = prte->eref->colnames;
		GPOS_ASSERT(NULL != plColnames);
		GPOS_ASSERT(gpdb::UlListLength(plTuple) == gpdb::UlListLength(plColnames));

		// translate the columns
		ULONG ulColPos = 0;
		ListCell *plcColumn = NULL;
		ForEach (plcColumn, plTuple)
		{
			Expr *pexpr = (Expr *) lfirst(plcColumn);

			CHAR *szColName = (CHAR *) strVal(gpdb::PvListNth(plColnames, ulColPos));
			ULONG ulColId = ULONG_MAX;	
			if (IsA(pexpr, Const))
			{
				// extract the datum
				Const *pconst = (Const *) pexpr;
				CDXLDatum *pdxldatum = m_psctranslator->Pdxldatum(pconst);
				pdrgpdxldatum->Append(pdxldatum);

				ulColId = m_pidgtorCol->UlNextId();

				CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, szColName);
				CMDName *pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
				GPOS_DELETE(pstrAlias);

				CDXLColDescr *pdxlcd = GPOS_NEW(m_pmp) CDXLColDescr
													(
													m_pmp,
													pmdname,
													ulColId,
													ulColPos + 1 /* iAttno */,
													GPOS_NEW(m_pmp) CMDIdGPDB(pconst->consttype),
													false /* fDropped */
													);

				if (0 == ulTuplePos)
				{
					pdxlcd->AddRef();
					pdrgpdxlcd->Append(pdxlcd);
				}
				pdrgpdxlcdCTG->Append(pdxlcd);
			}
			else
			{
				fAllConstant = false;
				// translate the scalar expression into a project element
				CDXLNode *pdxlnPrE = PdxlnPrEFromGPDBExpr(pexpr, szColName, true /* fInsistNewColIds */ );
				pdrgpdxlnPrEl->Append(pdxlnPrE);
				ulColId = CDXLScalarProjElem::PdxlopConvert(pdxlnPrE->Pdxlop())->UlId();

				if (0 == ulTuplePos)
				{
					CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, szColName);
					CMDName *pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
					GPOS_DELETE(pstrAlias);

					CDXLColDescr *pdxlcd = GPOS_NEW(m_pmp) CDXLColDescr
														(
														m_pmp,
														pmdname,
														ulColId,
														ulColPos + 1 /* iAttno */,
														GPOS_NEW(m_pmp) CMDIdGPDB(gpdb::OidExprType((Node*) pexpr)),
														false /* fDropped */
														);
					pdrgpdxlcd->Append(pdxlcd);
				}
			}

			GPOS_ASSERT(ULONG_MAX != ulColId);

			pdrgpulColIds->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
			ulColPos++;
		}

		pdrgpdxln->Append(PdxlnFromColumnValues(pdrgpdxldatum, pdrgpdxlcdCTG, pdrgpdxlnPrEl));
		if (fAllConstant)
		{
			pdrgpdxldatum->AddRef();
			pdrgpdrgpdxldatumValues->Append(pdrgpdxldatum);
		}

		pdrgpdrgulInputColIds->Append(pdrgpulColIds);
		ulTuplePos++;

		// cleanup
		pdrgpdxldatum->Release();
		pdrgpdxlnPrEl->Release();
		pdrgpdxlcdCTG->Release();
	}

	GPOS_ASSERT(NULL != pdrgpdxlcd);

	if (fAllConstant)
	{
		// create Const Table DXL Node
		CDXLLogicalConstTable *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalConstTable(m_pmp, pdrgpdxlcd, pdrgpdrgpdxldatumValues);
		CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

		// make note of new columns from Value Scan
		m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdxlop->Pdrgpdxlcd());

		// cleanup
		pdrgpdxln->Release();
		pdrgpdrgulInputColIds->Release();

		return pdxln;
	}
	else if (1 < ulValues)
	{
		// create a UNION ALL operator
		CDXLLogicalSetOp *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalSetOp(m_pmp, EdxlsetopUnionAll, pdrgpdxlcd, pdrgpdrgulInputColIds, false);
		CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop, pdrgpdxln);

		// make note of new columns from UNION ALL
		m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdxlop->Pdrgpdxlcd());
		pdrgpdrgpdxldatumValues->Release();

		return pdxln;
	}

	GPOS_ASSERT(1 == pdrgpdxln->UlLength());

	CDXLNode *pdxln = (*pdrgpdxln)[0];
	pdxln->AddRef();

	// make note of new columns
	m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdrgpdxlcd);	

	//cleanup
	pdrgpdrgpdxldatumValues->Release();
	pdrgpdxln->Release();
	pdrgpdrgulInputColIds->Release();
	pdrgpdxlcd->Release();

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromColumnValues
//
//	@doc:
//		Generate a DXL node from column values, where each column value is 
//		either a datum or scalar expression represented as project element.
//		Each datum is associated with a column descriptors used by the CTG
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromColumnValues
	(
	DrgPdxldatum *pdrgpdxldatumCTG,
	DrgPdxlcd *pdrgpdxlcdCTG,
	DrgPdxln *pdrgpdxlnPrEl
	)
	const
{
	GPOS_ASSERT(NULL != pdrgpdxldatumCTG);
	GPOS_ASSERT(NULL != pdrgpdxlnPrEl);
	
	CDXLNode *pdxlnCTG = NULL;
	if (0 == pdrgpdxldatumCTG->UlLength())
	{
		// add a dummy CTG
		pdxlnCTG = PdxlnConstTableGet();
	}
	else 
	{
		// create the array of datum arrays
		DrgPdrgPdxldatum *pdrgpdrgpdxldatumCTG = GPOS_NEW(m_pmp) DrgPdrgPdxldatum(m_pmp);
		
		pdrgpdxldatumCTG->AddRef();
		pdrgpdrgpdxldatumCTG->Append(pdrgpdxldatumCTG);
		
		pdrgpdxlcdCTG->AddRef();
		CDXLLogicalConstTable *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalConstTable(m_pmp, pdrgpdxlcdCTG, pdrgpdrgpdxldatumCTG);
		
		pdxlnCTG = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);
	}

	if (0 == pdrgpdxlnPrEl->UlLength())
	{
		return pdxlnCTG;
	}

	// create a project node for the list of project elements
	pdrgpdxlnPrEl->AddRef();
	CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode
										(
										m_pmp,
										GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp),
										pdrgpdxlnPrEl
										);
	
	CDXLNode *pdxlnProject = GPOS_NEW(m_pmp) CDXLNode
											(
											m_pmp,
											GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp),
											pdxlnPrL,
											pdxlnCTG
											);

	return pdxlnProject;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromTVF
//
//	@doc:
//		Returns a CDXLNode representing a from relation range table entry
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromTVF
	(
	const RangeTblEntry *prte,
	ULONG ulRTIndex,
	ULONG //ulCurrQueryLevel
	)
{
	GPOS_ASSERT(NULL != prte->funcexpr);

	// if this is a folded function expression, generate a project over a CTG
	if (!IsA(prte->funcexpr, FuncExpr))
	{
		CDXLNode *pdxlnCTG = PdxlnConstTableGet();

		CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr((Expr *) prte->funcexpr, prte->eref->aliasname, true /* fInsistNewColIds */);
		pdxlnPrL->AddChild(pdxlnPrEl);

		CDXLNode *pdxlnProject = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp));
		pdxlnProject->AddChild(pdxlnPrL);
		pdxlnProject->AddChild(pdxlnCTG);

		m_pmapvarcolid->LoadProjectElements(m_ulQueryLevel, ulRTIndex, pdxlnPrL);

		return pdxlnProject;
	}

	CDXLLogicalTVF *pdxlopTVF = CTranslatorUtils::Pdxltvf(m_pmp, m_pmda, m_pidgtorCol, prte);
	CDXLNode *pdxlnTVF = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopTVF);

	// make note of new columns from function
	m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdxlopTVF->Pdrgpdxlcd());

	FuncExpr *pfuncexpr = (FuncExpr *) prte->funcexpr;
	BOOL fSubqueryInArgs = false;

	// check if arguments contain SIRV functions
	if (NIL != pfuncexpr->args && FHasSirvFunctions((Node *) pfuncexpr->args))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}

	ListCell *plc = NULL;
	ForEach (plc, pfuncexpr->args)
	{
		Node *pnodeArg = (Node *) lfirst(plc);
		fSubqueryInArgs = fSubqueryInArgs || CTranslatorUtils::FHasSubquery(pnodeArg);
		CDXLNode *pdxlnFuncExprArg =
				m_psctranslator->PdxlnScOpFromExpr((Expr *) pnodeArg, m_pmapvarcolid, &m_fHasDistributedTables);
		GPOS_ASSERT(NULL != pdxlnFuncExprArg);
		pdxlnTVF->AddChild(pdxlnFuncExprArg);
	}

	CMDIdGPDB *pmdidFunc = GPOS_NEW(m_pmp) CMDIdGPDB(pfuncexpr->funcid);
	const IMDFunction *pmdfunc = m_pmda->Pmdfunc(pmdidFunc);
	if (fSubqueryInArgs && IMDFunction::EfsVolatile == pmdfunc->EfsStability())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Volatile functions with subqueries in arguments"));
	}
	pmdidFunc->Release();

	return pdxlnTVF;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromCTE
//
//	@doc:
//		Translate a common table expression into CDXLNode
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromCTE
	(
	const RangeTblEntry *prte,
	ULONG ulRTIndex,
	ULONG ulCurrQueryLevel
	)
{
	const ULONG ulCteQueryLevel = ulCurrQueryLevel - prte->ctelevelsup;
	const CCTEListEntry *pctelistentry = m_phmulCTEEntries->PtLookup(&ulCteQueryLevel);
	if (NULL == pctelistentry)
	{
		// TODO: Sept 09 2013, remove temporary fix  (revert exception to assert) to avoid crash during algebrization
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiQuery2DXLError,
			GPOS_WSZ_LIT("No CTE")
			);
	}

	const CDXLNode *pdxlnCTEProducer = pctelistentry->PdxlnCTEProducer(prte->ctename);
	const List *plCTEProducerTargetList = pctelistentry->PlCTEProducerTL(prte->ctename);
	
	GPOS_ASSERT(NULL != pdxlnCTEProducer && NULL != plCTEProducerTargetList);

	CDXLLogicalCTEProducer *pdxlopProducer = CDXLLogicalCTEProducer::PdxlopConvert(pdxlnCTEProducer->Pdxlop()); 
	ULONG ulCTEId = pdxlopProducer->UlId();
	DrgPul *pdrgpulCTEProducer = pdxlopProducer->PdrgpulColIds();
	
	// construct output column array
	DrgPul *pdrgpulCTEConsumer = PdrgpulGenerateColIds(m_pmp, pdrgpulCTEProducer->UlLength());
			
	// load the new columns from the CTE
	m_pmapvarcolid->LoadCTEColumns(ulCurrQueryLevel, ulRTIndex, pdrgpulCTEConsumer, const_cast<List *>(plCTEProducerTargetList));

	CDXLLogicalCTEConsumer *pdxlopCTEConsumer = GPOS_NEW(m_pmp) CDXLLogicalCTEConsumer(m_pmp, ulCTEId, pdrgpulCTEConsumer);
	CDXLNode *pdxlnCTE = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopCTEConsumer);

	return pdxlnCTE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnFromDerivedTable
//
//	@doc:
//		Translate a derived table into CDXLNode
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnFromDerivedTable
	(
	const RangeTblEntry *prte,
	ULONG ulRTIndex,
	ULONG ulCurrQueryLevel
	)
{
	Query *pqueryDerTbl = prte->subquery;
	GPOS_ASSERT(NULL != pqueryDerTbl);

	CMappingVarColId *pmapvarcolid = m_pmapvarcolid->PmapvarcolidCopy(m_pmp);

	CTranslatorQueryToDXL trquerytodxl
		(
		m_pmp,
		m_pmda,
		m_pidgtorCol,
		m_pidgtorCTE,
		pmapvarcolid,
		pqueryDerTbl,
		m_ulQueryLevel + 1,
		FDMLQuery(),
		m_phmulCTEEntries
		);

	// translate query representing the derived table to its DXL representation
	CDXLNode *pdxlnDerTbl = trquerytodxl.PdxlnFromQueryInternal();

	// get the output columns of the derived table
	DrgPdxln *pdrgpdxlnQueryOutputDerTbl = trquerytodxl.PdrgpdxlnQueryOutput();
	DrgPdxln *pdrgpdxlnCTE = trquerytodxl.PdrgpdxlnCTE();
	GPOS_ASSERT(NULL != pdxlnDerTbl && pdrgpdxlnQueryOutputDerTbl != NULL);

	CUtils::AddRefAppend(m_pdrgpdxlnCTE, pdrgpdxlnCTE);
	
	m_fHasDistributedTables = m_fHasDistributedTables || trquerytodxl.FHasDistributedTables();

	// make note of new columns from derived table
	m_pmapvarcolid->LoadDerivedTblColumns(ulCurrQueryLevel, ulRTIndex, pdrgpdxlnQueryOutputDerTbl, trquerytodxl.Pquery()->targetList);

	return pdxlnDerTbl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnScFromGPDBExpr
//
//	@doc:
//		Translate the Expr into a CDXLScalar node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnScFromGPDBExpr
	(
	Expr *pexpr
	)
{
	CDXLNode *pdxlnScalar = m_psctranslator->PdxlnScOpFromExpr(pexpr, m_pmapvarcolid, &m_fHasDistributedTables);
	GPOS_ASSERT(NULL != pdxlnScalar);

	return pdxlnScalar;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnLgJoinFromGPDBJoinExpr
//
//	@doc:
//		Translate the JoinExpr on a GPDB query into a CDXLLogicalJoin node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnLgJoinFromGPDBJoinExpr
	(
	JoinExpr *pjoinexpr
	)
{
	GPOS_ASSERT(NULL != pjoinexpr);

	CDXLNode *pdxlnLeftChild = PdxlnFromGPDBFromClauseEntry(pjoinexpr->larg);
	CDXLNode *pdxlnRightChild = PdxlnFromGPDBFromClauseEntry(pjoinexpr->rarg);
	EdxlJoinType edxljt = CTranslatorUtils::EdxljtFromJoinType(pjoinexpr->jointype);
	CDXLNode *pdxlnJoin = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalJoin(m_pmp, edxljt));

	GPOS_ASSERT(NULL != pdxlnLeftChild && NULL != pdxlnRightChild);

	pdxlnJoin->AddChild(pdxlnLeftChild);
	pdxlnJoin->AddChild(pdxlnRightChild);

	Node* pnode = pjoinexpr->quals;

	// translate the join condition
	if (NULL != pnode)
	{
		pdxlnJoin->AddChild(PdxlnScFromGPDBExpr( (Expr*) pnode));
	}
	else
	{
		// a cross join therefore add a CDXLScalarConstValue representing the value "true"
		pdxlnJoin->AddChild(PdxlnScConstValueTrue());
	}

	// extract the range table entry for the join expr to:
	// 1. Process the alias names of the columns
	// 2. Generate a project list for the join expr and maintain it in our hash map

	const ULONG ulRtIndex = pjoinexpr->rtindex;
	RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pquery->rtable, ulRtIndex - 1);
	GPOS_ASSERT(NULL != prte);

	Alias *palias = prte->eref;
	GPOS_ASSERT(NULL != palias);
	GPOS_ASSERT(NULL != palias->colnames && 0 < gpdb::UlListLength(palias->colnames));
	GPOS_ASSERT(gpdb::UlListLength(prte->joinaliasvars) == gpdb::UlListLength(palias->colnames));

	CDXLNode *pdxlnPrLComputedColumns = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));
	CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	// construct a proj element node for each entry in the joinaliasvars
	ListCell *plcNode = NULL;
	ListCell *plcColName = NULL;
	ForBoth (plcNode, prte->joinaliasvars,
			plcColName, palias->colnames)
	{
		Node *pnodeJoinAlias = (Node *) lfirst(plcNode);
		GPOS_ASSERT(IsA(pnodeJoinAlias, Var) || IsA(pnodeJoinAlias, CoalesceExpr));
		Value *pvalue = (Value *) lfirst(plcColName);
		CHAR *szColName = strVal(pvalue);

		// create the DXL node holding the target list entry and add it to proj list
		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr( (Expr*) pnodeJoinAlias, szColName);
		pdxlnPrL->AddChild(pdxlnPrEl);

		if (IsA(pnodeJoinAlias, CoalesceExpr))
		{
			// add coalesce expression to the computed columns
			pdxlnPrEl->AddRef();
			pdxlnPrLComputedColumns->AddChild(pdxlnPrEl);
		}
	}
	m_pmapvarcolid->LoadProjectElements(m_ulQueryLevel, ulRtIndex, pdxlnPrL);
	pdxlnPrL->Release();

	if (0 == pdxlnPrLComputedColumns->UlArity())
	{
		pdxlnPrLComputedColumns->Release();
		return pdxlnJoin;
	}

	CDXLNode *pdxlnProject = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp));
	pdxlnProject->AddChild(pdxlnPrLComputedColumns);
	pdxlnProject->AddChild(pdxlnJoin);

	return pdxlnProject;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnLgProjectFromGPDBTL
//
//	@doc:
//		Create a DXL project list from the target list. The function allocates
//		memory in the translator memory pool and caller responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnLgProjectFromGPDBTL
	(
	List *plTargetList,
	CDXLNode *pdxlnChild,
	HMIUl *phmiulSortgrouprefColId,
	HMIUl *phmiulOutputCols,
	List *plgrpcl,
	BOOL fExpandAggrefExpr
	)
{
	BOOL fGroupBy = (0 != gpdb::UlListLength(m_pquery->groupClause) || m_pquery->hasAggs);

	CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	// construct a proj element node for each entry in the target list
	ListCell *plcTE = NULL;
	
	// target entries that are result of flattening join alias
	// and are equivalent to a defined grouping column target entry
	List *plOmittedTE = NIL; 

	// list for all vars used in aggref expressions
	List *plVars = NULL;
	ULONG ulResno = 0;
	ForEach(plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));
		GPOS_ASSERT(0 < pte->resno);
		ulResno = pte->resno;

		BOOL fGroupingCol = CTranslatorUtils::FGroupingColumn(pte, plgrpcl);
		if (!fGroupBy || (fGroupBy && fGroupingCol))
		{
			CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr(pte->expr, pte->resname);
			ULONG ulColId = CDXLScalarProjElem::PdxlopConvert(pdxlnPrEl->Pdxlop())->UlId();

			AddSortingGroupingColumn(pte, phmiulSortgrouprefColId, ulColId);

			// add column to the list of output columns of the query
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);

			if (!IsA(pte->expr, Var))
			{
				// only add computed columns to the project list
				pdxlnPrL->AddChild(pdxlnPrEl);
			}
			else
			{
				pdxlnPrEl->Release();
			}
		}
		else if (fExpandAggrefExpr && IsA(pte->expr, Aggref))
		{
			plVars = gpdb::PlConcat(plVars, gpdb::PlExtractNodesExpression((Node *) pte->expr, T_Var, false /*descendIntoSubqueries*/));
		}
		else if (!IsA(pte->expr, Aggref))
		{
			plOmittedTE = gpdb::PlAppendElement(plOmittedTE, pte);
		}
	}

	// process target entries that are a result of flattening join alias
	plcTE = NULL;
	ForEach(plcTE, plOmittedTE)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		INT iSortGroupRef = (INT) pte->ressortgroupref;

		TargetEntry *pteGroupingColumn = CTranslatorUtils::PteGroupingColumn( (Node*) pte->expr, plgrpcl, plTargetList);
		if (NULL != pteGroupingColumn)
		{
			const ULONG ulColId = CTranslatorUtils::UlColId((INT) pteGroupingColumn->ressortgroupref, phmiulSortgrouprefColId);
			StoreAttnoColIdMapping(phmiulOutputCols, pte->resno, ulColId);
			if (0 < iSortGroupRef && 0 < ulColId && NULL == phmiulSortgrouprefColId->PtLookup(&iSortGroupRef))
			{
				AddSortingGroupingColumn(pte, phmiulSortgrouprefColId, ulColId);
			}
		}
	}
	if (NIL != plOmittedTE)
	{
		gpdb::GPDBFree(plOmittedTE);
	}

	GPOS_ASSERT_IMP(!fExpandAggrefExpr, NULL == plVars);
	
	// process all additional vars in aggref expressions
	ListCell *plcVar = NULL;
	ForEach (plcVar, plVars)
	{
		ulResno++;
		Var *pvar = (Var *) lfirst(plcVar);

		// TODO: Dec 28, 2012; figure out column's name
		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr((Expr*) pvar, "?col?");
		
		ULONG ulColId = CDXLScalarProjElem::PdxlopConvert(pdxlnPrEl->Pdxlop())->UlId();

		// add column to the list of output columns of the query
		StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);
		
		pdxlnPrEl->Release();
	}
	
	if (0 < pdxlnPrL->UlArity())
	{
		// create a node with the CDXLLogicalProject operator and add as its children:
		// the CDXLProjectList node and the node representing the input to the project node
		CDXLNode *pdxlnProject = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp));
		pdxlnProject->AddChild(pdxlnPrL);
		pdxlnProject->AddChild(pdxlnChild);
		GPOS_ASSERT(NULL != pdxlnProject);
		return pdxlnProject;
	}

	pdxlnPrL->Release();
	return pdxlnChild;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnProjectNullsForGroupingSets
//
//	@doc:
//		Construct a DXL project node projecting NULL values for the columns in the
//		given bitset
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnProjectNullsForGroupingSets
	(
	List *plTargetList,
	CDXLNode *pdxlnChild,
	CBitSet *pbs,					// group by columns
	HMIUl *phmiulSortgrouprefCols,	// mapping of sorting and grouping columns
	HMIUl *phmiulOutputCols,		// mapping of output columns
	HMUlUl *phmululGrpColPos		// mapping of unique grouping col positions
	)
	const
{
	CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	// construct a proj element node for those non-aggregate entries in the target list which
	// are not included in the grouping set
	ListCell *plcTE = NULL;
	ForEach (plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));

		BOOL fGroupingCol = pbs->FBit(pte->ressortgroupref);
		ULONG ulResno = pte->resno;

		ULONG ulColId = 0;
		
		if (IsA(pte->expr, GroupingFunc))
		{
			ulColId = m_pidgtorCol->UlNextId();
			CDXLNode *pdxlnGroupingFunc = PdxlnGroupingFunc(pte->expr, pbs, phmululGrpColPos);
			CMDName *pmdnameAlias = NULL;

			if (NULL == pte->resname)
			{
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("grouping"));
				pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol);
			}
			else
			{
				CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, pte->resname);
				pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
				GPOS_DELETE(pstrAlias);
			}
			CDXLNode *pdxlnPrEl = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjElem(m_pmp, ulColId, pmdnameAlias), pdxlnGroupingFunc);
			pdxlnPrL->AddChild(pdxlnPrEl);
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);
		}
		else if (!fGroupingCol && !IsA(pte->expr, Aggref))
		{
			OID oidType = gpdb::OidExprType((Node *) pte->expr);

			ulColId = m_pidgtorCol->UlNextId();

			CMDIdGPDB *pmdid = GPOS_NEW(m_pmp) CMDIdGPDB(oidType);
			CDXLNode *pdxlnPrEl = CTranslatorUtils::PdxlnPrElNull(m_pmp, m_pmda, pmdid, ulColId, pte->resname);
			pmdid->Release();
			
			pdxlnPrL->AddChild(pdxlnPrEl);
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);
		}
		
		INT iSortGroupRef = INT (pte->ressortgroupref);
		
		GPOS_ASSERT_IMP(0 == phmiulSortgrouprefCols, NULL != phmiulSortgrouprefCols->PtLookup(&iSortGroupRef) && "Grouping column with no mapping");
		
		if (0 < iSortGroupRef && 0 < ulColId && NULL == phmiulSortgrouprefCols->PtLookup(&iSortGroupRef))
		{
			AddSortingGroupingColumn(pte, phmiulSortgrouprefCols, ulColId);
		}
	}

	if (0 == pdxlnPrL->UlArity())
	{
		// no project necessary
		pdxlnPrL->Release();
		return pdxlnChild;
	}

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp), pdxlnPrL, pdxlnChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnProjectGroupingFuncs
//
//	@doc:
//		Construct a DXL project node projecting values for the grouping funcs in
//		the target list 
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnProjectGroupingFuncs
	(
	List *plTargetList,
	CDXLNode *pdxlnChild,
	CBitSet *pbs,
	HMIUl *phmiulOutputCols,
	HMUlUl *phmululGrpColPos,
	HMIUl *phmiulSortgrouprefColId
	)
	const
{
	CDXLNode *pdxlnPrL = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjList(m_pmp));

	// construct a proj element node for those non-aggregate entries in the target list which
	// are not included in the grouping set
	ListCell *plcTE = NULL;
	ForEach (plcTE, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(pte, TargetEntry));

		ULONG ulResno = pte->resno;

		if (IsA(pte->expr, GroupingFunc))
		{
			ULONG ulColId = m_pidgtorCol->UlNextId();
			CDXLNode *pdxlnGroupingFunc = PdxlnGroupingFunc(pte->expr, pbs, phmululGrpColPos);
			CMDName *pmdnameAlias = NULL;

			if (NULL == pte->resname)
			{
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("grouping"));
				pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol);
			}
			else
			{
				CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, pte->resname);
				pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
				GPOS_DELETE(pstrAlias);
			}
			CDXLNode *pdxlnPrEl = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjElem(m_pmp, ulColId, pmdnameAlias), pdxlnGroupingFunc);
			pdxlnPrL->AddChild(pdxlnPrEl);
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, ulColId);
			AddSortingGroupingColumn(pte, phmiulSortgrouprefColId, ulColId);
		}
	}

	if (0 == pdxlnPrL->UlArity())
	{
		// no project necessary
		pdxlnPrL->Release();
		return pdxlnChild;
	}

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalProject(m_pmp), pdxlnPrL, pdxlnChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::StoreAttnoColIdMapping
//
//	@doc:
//		Store mapping between attno and generate colid
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::StoreAttnoColIdMapping
	(
	HMIUl *phmiul,
	INT iAttno,
	ULONG ulColId
	)
	const
{
	GPOS_ASSERT(NULL != phmiul);

#ifdef GPOS_DEBUG
	BOOL fResult =
#endif // GPOS_DEBUG
	phmiul->FInsert(GPOS_NEW(m_pmp) INT(iAttno), GPOS_NEW(m_pmp) ULONG(ulColId));

	GPOS_ASSERT(fResult);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpdxlnConstructOutputCols
//
//	@doc:
//		Construct an array of DXL nodes representing the query output
//
//---------------------------------------------------------------------------
DrgPdxln *
CTranslatorQueryToDXL::PdrgpdxlnConstructOutputCols
	(
	List *plTargetList,
	HMIUl *phmiulAttnoColId
	)
	const
{
	GPOS_ASSERT(NULL != plTargetList);
	GPOS_ASSERT(NULL != phmiulAttnoColId);

	DrgPdxln *pdrgpdxln = GPOS_NEW(m_pmp) DrgPdxln(m_pmp);

	ListCell *plc = NULL;
	ForEach (plc, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plc);
		GPOS_ASSERT(0 < pte->resno);
		ULONG ulResNo = pte->resno;

		if (pte->resjunk)
		{
			continue;
		}

		GPOS_ASSERT(NULL != pte);
		CMDName *pmdname = NULL;
		if (NULL == pte->resname)
		{
			CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
			pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol);
		}
		else
		{
			CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, pte->resname);
			pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
			// CName constructor copies string
			GPOS_DELETE(pstrAlias);
		}

		const ULONG ulColId = CTranslatorUtils::UlColId(ulResNo, phmiulAttnoColId);

		// create a column reference
		CDXLColRef *pdxlcr = GPOS_NEW(m_pmp) CDXLColRef(m_pmp, pmdname, ulColId);
		CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_pmp) CDXLScalarIdent
												(
												m_pmp,
												pdxlcr,
												GPOS_NEW(m_pmp) CMDIdGPDB(gpdb::OidExprType( (Node*) pte->expr))
												);

		// create the DXL node holding the scalar ident operator
		CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopIdent);

		pdrgpdxln->Append(pdxln);
	}

	return pdrgpdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnPrEFromGPDBExpr
//
//	@doc:
//		Create a DXL project element node from the target list entry or var.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnPrEFromGPDBExpr
	(
	Expr *pexpr,
	const CHAR *szAliasName,
	BOOL fInsistNewColIds
	)
{
	GPOS_ASSERT(NULL != pexpr);

	// construct a scalar operator
	CDXLNode *pdxlnChild = PdxlnScFromGPDBExpr(pexpr);

	// get the id and alias for the proj elem
	ULONG ulPrElId;
	CMDName *pmdnameAlias = NULL;

	if (NULL == szAliasName)
	{
		CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
		pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol);
	}
	else
	{
		CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, szAliasName);
		pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, pstrAlias);
		GPOS_DELETE(pstrAlias);
	}

	if (IsA(pexpr, Var) && !fInsistNewColIds)
	{
		// project elem is a a reference to a column - use the colref id
		GPOS_ASSERT(EdxlopScalarIdent == pdxlnChild->Pdxlop()->Edxlop());
		CDXLScalarIdent *pdxlopIdent = (CDXLScalarIdent *) pdxlnChild->Pdxlop();
		ulPrElId = pdxlopIdent->Pdxlcr()->UlID();
	}
	else
	{
		// project elem is a defined column - get a new id
		ulPrElId = m_pidgtorCol->UlNextId();
	}

	CDXLNode *pdxlnPrEl = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjElem(m_pmp, ulPrElId, pmdnameAlias));
	pdxlnPrEl->AddChild(pdxlnChild);

	return pdxlnPrEl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnScConstValueTrue
//
//	@doc:
//		Returns a CDXLNode representing scalar condition "true"
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnScConstValueTrue()
{
	Const *pconst = (Const*) gpdb::PnodeMakeBoolConst(true /*value*/, false /*isnull*/);
	CDXLNode *pdxln = PdxlnScFromGPDBExpr((Expr*) pconst);
	gpdb::GPDBFree(pconst);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdxlnGroupingFunc
//
//	@doc:
//		Translate grouping func
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::PdxlnGroupingFunc
	(
	const Expr *pexpr,
	CBitSet *pbs,
	HMUlUl *phmululGrpColPos
	)
	const
{
	GPOS_ASSERT(IsA(pexpr, GroupingFunc));
	GPOS_ASSERT(NULL != phmululGrpColPos);

	const GroupingFunc *pgroupingfunc = (GroupingFunc *) pexpr;

	if (1 < gpdb::UlListLength(pgroupingfunc->args))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Grouping function with multiple arguments"));
	}
	
	Node *pnode = (Node *) gpdb::PvListNth(pgroupingfunc->args, 0);
	ULONG ulGroupingIndex = gpdb::IValue(pnode); 
		
	// generate a constant value for the result of the grouping function as follows:
	// if the grouping function argument is a group-by column, result is 0
	// otherwise, the result is 1 
	LINT lValue = 0;
	
	ULONG *pulSortGrpRef = phmululGrpColPos->PtLookup(&ulGroupingIndex);
	GPOS_ASSERT(NULL != pulSortGrpRef);
	BOOL fGroupingCol = pbs->FBit(*pulSortGrpRef);	
	if (!fGroupingCol)
	{
		// not a grouping column
		lValue = 1; 
	}

	const IMDType *pmdtype = m_pmda->PtMDType<IMDTypeInt8>(m_sysid);
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(m_pmp) CMDIdGPDB(*pmdidMDC);
	
	CDXLDatum *pdxldatum = GPOS_NEW(m_pmp) CDXLDatumInt8(m_pmp, pmdid, false /* fNull */, lValue);
	CDXLScalarConstValue *pdxlop = GPOS_NEW(m_pmp) CDXLScalarConstValue(m_pmp, pdxldatum);
	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::ConstructCTEProducerList
//
//	@doc:
//		Construct a list of CTE producers from the query's CTE list
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::ConstructCTEProducerList
	(
	List *plCTE,
	ULONG ulCteQueryLevel
	)
{
	GPOS_ASSERT(NULL != m_pdrgpdxlnCTE && "CTE Producer list not initialized"); 
	
	if (NULL == plCTE)
	{
		return;
	}
	
	ListCell *plc = NULL;
	
	ForEach (plc, plCTE)
	{
		CommonTableExpr *pcte = (CommonTableExpr *) lfirst(plc);
		GPOS_ASSERT(IsA(pcte->ctequery, Query));
		
		if (pcte->cterecursive)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("WITH RECURSIVE"));
		}

		Query *pqueryCte = CQueryMutators::PqueryNormalize(m_pmp, m_pmda, (Query *) pcte->ctequery, ulCteQueryLevel + 1);
		
		// the query representing the cte can only access variables defined in the current level as well as
		// those defined at prior query levels
		CMappingVarColId *pmapvarcolid = m_pmapvarcolid->PmapvarcolidCopy(ulCteQueryLevel);

		CTranslatorQueryToDXL trquerytodxl
			(
			m_pmp,
			m_pmda,
			m_pidgtorCol,
			m_pidgtorCTE,
			pmapvarcolid,
			pqueryCte,
			ulCteQueryLevel + 1,
			FDMLQuery(),
			m_phmulCTEEntries
			);

		// translate query representing the cte table to its DXL representation
		CDXLNode *pdxlnCteChild = trquerytodxl.PdxlnFromQueryInternal();

		// get the output columns of the cte table
		DrgPdxln *pdrgpdxlnQueryOutputCte = trquerytodxl.PdrgpdxlnQueryOutput();
		DrgPdxln *pdrgpdxlnCTE = trquerytodxl.PdrgpdxlnCTE();
		m_fHasDistributedTables = m_fHasDistributedTables || trquerytodxl.FHasDistributedTables();

		GPOS_ASSERT(NULL != pdxlnCteChild && NULL != pdrgpdxlnQueryOutputCte && NULL != pdrgpdxlnCTE);
		
		// append any nested CTE
		CUtils::AddRefAppend(m_pdrgpdxlnCTE, pdrgpdxlnCTE);
		
		DrgPul *pdrgpulColIds = GPOS_NEW(m_pmp) DrgPul(m_pmp);
		
		const ULONG ulOutputCols = pdrgpdxlnQueryOutputCte->UlLength();
		for (ULONG ul = 0; ul < ulOutputCols; ul++)
		{
			CDXLNode *pdxlnOutputCol = (*pdrgpdxlnQueryOutputCte)[ul];
			CDXLScalarIdent *pdxlnIdent = CDXLScalarIdent::PdxlopConvert(pdxlnOutputCol->Pdxlop());
			pdrgpulColIds->Append(GPOS_NEW(m_pmp) ULONG(pdxlnIdent->Pdxlcr()->UlID()));
		}
		
		CDXLLogicalCTEProducer *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalCTEProducer(m_pmp, m_pidgtorCTE->UlNextId(), pdrgpulColIds);
		CDXLNode *pdxlnCTEProducer = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop, pdxlnCteChild);
		
		m_pdrgpdxlnCTE->Append(pdxlnCTEProducer);
#ifdef GPOS_DEBUG
		BOOL fResult =
#endif
		m_phmulfCTEProducers->FInsert(GPOS_NEW(m_pmp) ULONG(pdxlop->UlId()), GPOS_NEW(m_pmp) BOOL(true));
		GPOS_ASSERT(fResult);
		
		// update CTE producer mappings
		CCTEListEntry *pctelistentry = m_phmulCTEEntries->PtLookup(&ulCteQueryLevel);
		if (NULL == pctelistentry)
		{
			pctelistentry = GPOS_NEW(m_pmp) CCTEListEntry (m_pmp, ulCteQueryLevel, pcte, pdxlnCTEProducer);
#ifdef GPOS_DEBUG
		BOOL fRes =
#endif
			m_phmulCTEEntries->FInsert(GPOS_NEW(m_pmp) ULONG(ulCteQueryLevel), pctelistentry);
			GPOS_ASSERT(fRes);
		}
		else
		{
			pctelistentry->AddCTEProducer(m_pmp, pcte, pdxlnCTEProducer);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::ConstructCTEAnchors
//
//	@doc:
//		Construct a stack of CTE anchors for each CTE producer in the given array
//
//---------------------------------------------------------------------------
void
CTranslatorQueryToDXL::ConstructCTEAnchors
	(
	DrgPdxln *pdrgpdxln,
	CDXLNode **ppdxlnCTEAnchorTop,
	CDXLNode **ppdxlnCTEAnchorBottom
	)
{
	GPOS_ASSERT(NULL == *ppdxlnCTEAnchorTop);
	GPOS_ASSERT(NULL == *ppdxlnCTEAnchorBottom);

	if (NULL == pdrgpdxln || 0 == pdrgpdxln->UlLength())
	{
		return;
	}
	
	const ULONG ulCTEs = pdrgpdxln->UlLength();
	
	for (ULONG ul = ulCTEs; ul > 0; ul--)
	{
		// construct a new CTE anchor on top of the previous one
		CDXLNode *pdxlnCTEProducer = (*pdrgpdxln)[ul-1];
		CDXLLogicalCTEProducer *pdxlopCTEProducer = CDXLLogicalCTEProducer::PdxlopConvert(pdxlnCTEProducer->Pdxlop());
		ULONG ulCTEProducerId = pdxlopCTEProducer->UlId();
		
		if (NULL == m_phmulfCTEProducers->PtLookup(&ulCTEProducerId))
		{
			// cte not defined at this level: CTE anchor was already added
			continue;
		}
		
		CDXLNode *pdxlnCTEAnchorNew = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLLogicalCTEAnchor(m_pmp, ulCTEProducerId));
		
		if (NULL == *ppdxlnCTEAnchorBottom)
		{
			*ppdxlnCTEAnchorBottom = pdxlnCTEAnchorNew;
		}

		if (NULL != *ppdxlnCTEAnchorTop)
		{
			pdxlnCTEAnchorNew->AddChild(*ppdxlnCTEAnchorTop);
		}
		*ppdxlnCTEAnchorTop = pdxlnCTEAnchorNew;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpulGenerateColIds
//
//	@doc:
//		Generate an array of new column ids of the given size
//
//---------------------------------------------------------------------------
DrgPul *
CTranslatorQueryToDXL::PdrgpulGenerateColIds
	(
	IMemoryPool *pmp,
	ULONG ulSize
	)
	const
{
	DrgPul *pdrgpul = GPOS_NEW(pmp) DrgPul(pmp);
	
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		pdrgpul->Append(GPOS_NEW(pmp) ULONG(m_pidgtorCol->UlNextId()));
	}
	
	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpulExtractColIds
//
//	@doc:
//		Extract column ids from the given mapping
//
//---------------------------------------------------------------------------
DrgPul *
CTranslatorQueryToDXL::PdrgpulExtractColIds
	(
	IMemoryPool *pmp,
	HMIUl *phmiul
	)
	const
{
	HMUlUl *phmulul = GPOS_NEW(pmp) HMUlUl(pmp);
	
	DrgPul *pdrgpul = GPOS_NEW(pmp) DrgPul(pmp);
	
	HMIUlIter mi(phmiul);
	while (mi.FAdvance())
	{
		ULONG ulColId = *(mi.Pt());
		
		// do not insert colid if already inserted
		if (NULL == phmulul->PtLookup(&ulColId))
		{
			pdrgpul->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
			phmulul->FInsert(GPOS_NEW(m_pmp) ULONG(ulColId), GPOS_NEW(m_pmp) ULONG(ulColId));
		}
	}
		
	phmulul->Release();
	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PhmiulRemapColIds
//
//	@doc:
//		Construct a new hashmap which replaces the values in the From array
//		with the corresponding value in the To array
//
//---------------------------------------------------------------------------
HMIUl *
CTranslatorQueryToDXL::PhmiulRemapColIds
	(
	IMemoryPool *pmp,
	HMIUl *phmiul,
	DrgPul *pdrgpulFrom,
	DrgPul *pdrgpulTo
	)
	const
{
	GPOS_ASSERT(NULL != phmiul);
	GPOS_ASSERT(NULL != pdrgpulFrom && NULL != pdrgpulTo);
	GPOS_ASSERT(pdrgpulFrom->UlLength() == pdrgpulTo->UlLength());
	
	// compute a map of the positions in the from array
	HMUlUl *phmulul = GPOS_NEW(pmp) HMUlUl(pmp);
	const ULONG ulSize = pdrgpulFrom->UlLength();
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
#ifdef GPOS_DEBUG
		BOOL fResult = 
#endif // GPOS_DEBUG
		phmulul->FInsert(GPOS_NEW(pmp) ULONG(*((*pdrgpulFrom)[ul])), GPOS_NEW(pmp) ULONG(*((*pdrgpulTo)[ul])));
		GPOS_ASSERT(fResult);
	}

	HMIUl *phmiulResult = GPOS_NEW(pmp) HMIUl(pmp);
	HMIUlIter mi(phmiul);
	while (mi.FAdvance())
	{
		INT *piKey = GPOS_NEW(pmp) INT(*(mi.Pk()));
		const ULONG *pulValue = mi.Pt();
		GPOS_ASSERT(NULL != pulValue);
		
		ULONG *pulValueRemapped = GPOS_NEW(pmp) ULONG(*(phmulul->PtLookup(pulValue)));
		phmiulResult->FInsert(piKey, pulValueRemapped);
	}
		
	phmulul->Release();
	
	return phmiulResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PhmiulRemapColIds
//
//	@doc:
//		True iff this query or one of its ancestors is a DML query
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::FDMLQuery()
{
	return (m_fTopDMLQuery || m_pquery->resultRelation != 0);
}

// EOF
