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
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *pidgtorColId,
	CIdGenerator *pidgtorCTE,
	CMappingVarColId *var_col_id_mapping,
	Query *query,
	ULONG query_level,
	BOOL fTopDMLQuery,
	HMUlCTEListEntry *phmulCTEEntries
	)
	:
	m_memory_pool(memory_pool),
	m_sysid(IMDId::EmdidGPDB, GPMD_GPDB_SYSID),
	m_pmda(md_accessor),
	m_pidgtorCol(pidgtorColId),
	m_pidgtorCTE(pidgtorCTE),
	m_pmapvarcolid(var_col_id_mapping),
	m_ulQueryLevel(query_level),
	m_fHasDistributedTables(false),
	m_fTopDMLQuery(fTopDMLQuery),
	m_fCTASQuery(false),
	m_phmulCTEEntries(NULL),
	m_pdrgpdxlnQueryOutput(NULL),
	m_pdrgpdxlnCTE(NULL),
	m_phmulfCTEProducers(NULL)
{
	GPOS_ASSERT(NULL != query);
	CheckSupportedCmdType(query);
	
	m_phmulCTEEntries = GPOS_NEW(m_memory_pool) HMUlCTEListEntry(m_memory_pool);
	m_pdrgpdxlnCTE = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
	m_phmulfCTEProducers = GPOS_NEW(m_memory_pool) HMUlF(m_memory_pool);
	
	if (NULL != phmulCTEEntries)
	{
		HMIterUlCTEListEntry hmiterullist(phmulCTEEntries);

		while (hmiterullist.Advance())
		{
			ULONG ulCTEQueryLevel = *(hmiterullist.Key());

			CCTEListEntry *pctelistentry =  const_cast<CCTEListEntry *>(hmiterullist.Value());

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

			if (ulCTEQueryLevel < query_level && NULL != pctelistentry)
			{
				pctelistentry->AddRef();
#ifdef GPOS_DEBUG
				BOOL fRes =
#endif
				m_phmulCTEEntries->Insert(GPOS_NEW(memory_pool) ULONG(ulCTEQueryLevel), pctelistentry);
				GPOS_ASSERT(fRes);
			}
		}
	}

	// check if the query has any unsupported node types
	CheckUnsupportedNodeTypes(query);

	// check if the query has SIRV functions in the targetlist without a FROM clause
	CheckSirvFuncsWithoutFromClause(query);

	// first normalize the query
	m_pquery = CQueryMutators::PqueryNormalize(m_memory_pool, m_pmda, query, query_level);

	if (NULL != m_pquery->cteList)
	{
		ConstructCTEProducerList(m_pquery->cteList, query_level);
	}

	m_psctranslator = GPOS_NEW(m_memory_pool) CTranslatorScalarToDXL
									(
									m_memory_pool,
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
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *pidgtorColId,
	CIdGenerator *pidgtorCTE,
	CMappingVarColId *var_col_id_mapping,
	Query *query,
	ULONG query_level,
	HMUlCTEListEntry *phmulCTEEntries
	)
{
	return GPOS_NEW(memory_pool) CTranslatorQueryToDXL
		(
		memory_pool,
		md_accessor,
		pidgtorColId,
		pidgtorCTE,
		var_col_id_mapping,
		query,
		query_level,
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
	Query *query
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
		{T_CurrentOfExpr, GPOS_WSZ_LIT("CURRENT OF")},
	};

	List *plUnsupported = NIL;
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgUnsupported); ul++)
	{
		plUnsupported = gpdb::PlAppendInt(plUnsupported, rgUnsupported[ul].ent);
	}

	INT iUnsupported = gpdb::IFindNodes((Node *) query, plUnsupported);
	gpdb::GPDBFree(plUnsupported);

	if (0 <= iUnsupported)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, rgUnsupported[iUnsupported].m_wsz);
	}

	// GDPB_91_MERGE_FIXME: collation
	INT iNonDefaultCollation = gpdb::ICheckCollation((Node *) query);

	if (0 < iNonDefaultCollation)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Non-default collation"));
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
	Query *query
	)
{
	// if there is a FROM clause or if target list is empty, look no further
	if ((NULL != query->jointree && 0 < gpdb::ListLength(query->jointree->fromlist))
		|| NIL == query->targetList)
	{
		return;
	}

	// see if we have SIRV functions in the target list
	if (FHasSirvFunctions((Node *) query->targetList))
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
	ListCell *lc = NULL;

	BOOL fHasSirv = false;
	ForEach (lc, plFunctions)
	{
		FuncExpr *pfuncexpr = (FuncExpr *) lfirst(lc);
		if (CTranslatorUtils::FSirvFunc(m_memory_pool, m_pmda, pfuncexpr->funcid))
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
	Query *query
	)
{
	if (NULL != query->utilityStmt)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("UTILITY command"));
	}

	if (CMD_SELECT == query->commandType)
	{		
		if (!optimizer_enable_ctas && NULL != query->intoClause)
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
		if (mapelem.m_cmdtype == query->commandType)
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
DXLNodeArray *
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
DXLNodeArray *
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

	// RETURNING is not supported yet.
	if (m_pquery->returningList)
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("RETURNING clause"));

	CDXLNode *pdxlnChild = NULL;
	IntUlongHashMap *phmiulSortGroupColsColId =  GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
	IntUlongHashMap *phmiulOutputCols = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);

	// construct CTEAnchor operators for the CTEs defined at the top level
	CDXLNode *pdxlnCTEAnchorTop = NULL;
	CDXLNode *pdxlnCTEAnchorBottom = NULL;
	ConstructCTEAnchors(m_pdrgpdxlnCTE, &pdxlnCTEAnchorTop, &pdxlnCTEAnchorBottom);
	GPOS_ASSERT_IMP(m_pdrgpdxlnCTE == NULL || 0 < m_pdrgpdxlnCTE->Size(),
					NULL != pdxlnCTEAnchorTop && NULL != pdxlnCTEAnchorBottom);
	
	GPOS_ASSERT_IMP(NULL != m_pquery->setOperations, 0 == gpdb::ListLength(m_pquery->windowClause));
	if (NULL != m_pquery->setOperations)
	{
		List *target_list = m_pquery->targetList;
		// translate set operations
		pdxlnChild = PdxlnFromSetOp(m_pquery->setOperations, target_list, phmiulOutputCols);

		CDXLLogicalSetOp *pdxlop = CDXLLogicalSetOp::Cast(pdxlnChild->GetOperator());
		const ColumnDescrDXLArray *pdrgpdxlcd = pdxlop->GetColumnDescrDXLArray();
		ListCell *plcTE = NULL;
		ULONG ulResNo = 1;
		ForEach (plcTE, target_list)
		{
			TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
			if (0 < target_entry->ressortgroupref)
			{
				ULONG col_id = ((*pdrgpdxlcd)[ulResNo - 1])->Id();
				AddSortingGroupingColumn(target_entry, phmiulSortGroupColsColId, col_id);
			}
			ulResNo++;
		}
	}
	else if (0 != gpdb::ListLength(m_pquery->windowClause)) // translate window clauses
	{
		CDXLNode *dxlnode = PdxlnFromGPDBFromExpr(m_pquery->jointree);
		GPOS_ASSERT(NULL == m_pquery->groupClause);
		pdxlnChild = PdxlnWindow
						(
						dxlnode,
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
		m_pdrgpdxlnQueryOutput = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
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
	List *target_list,
	FromExpr *pfromexpr,
	IntUlongHashMap *phmiulSortGroupColsColId,
	IntUlongHashMap *phmiulOutputCols,
	List *plGroupClause
	)
{
	CDXLNode *pdxlnJoinTree = PdxlnFromGPDBFromExpr(pfromexpr);

	// translate target list entries into a logical project
	return PdxlnLgProjectFromGPDBTL(target_list, pdxlnJoinTree, phmiulSortGroupColsColId, phmiulOutputCols, plGroupClause);
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
	List *target_list,
	FromExpr *pfromexpr,
	IntUlongHashMap *phmiulSortGroupColsColId,
	IntUlongHashMap *phmiulOutputCols,
	List *plGroupClause
	)
{
	CDXLNode *pdxlnJoinTree = PdxlnFromGPDBFromExpr(pfromexpr);

	// translate target list entries into a logical project
	return PdxlnLgProjectFromGPDBTL(target_list, pdxlnJoinTree, phmiulSortGroupColsColId, phmiulOutputCols, plGroupClause, true /*fExpandAggrefExpr*/);
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

	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);
	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::FRelHasTriggers(m_memory_pool, m_pmda, pmdrel, Edxldmlinsert))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("INSERT with triggers"));
	}

	BOOL fRelHasConstraints = CTranslatorUtils::FRelHasConstraints(pmdrel);
	if (!optimizer_enable_dml_constraints && fRelHasConstraints)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("INSERT with constraints"));
	}
	
	const ULONG ulLenTblCols = CTranslatorUtils::UlNonSystemColumns(pmdrel);
	const ULONG ulLenTL = gpdb::ListLength(m_pquery->targetList);
	GPOS_ASSERT(ulLenTblCols >= ulLenTL);
	GPOS_ASSERT(ulLenTL == m_pdrgpdxlnQueryOutput->Size());

	CDXLNode *project_list_dxl = NULL;
	
	const ULONG ulSystemCols = pmdrel->ColumnCount() - ulLenTblCols;
	const ULONG ulLenNonDroppedCols = pmdrel->NonDroppedColsCount() - ulSystemCols;
	if (ulLenNonDroppedCols > ulLenTL)
	{
		// missing target list entries
		project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));
	}

	ULongPtrArray *pdrgpulSource = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	ULONG ulPosTL = 0;
	for (ULONG ul = 0; ul < ulLenTblCols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		GPOS_ASSERT(!pmdcol->IsSystemColumn());
		
		if (pmdcol->IsDropped())
		{
			continue;
		}
		
		if (ulPosTL < ulLenTL)
		{
			INT iAttno = pmdcol->AttrNum();
			
			TargetEntry *target_entry = (TargetEntry *) gpdb::PvListNth(m_pquery->targetList, ulPosTL);
			AttrNumber iResno = target_entry->resno;

			if (iAttno == iResno)
			{
				CDXLNode *pdxlnCol = (*m_pdrgpdxlnQueryOutput)[ulPosTL];
				CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::Cast(pdxlnCol->GetOperator());
				pdrgpulSource->Append(GPOS_NEW(m_memory_pool) ULONG(pdxlopIdent->MakeDXLColRef()->Id()));
				ulPosTL++;
				continue;
			}
		}

		// target entry corresponding to the tables column not found, therefore
		// add a project element with null value scalar child
		CDXLNode *pdxlnPrE = CTranslatorUtils::PdxlnPrElNull(m_memory_pool, m_pmda, m_pidgtorCol, pmdcol);
		ULONG col_id = CDXLScalarProjElem::Cast(pdxlnPrE->GetOperator())->Id();
 	 	project_list_dxl->AddChild(pdxlnPrE);
	 	pdrgpulSource->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	CDXLLogicalInsert *pdxlopInsert = GPOS_NEW(m_memory_pool) CDXLLogicalInsert(m_memory_pool, table_descr, pdrgpulSource);

	if (NULL != project_list_dxl)
	{
		GPOS_ASSERT(0 < project_list_dxl->Arity());
		
		CDXLNode *pdxlnProject = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		pdxlnProject->AddChild(project_list_dxl);
		pdxlnProject->AddChild(pdxlnQuery);
		pdxlnQuery = pdxlnProject;
	}

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopInsert, pdxlnQuery);
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
		
	CMDName *pmdnameRel = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, pintocl->rel->relname);
	
	ColumnDescrDXLArray *pdrgpdxlcd = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);
	
	const ULONG ulColumns = gpdb::ListLength(m_pquery->targetList);

	ULongPtrArray *pdrgpulSource = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	IntPtrArray* pdrgpiVarTypMod = GPOS_NEW(m_memory_pool) IntPtrArray(m_memory_pool);
	
	List *plColnames = pintocl->colNames;
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		TargetEntry *target_entry = (TargetEntry *) gpdb::PvListNth(m_pquery->targetList, ul);
		if (target_entry->resjunk)
		{
			continue;
		}
		AttrNumber iResno = target_entry->resno;
		int iVarTypMod = gpdb::IExprTypeMod((Node*)target_entry->expr);
		pdrgpiVarTypMod->Append(GPOS_NEW(m_memory_pool) INT(iVarTypMod));

		CDXLNode *pdxlnCol = (*m_pdrgpdxlnQueryOutput)[ul];
		CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::Cast(pdxlnCol->GetOperator());
		pdrgpulSource->Append(GPOS_NEW(m_memory_pool) ULONG(pdxlopIdent->MakeDXLColRef()->Id()));
		
		CMDName *pmdnameCol = NULL;
		if (NULL != plColnames && ul < gpdb::ListLength(plColnames))
		{
			ColumnDef *pcoldef = (ColumnDef *) gpdb::PvListNth(plColnames, ul);
			pmdnameCol = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, pcoldef->colname);
		}
		else
		{
			pmdnameCol = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pdxlopIdent->MakeDXLColRef()->MdName()->GetMDName());
		}
		
		GPOS_ASSERT(NULL != pmdnameCol);
		IMDId *pmdid = pdxlopIdent->MDIdType();
		pmdid->AddRef();
		CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
											(
											m_memory_pool,
											pmdnameCol,
											m_pidgtorCol->next_id(),
											iResno /* iAttno */,
											pmdid,
											pdxlopIdent->TypeModifier(),
											false /* is_dropped */
											);
		pdrgpdxlcd->Append(dxl_col_descr);
	}

	IMDRelation::Ereldistrpolicy rel_distr_policy = IMDRelation::EreldistrRandom;
	ULongPtrArray *pdrgpulDistr = NULL;
	
	if (NULL != m_pquery->intoPolicy)
	{
		rel_distr_policy = CTranslatorRelcacheToDXL::GetRelDistribution(m_pquery->intoPolicy);
		
		if (IMDRelation::EreldistrHash == rel_distr_policy)
		{
			pdrgpulDistr = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

			for (ULONG ul = 0; ul < (ULONG) m_pquery->intoPolicy->nattrs; ul++)
			{
				AttrNumber attno = m_pquery->intoPolicy->attrs[ul];
				GPOS_ASSERT(0 < attno);
				pdrgpulDistr->Append(GPOS_NEW(m_memory_pool) ULONG(attno - 1));
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
	
	GPOS_ASSERT(IMDRelation::EreldistrMasterOnly != rel_distr_policy);
	m_fHasDistributedTables = true;

	// TODO: Mar 5, 2014; reserve an OID
	OID oid = 1;
	CMDIdGPDB *pmdid = GPOS_NEW(m_memory_pool) CMDIdGPDBCtas(oid);
	
	CMDName *pmdnameTableSpace = NULL;
	if (NULL != pintocl->tableSpaceName)
	{
		pmdnameTableSpace = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, pintocl->tableSpaceName);
	}
	
	CMDName *pmdnameSchema = NULL;
	if (NULL != pintocl->rel->schemaname)
	{
		pmdnameSchema = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, pintocl->rel->schemaname);
	}
	
	CDXLCtasStorageOptions::ECtasOnCommitAction ectascommit = (CDXLCtasStorageOptions::ECtasOnCommitAction) pintocl->onCommit;
	
	IMDRelation::Erelstoragetype rel_storage_type = IMDRelation::ErelstorageHeap;
	CDXLCtasStorageOptions::DXLCtasOptionArray *pdrgpctasopt = GetDXLCtasOptionArray(pintocl->options, &rel_storage_type);
	
	BOOL fHasOids = gpdb::FInterpretOidsOption(pintocl->options);
	CDXLLogicalCTAS *pdxlopCTAS = GPOS_NEW(m_memory_pool) CDXLLogicalCTAS
									(
									m_memory_pool,
									pmdid,
									pmdnameSchema,
									pmdnameRel, 
									pdrgpdxlcd, 
									GPOS_NEW(m_memory_pool) CDXLCtasStorageOptions(pmdnameTableSpace, ectascommit, pdrgpctasopt),
									rel_distr_policy,
									pdrgpulDistr,  
									pintocl->rel->relpersistence == RELPERSISTENCE_TEMP,
									fHasOids,
									rel_storage_type,
									pdrgpulSource,
									pdrgpiVarTypMod
									);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopCTAS, pdxlnQuery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::GetDXLCtasOptionArray
//
//	@doc:
//		Translate CTAS storage options
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::DXLCtasOptionArray *
CTranslatorQueryToDXL::GetDXLCtasOptionArray
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
	
	CDXLCtasStorageOptions::DXLCtasOptionArray *pdrgpctasopt = GPOS_NEW(m_memory_pool) CDXLCtasStorageOptions::DXLCtasOptionArray(m_memory_pool);
	ListCell *lc = NULL;
	BOOL fAO = false;
	BOOL fAOCO = false;
	BOOL fParquet = false;
	
	CWStringConst strAppendOnly(GPOS_WSZ_LIT("appendonly"));
	CWStringConst strOrientation(GPOS_WSZ_LIT("orientation"));
	CWStringConst strOrientationParquet(GPOS_WSZ_LIT("parquet"));
	CWStringConst strOrientationColumn(GPOS_WSZ_LIT("column"));
	
	ForEach (lc, plOptions)
	{
		DefElem *pdefelem = (DefElem *) lfirst(lc);
		CWStringDynamic *pstrName = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, pdefelem->defname);
		CWStringDynamic *pstrValue = NULL;
		
		BOOL fNullArg = (NULL == pdefelem->arg);

		// pdefelem->arg is NULL for queries of the form "create table t with (oids) as ... "
		if (fNullArg)
		{
			// we represent null options as an empty arg string and set the IsNull flag on
			pstrValue = GPOS_NEW(m_memory_pool) CWStringDynamic(m_memory_pool);
		}
		else
		{
			pstrValue = PstrExtractOptionValue(pdefelem);

			if (pstrName->Equals(&strAppendOnly) && pstrValue->Equals(CDXLTokens::GetDXLTokenStr(EdxltokenTrue)))
			{
				fAO = true;
			}
			
			if (pstrName->Equals(&strOrientation) && pstrValue->Equals(&strOrientationColumn))
			{
				GPOS_ASSERT(!fParquet);
				fAOCO = true;
			}

			if (pstrName->Equals(&strOrientation) && pstrValue->Equals(&strOrientationParquet))
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
				GPOS_NEW(m_memory_pool) CDXLCtasStorageOptions::CDXLCtasOption(argType, pstrName, pstrValue, fNullArg);
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

	CWStringDynamic *pstrResult = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, szValue);
	
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
	IMDId *pmdid = CTranslatorUtils::PmdidSystemColType(m_memory_pool, SelfItemPointerAttributeNumber);
	*pulCtid = CTranslatorUtils::GetColId(m_ulQueryLevel, m_pquery->resultRelation, SelfItemPointerAttributeNumber, pmdid, m_pmapvarcolid);
	pmdid->Release();

	// segmentid column id
	pmdid = CTranslatorUtils::PmdidSystemColType(m_memory_pool, GpSegmentIdAttributeNumber);
	*pulSegmentId = CTranslatorUtils::GetColId(m_ulQueryLevel, m_pquery->resultRelation, GpSegmentIdAttributeNumber, pmdid, m_pmapvarcolid);
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
	IMDId *pmdid = CTranslatorUtils::PmdidSystemColType(m_memory_pool, ObjectIdAttributeNumber);
	ULONG ulTupleOidColId = CTranslatorUtils::GetColId(m_ulQueryLevel, m_pquery->resultRelation, ObjectIdAttributeNumber, pmdid, m_pmapvarcolid);
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

	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);
	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::FRelHasTriggers(m_memory_pool, m_pmda, pmdrel, Edxldmldelete))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("DELETE with triggers"));
	}

	ULONG ctid_colid = 0;
	ULONG segid_colid = 0;
	GetCtidAndSegmentId(&ctid_colid, &segid_colid);

	ULongPtrArray *delete_colid_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	const ULONG ulRelColumns = pmdrel->ColumnCount();
	for (ULONG ul = 0; ul < ulRelColumns; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		if (pmdcol->IsSystemColumn() || pmdcol->IsDropped())
		{
			continue;
		}

		ULONG col_id = CTranslatorUtils::GetColId(m_ulQueryLevel, m_pquery->resultRelation, pmdcol->AttrNum(), pmdcol->MDIdType(), m_pmapvarcolid);
		delete_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	CDXLLogicalDelete *pdxlopdelete = GPOS_NEW(m_memory_pool) CDXLLogicalDelete(m_memory_pool, table_descr, ctid_colid, segid_colid, delete_colid_array);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopdelete, pdxlnQuery);
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

	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);
	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::FRelHasTriggers(m_memory_pool, m_pmda, pmdrel, Edxldmlupdate))
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
	

	BOOL fHasOids = pmdrel->HasOids();
	if (fHasOids)
	{
		ulTupleOidColId = UlTupleOidColId();
	}

	// get (resno -> colId) mapping of columns to be updated
	IntUlongHashMap *phmiulUpdateCols = PhmiulUpdateCols();

	const ULONG ulRelColumns = pmdrel->ColumnCount();
	ULongPtrArray *insert_colid_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	ULongPtrArray *delete_colid_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	for (ULONG ul = 0; ul < ulRelColumns; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		if (pmdcol->IsSystemColumn() || pmdcol->IsDropped())
		{
			continue;
		}

		INT iAttno = pmdcol->AttrNum();
		ULONG *pulColId = phmiulUpdateCols->Find(&iAttno);

		ULONG col_id = CTranslatorUtils::GetColId(m_ulQueryLevel, m_pquery->resultRelation, iAttno, pmdcol->MDIdType(), m_pmapvarcolid);

		// if the column is in the query outputs then use it
		// otherwise get the column id created by the child query
		if (NULL != pulColId)
		{
			insert_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(*pulColId));
		}
		else
		{
			insert_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
		}

		delete_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	phmiulUpdateCols->Release();
	CDXLLogicalUpdate *pdxlopupdate = GPOS_NEW(m_memory_pool) CDXLLogicalUpdate(m_memory_pool, table_descr, ulCtidColId, ulSegmentIdColId, delete_colid_array, insert_colid_array, fHasOids, ulTupleOidColId);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopupdate, pdxlnQuery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PhmiulUpdateCols
//
//	@doc:
// 		Return resno -> colId mapping of columns to be updated
//
//---------------------------------------------------------------------------
IntUlongHashMap *
CTranslatorQueryToDXL::PhmiulUpdateCols()
{
	IntUlongHashMap *phmiulUpdateCols = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);

	ListCell *lc = NULL;
	ULONG ul = 0;
	ULONG ulOutputCols = 0;
	ForEach (lc, m_pquery->targetList)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		ULONG ulResno = target_entry->resno;
		GPOS_ASSERT(0 < ulResno);

		// resjunk true columns may be now existing in the query tree, for instance
		// ctid column in case of relations, see rewriteTargetListUD in GPDB.
		// In ORCA, resjunk true columns (ex ctid) required to identify the tuple
		// are included later, so, its safe to not include them here in the output query list.
		// In planner, a MODIFYTABLE node is created on top of the plan instead of DML node,
		// once we plan generating MODIFYTABLE node from ORCA, we may revisit it.
		if (!target_entry->resjunk)
		{
			CDXLNode *pdxlnCol = (*m_pdrgpdxlnQueryOutput)[ul];
			CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::Cast(
					pdxlnCol->GetOperator());
			ULONG col_id = pdxlopIdent->MakeDXLColRef()->Id();

			StoreAttnoColIdMapping(phmiulUpdateCols, ulResno, col_id);
			ulOutputCols++;
		}
		ul++;
	}

	GPOS_ASSERT(ulOutputCols == m_pdrgpdxlnQueryOutput->Size());
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
	ULONG size
	)
{
	BOOL fFound = false;
	for (ULONG ul = 0; !fFound && ul < size; ul++)
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
	if (EdxlopScalarWindowRef == pdxlop->GetDXLOperator())
	{
		CDXLScalarWindowRef *pdxlopWinref = CDXLScalarWindowRef::Cast(pdxlop);
		const CMDIdGPDB *pmdidgpdb = CMDIdGPDB::CastMdid(pdxlopWinref->FuncMdId());
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
	if (EdxlopScalarWindowRef == pdxlop->GetDXLOperator())
	{
		CDXLScalarWindowRef *pdxlopWinref = CDXLScalarWindowRef::Cast(pdxlop);
		const CMDIdGPDB *pmdidgpdb = CMDIdGPDB::CastMdid(pdxlopWinref->FuncMdId());
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
		pdxlnLeadEdge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, true /* fLeading */, edxlfbLead));
		pdxlnTrailEdge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, false /* fLeading */, edxlfbTrail));

		pdxlnLeadEdge->AddChild(CTranslatorUtils::PdxlnInt8Const(m_memory_pool, m_pmda, 1 /*iVal*/));
		pdxlnTrailEdge->AddChild(CTranslatorUtils::PdxlnInt8Const(m_memory_pool, m_pmda, 1 /*iVal*/));
	}
	else
	{
		// overwrite frame edge types based on specified offset type
		if (EdxlopScalarConstValue != pdxlnOffset->GetOperator()->GetDXLOperator())
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
		pdxlnLeadEdge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, true /* fLeading */, edxlfbLead));
		pdxlnTrailEdge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, false /* fLeading */, edxlfbTrail));

		pdxlnOffset->AddRef();
		pdxlnLeadEdge->AddChild(pdxlnOffset);
		pdxlnOffset->AddRef();
		pdxlnTrailEdge->AddChild(pdxlnOffset);
	}

	// manufacture a frame for LEAD/LAG function
	return GPOS_NEW(m_memory_pool) CDXLWindowFrame
							(
							m_memory_pool,
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
	CDXLNode *project_list_dxl, // project list holding WinRef nodes
	DXLWindowSpecArray *window_spec_array // original list of window spec
	)
	const
{
	GPOS_ASSERT(NULL != project_list_dxl);
	GPOS_ASSERT(NULL != window_spec_array);

	const ULONG arity = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnChild = (*(*project_list_dxl)[ul])[0];
		CDXLOperator *pdxlop = pdxlnChild->GetOperator();
		BOOL fLead = FLeadWindowFunc(pdxlop);
		BOOL fLag = FLagWindowFunc(pdxlop);
		if (fLead || fLag)
		{
			CDXLScalarWindowRef *pdxlopWinref = CDXLScalarWindowRef::Cast(pdxlop);
			CDXLWindowSpec *pdxlws = (*window_spec_array)[pdxlopWinref->GetWindSpecPos()];
			CMDName *mdname = NULL;
			if (NULL != pdxlws->MdName())
			{
				mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pdxlws->MdName()->GetMDName());
			}

			// find if an offset is specified
			CDXLNode *pdxlnOffset = NULL;
			if (1 < pdxlnChild->Arity())
			{
				pdxlnOffset = (*pdxlnChild)[1];
			}

			// create LEAD/LAG frame
			CDXLWindowFrame *window_frame = PdxlwfLeadLag(fLead, pdxlnOffset);

			// create new window spec object
			pdxlws->GetPartitionByColIdArray()->AddRef();
			pdxlws->GetSortColListDXL()->AddRef();
			CDXLWindowSpec *pdxlwsNew =
				GPOS_NEW(m_memory_pool) CDXLWindowSpec
					(
					m_memory_pool,
					pdxlws->GetPartitionByColIdArray(),
					mdname,
					pdxlws->GetSortColListDXL(),
					window_frame
					);
			window_spec_array->Append(pdxlwsNew);

			// update win spec pos of LEAD/LAG function
			pdxlopWinref->SetWinSpecPos(window_spec_array->Size() - 1);
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
DXLWindowSpecArray *
CTranslatorQueryToDXL::Pdrgpdxlws
	(
	List *plWindowClause,
	IntUlongHashMap *phmiulSortColsColId,
	CDXLNode *pdxlnScPrL
	)
{
	GPOS_ASSERT(NULL != plWindowClause);
	GPOS_ASSERT(NULL != phmiulSortColsColId);
	GPOS_ASSERT(NULL != pdxlnScPrL);

	DXLWindowSpecArray *window_spec_array = GPOS_NEW(m_memory_pool) DXLWindowSpecArray(m_memory_pool);

	// translate window specification
	ListCell *plcWindowCl;
	ForEach (plcWindowCl, plWindowClause)
	{
		WindowClause *pwc = (WindowClause *) lfirst(plcWindowCl);
		ULongPtrArray *pdrgppulPartCol = PdrgpulPartCol(pwc->partitionClause, phmiulSortColsColId);

		CDXLNode *sort_col_list_dxl = NULL;
		CMDName *mdname = NULL;
		CDXLWindowFrame *window_frame = NULL;

		if (NULL != pwc->name)
		{
			CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, pwc->name);
			mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
			GPOS_DELETE(pstrAlias);
		}

		if (0 < gpdb::ListLength(pwc->orderClause))
		{
			// create a sorting col list
			sort_col_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSortColList(m_memory_pool));

			DXLNodeArray *pdrgpdxlnSortCol = PdrgpdxlnSortCol(pwc->orderClause, phmiulSortColsColId);
			const ULONG size = pdrgpdxlnSortCol->Size();
			for (ULONG ul = 0; ul < size; ul++)
			{
				CDXLNode *pdxlnSortClause = (*pdrgpdxlnSortCol)[ul];
				pdxlnSortClause->AddRef();
				sort_col_list_dxl->AddChild(pdxlnSortClause);
			}
			pdrgpdxlnSortCol->Release();
		}

		window_frame = m_psctranslator->GetWindowFrame(pwc->frameOptions,
						 pwc->startOffset,
						 pwc->endOffset,
						 m_pmapvarcolid,
						 pdxlnScPrL,
						 &m_fHasDistributedTables);

		CDXLWindowSpec *pdxlws = GPOS_NEW(m_memory_pool) CDXLWindowSpec(m_memory_pool, pdrgppulPartCol, mdname, sort_col_list_dxl, window_frame);
		window_spec_array->Append(pdxlws);
	}

	return window_spec_array;
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
	List *target_list,
	List *plWindowClause,
	List *plSortClause,
	IntUlongHashMap *phmiulSortColsColId,
	IntUlongHashMap *phmiulOutputCols
	)
{
	if (0 == gpdb::ListLength(plWindowClause))
	{
		return pdxlnChild;
	}

	// translate target list entries
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	CDXLNode *pdxlnNewChildScPrL = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));
	ListCell *plcTE = NULL;
	ULONG ulResno = 1;

	// target entries that are result of flattening join alias and 
	// are equivalent to a defined Window specs target entry
	List *plTEOmitted = NIL; 
	List *plResno = NIL;
	
	ForEach (plcTE, target_list)
	{
		BOOL fInsertSortInfo = true;
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		// create the DXL node holding the target list entry
		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr(target_entry->expr, target_entry->resname);
		ULONG col_id = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator())->Id();

		if (IsA(target_entry->expr, WindowFunc))
		{
			CTranslatorUtils::CheckAggregateWindowFn((Node*) target_entry->expr);
		}
		if (!target_entry->resjunk)
		{
			if (IsA(target_entry->expr, Var) || IsA(target_entry->expr, WindowFunc))
			{
				// add window functions and non-computed columns to the project list of the window operator
				project_list_dxl->AddChild(pdxlnPrEl);

				StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);
			}
			else if (CTranslatorUtils::FWindowSpec(target_entry, plWindowClause))
			{
				// add computed column used in window specification needed in the output columns
				// to the child's project list
				pdxlnNewChildScPrL->AddChild(pdxlnPrEl);

				// construct a scalar identifier that points to the computed column and
				// add it to the project list of the window operator
				CMDName *pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName
													(
													m_memory_pool,
													CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator())->GetMdNameAlias()->GetMDName()
													);
				CDXLNode *pdxlnPrElNew = GPOS_NEW(m_memory_pool) CDXLNode
													(
													m_memory_pool,
													GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, col_id, pmdnameAlias)
													);
				CDXLNode *pdxlnPrElNewChild = GPOS_NEW(m_memory_pool) CDXLNode
															(
															m_memory_pool,
															GPOS_NEW(m_memory_pool) CDXLScalarIdent
																		(
																		m_memory_pool,
																		GPOS_NEW(m_memory_pool) CDXLColRef
																					(
																					m_memory_pool,
																					GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pmdnameAlias->GetMDName()),
																					col_id,
																					GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::OidExprType((Node*) target_entry->expr)),
																					gpdb::IExprTypeMod((Node*) target_entry->expr)
																					)
																		)
															);
				pdxlnPrElNew->AddChild(pdxlnPrElNewChild);
				project_list_dxl->AddChild(pdxlnPrElNew);

				StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);
			}
			else
			{
				fInsertSortInfo = false;
				plTEOmitted = gpdb::PlAppendElement(plTEOmitted, target_entry);
				plResno = gpdb::PlAppendInt(plResno, ulResno);

				pdxlnPrEl->Release();
			}
		}
		else if (IsA(target_entry->expr, WindowFunc))
		{
			// computed columns used in the order by clause
			project_list_dxl->AddChild(pdxlnPrEl);
		}
		else if (!IsA(target_entry->expr, Var))
		{
			GPOS_ASSERT(CTranslatorUtils::FWindowSpec(target_entry, plWindowClause));
			// computed columns used in the window specification
			pdxlnNewChildScPrL->AddChild(pdxlnPrEl);
		}
		else
		{
			pdxlnPrEl->Release();
		}

		if (fInsertSortInfo)
		{
			AddSortingGroupingColumn(target_entry, phmiulSortColsColId, col_id);
		}

		ulResno++;
	}

	plcTE = NULL;

	// process target entries that are a result of flattening join alias
	ListCell *plcResno = NULL;
	ForBoth (plcTE, plTEOmitted,
			plcResno, plResno)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		INT iResno = (INT) lfirst_int(plcResno);

		TargetEntry *pteWindowSpec = CTranslatorUtils::PteWindowSpec( (Node*) target_entry->expr, plWindowClause, target_list);
		if (NULL != pteWindowSpec)
		{
			const ULONG col_id = CTranslatorUtils::GetColId( (INT) pteWindowSpec->ressortgroupref, phmiulSortColsColId);
			StoreAttnoColIdMapping(phmiulOutputCols, iResno, col_id);
			AddSortingGroupingColumn(target_entry, phmiulSortColsColId, col_id);
		}
	}
	if (NIL != plTEOmitted)
	{
		gpdb::GPDBFree(plTEOmitted);
	}

	// translate window spec
	DXLWindowSpecArray *window_spec_array = Pdrgpdxlws(plWindowClause, phmiulSortColsColId, pdxlnNewChildScPrL);

	CDXLNode *pdxlnNewChild = NULL;

	if (0 < pdxlnNewChildScPrL->Arity())
	{
		// create a project list for the computed columns used in the window specification
		pdxlnNewChild = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		pdxlnNewChild->AddChild(pdxlnNewChildScPrL);
		pdxlnNewChild->AddChild(pdxlnChild);
		pdxlnChild = pdxlnNewChild;
	}
	else
	{
		// clean up
		pdxlnNewChildScPrL->Release();
	}

	if (!CTranslatorUtils::FHasProjElem(project_list_dxl, EdxlopScalarWindowRef))
	{
		project_list_dxl->Release();
		window_spec_array->Release();

		return pdxlnChild;
	}

	// update window spec positions of LEAD/LAG functions
	UpdateLeadLagWinSpecPos(project_list_dxl, window_spec_array);

	CDXLLogicalWindow *pdxlopWindow = GPOS_NEW(m_memory_pool) CDXLLogicalWindow(m_memory_pool, window_spec_array);
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopWindow);

	dxlnode->AddChild(project_list_dxl);
	dxlnode->AddChild(pdxlnChild);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpulPartCol
//
//	@doc:
//		Translate the list of partition-by column identifiers
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorQueryToDXL::PdrgpulPartCol
	(
	List *plPartCl,
	IntUlongHashMap *phmiulColColId
	)
	const
{
	ULongPtrArray *pdrgpul = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	ListCell *plcPartCl = NULL;
	ForEach (plcPartCl, plPartCl)
	{
		Node *pnodePartCl = (Node*) lfirst(plcPartCl);
		GPOS_ASSERT(NULL != pnodePartCl);

		GPOS_ASSERT(IsA(pnodePartCl, SortGroupClause));
		SortGroupClause *psortcl = (SortGroupClause *) pnodePartCl;

		// get the colid of the partition-by column
		ULONG col_id = CTranslatorUtils::GetColId((INT) psortcl->tleSortGroupRef, phmiulColColId);

		pdrgpul->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
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
DXLNodeArray *
CTranslatorQueryToDXL::PdrgpdxlnSortCol
	(
	List *plSortCl,
	IntUlongHashMap *phmiulColColId
	)
	const
{
	DXLNodeArray *pdrgpdxln = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

	ListCell *plcSortCl = NULL;
	ForEach (plcSortCl, plSortCl)
	{
		Node *pnodeSortCl = (Node*) lfirst(plcSortCl);
		GPOS_ASSERT(NULL != pnodeSortCl);

		GPOS_ASSERT(IsA(pnodeSortCl, SortGroupClause));

		SortGroupClause *psortcl = (SortGroupClause *) pnodeSortCl;

		// get the colid of the sorting column
		const ULONG col_id = CTranslatorUtils::GetColId((INT) psortcl->tleSortGroupRef, phmiulColColId);

		OID oid = psortcl->sortop;

		// get operator name
		CMDIdGPDB *pmdidScOp = GPOS_NEW(m_memory_pool) CMDIdGPDB(oid);
		const IMDScalarOp *md_scalar_op = m_pmda->Pmdscop(pmdidScOp);

		const CWStringConst *str = md_scalar_op->Mdname().GetMDName();
		GPOS_ASSERT(NULL != str);

		CDXLScalarSortCol *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarSortCol
												(
												m_memory_pool,
												col_id,
												pmdidScOp,
												GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()),
												psortcl->nulls_first
												);

		// create the DXL node holding the sorting col
		CDXLNode *pdxlnSortCol = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

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
	IntUlongHashMap *phmiulGrpColsColId
	)
{
	if (0 == gpdb::ListLength(plSortCl) && NULL == pnodeLimitCount && NULL == pnodeLimitOffset)
	{
		return pdxlnChild;
	}

	// do not remove limit if it is immediately under a DML (JIRA: GPSQL-2669)
	// otherwise we may increase the storage size because there are less opportunities for compression
	BOOL fTopLevelLimit = (m_fTopDMLQuery && 1 == m_ulQueryLevel) || (m_fCTASQuery && 0 == m_ulQueryLevel);
	CDXLNode *pdxlnLimit =
			GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalLimit(m_memory_pool, fTopLevelLimit));

	// create a sorting col list
	CDXLNode *sort_col_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSortColList(m_memory_pool));

	DXLNodeArray *pdrgpdxlnSortCol = PdrgpdxlnSortCol(plSortCl, phmiulGrpColsColId);
	const ULONG size = pdrgpdxlnSortCol->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CDXLNode *pdxlnSortCol = (*pdrgpdxlnSortCol)[ul];
		pdxlnSortCol->AddRef();
		sort_col_list_dxl->AddChild(pdxlnSortCol);
	}
	pdrgpdxlnSortCol->Release();

	// create limit count
	CDXLNode *pdxlnLimitCount = GPOS_NEW(m_memory_pool) CDXLNode
										(
										m_memory_pool,
										GPOS_NEW(m_memory_pool) CDXLScalarLimitCount(m_memory_pool)
										);

	if (NULL != pnodeLimitCount)
	{
		pdxlnLimitCount->AddChild(PdxlnScFromGPDBExpr((Expr*) pnodeLimitCount));
	}

	// create limit offset
	CDXLNode *pdxlnLimitOffset = GPOS_NEW(m_memory_pool) CDXLNode
										(
										m_memory_pool,
										GPOS_NEW(m_memory_pool) CDXLScalarLimitOffset(m_memory_pool)
										);

	if (NULL != pnodeLimitOffset)
	{
		pdxlnLimitOffset->AddChild(PdxlnScFromGPDBExpr((Expr*) pnodeLimitOffset));
	}

	pdxlnLimit->AddChild(sort_col_list_dxl);
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
	TargetEntry *target_entry,
	IntUlongHashMap *phmiulSortgrouprefColId,
	ULONG col_id
	)
	const
{
	if (0 < target_entry->ressortgroupref)
	{
		INT *piKey = GPOS_NEW(m_memory_pool) INT(target_entry->ressortgroupref);
		ULONG *pulValue = GPOS_NEW(m_memory_pool) ULONG(col_id);

		// insert idx-colid mapping in the hash map
#ifdef GPOS_DEBUG
		BOOL fRes =
#endif // GPOS_DEBUG
				phmiulSortgrouprefColId->Insert(piKey, pulValue);

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
	List *target_list,
	List *plGroupClause,
	CBitSet *pbsGroupByCols,
	BOOL fHasAggs,
	BOOL fGroupingSets,
	CDXLNode *pdxlnChild,
	IntUlongHashMap *phmiulSortgrouprefColId,
	IntUlongHashMap *phmiulChild,
	IntUlongHashMap *phmiulOutputCols
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
			while (mi.Advance())
			{
	#ifdef GPOS_DEBUG
				BOOL result =
	#endif // GPOS_DEBUG
				phmiulOutputCols->Insert(GPOS_NEW(m_memory_pool) INT(*(mi.Key())), GPOS_NEW(m_memory_pool) ULONG(*(mi.Value())));
				GPOS_ASSERT(result);
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
	CDXLNode *pdxlnPrLGrpBy = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	ListCell *plcTE = NULL;
	ULONG ulDQAs = 0;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG ulResNo = target_entry->resno;

		TargetEntry *pteEquivalent = CTranslatorUtils::PteGroupingColumn( (Node *) target_entry->expr, plGroupClause, target_list);

		BOOL fGroupingCol = pbsGroupByCols->Get(target_entry->ressortgroupref) || (NULL != pteEquivalent && pbsGroupByCols->Get(pteEquivalent->ressortgroupref));
		ULONG col_id = 0;

		if (fGroupingCol)
		{
			// find colid for grouping column
			col_id = CTranslatorUtils::GetColId(ulResNo, phmiulChild);
		}
		else if (IsA(target_entry->expr, Aggref))
		{
			if (IsA(target_entry->expr, Aggref) && ((Aggref *) target_entry->expr)->aggdistinct && !FDuplicateDqaArg(plDQA, (Aggref *) target_entry->expr))
			{
				plDQA = gpdb::PlAppendElement(plDQA, gpdb::PvCopyObject(target_entry->expr));
				ulDQAs++;
			}

			// create a project element for aggregate
			CDXLNode *pdxlnPrEl = PdxlnPrEFromGPDBExpr(target_entry->expr, target_entry->resname);
			pdxlnPrLGrpBy->AddChild(pdxlnPrEl);
			col_id = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator())->Id();
			AddSortingGroupingColumn(target_entry, phmiulSortgrouprefColId, col_id);
		}

		if (fGroupingCol || IsA(target_entry->expr, Aggref))
		{
			// add to the list of output columns
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, col_id);
		}
		else if (0 == pbsGroupByCols->Size() && !fGroupingSets && !fHasAggs)
		{
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, col_id);
		}
	}

	if (1 < ulDQAs && !optimizer_enable_multiple_distinct_aggs)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple Distinct Qualified Aggregates are disabled in the optimizer"));
	}

	// initialize the array of grouping columns
	ULongPtrArray *pdrgpul = CTranslatorUtils::GetGroupingColidArray(m_memory_pool, pbsGroupByCols, phmiulSortgrouprefColId);

	// clean up
	if (NIL != plDQA)
	{
		gpdb::FreeList(plDQA);
	}

	return GPOS_NEW(m_memory_pool) CDXLNode
						(
						m_memory_pool,
						GPOS_NEW(m_memory_pool) CDXLLogicalGroupBy(m_memory_pool, pdrgpul),
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

	if (NIL == plDQA || 0 == gpdb::ListLength(plDQA))
	{
		return false;
	}

	ListCell *lc = NULL;
	ForEach (lc, plDQA)
	{
		Node *pnode = (Node *) lfirst(lc);
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
	List *target_list,
	List *plGroupClause,
	BOOL fHasAggs,
	IntUlongHashMap *phmiulSortgrouprefColId,
	IntUlongHashMap *phmiulOutputCols
	)
{
	const ULONG ulCols = gpdb::ListLength(target_list) + 1;

	if (NULL == plGroupClause)
	{
		IntUlongHashMap *phmiulChild = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);

		CDXLNode *pdxlnSPJ = PdxlnSPJ(target_list, pfromexpr, phmiulSortgrouprefColId, phmiulChild, plGroupClause);

		CBitSet *pbs = NULL;
		if (fHasAggs)
		{ 
			pbs = GPOS_NEW(m_memory_pool) CBitSet(m_memory_pool);
		}
		
		// in case of aggregates, construct a group by operator
		CDXLNode *pdxlnResult = PdxlnSimpleGroupBy
								(
								target_list,
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
	UlongUlongHashMap *phmululGrpColPos = GPOS_NEW(m_memory_pool) UlongUlongHashMap(m_memory_pool);
	CBitSet *pbsUniqueueGrpCols = GPOS_NEW(m_memory_pool) CBitSet(m_memory_pool, ulCols);
	DrgPbs *pdrgpbs = CTranslatorUtils::PdrgpbsGroupBy(m_memory_pool, plGroupClause, ulCols, phmululGrpColPos, pbsUniqueueGrpCols);

	const ULONG ulGroupingSets = pdrgpbs->Size();

	if (1 == ulGroupingSets)
	{
		// simple group by
		IntUlongHashMap *phmiulChild = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
		CDXLNode *pdxlnSPJ = PdxlnSPJ(target_list, pfromexpr, phmiulSortgrouprefColId, phmiulChild, plGroupClause);

		// translate the groupby clauses into a logical group by operator
		CBitSet *pbs = (*pdrgpbs)[0];


		CDXLNode *pdxlnGroupBy = PdxlnSimpleGroupBy
								(
								target_list,
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
									target_list,
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
			target_list,
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
	List *target_list,
	List *plGroupClause,
	BOOL fHasAggs,
	DrgPbs *pdrgpbs,
	IntUlongHashMap *phmiulSortgrouprefColId,
	IntUlongHashMap *phmiulOutputCols,
	UlongUlongHashMap *phmululGrpColPos		// mapping pos->unique grouping columns for grouping func arguments
	)
{
	GPOS_ASSERT(NULL != pdrgpbs);
	GPOS_ASSERT(1 < pdrgpbs->Size());

	const ULONG ulGroupingSets = pdrgpbs->Size();
	CDXLNode *pdxlnUnionAll = NULL;
	ULongPtrArray *pdrgpulColIdsInner = NULL;

	const ULONG ulCTEId = m_pidgtorCTE->next_id();
	
	// construct a CTE producer on top of the SPJ query
	IntUlongHashMap *phmiulSPJ = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
	IntUlongHashMap *phmiulSortgrouprefColIdProducer = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
	CDXLNode *pdxlnSPJ = PdxlnSPJForGroupingSets(target_list, pfromexpr, phmiulSortgrouprefColIdProducer, phmiulSPJ, plGroupClause);

	// construct output colids
	ULongPtrArray *pdrgpulCTEProducer = PdrgpulExtractColIds(m_memory_pool, phmiulSPJ);

	GPOS_ASSERT (NULL != m_pdrgpdxlnCTE);
	
	CDXLLogicalCTEProducer *pdxlopCTEProducer = GPOS_NEW(m_memory_pool) CDXLLogicalCTEProducer(m_memory_pool, ulCTEId, pdrgpulCTEProducer);
	CDXLNode *pdxlnCTEProducer = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopCTEProducer, pdxlnSPJ);
	m_pdrgpdxlnCTE->Append(pdxlnCTEProducer);
	
	CMappingVarColId *pmapvarcolidOriginal = m_pmapvarcolid->CopyMapColId(m_memory_pool);
	
	for (ULONG ul = 0; ul < ulGroupingSets; ul++)
	{
		CBitSet *pbsGroupingSet = (*pdrgpbs)[ul];

		// remap columns
		ULongPtrArray *pdrgpulCTEConsumer = PdrgpulGenerateColIds(m_memory_pool, pdrgpulCTEProducer->Size());
		
		// reset col mapping with new consumer columns
		GPOS_DELETE(m_pmapvarcolid);
		m_pmapvarcolid = pmapvarcolidOriginal->CopyRemapColId(m_memory_pool, pdrgpulCTEProducer, pdrgpulCTEConsumer);
		
		IntUlongHashMap *phmiulSPJConsumer = PhmiulRemapColIds(m_memory_pool, phmiulSPJ, pdrgpulCTEProducer, pdrgpulCTEConsumer);
		IntUlongHashMap *phmiulSortgrouprefColIdConsumer = PhmiulRemapColIds(m_memory_pool, phmiulSortgrouprefColIdProducer, pdrgpulCTEProducer, pdrgpulCTEConsumer);

		// construct a CTE consumer
		CDXLNode *pdxlnCTEConsumer = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalCTEConsumer(m_memory_pool, ulCTEId, pdrgpulCTEConsumer));

		IntUlongHashMap *phmiulGroupBy = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
		CDXLNode *pdxlnGroupBy = PdxlnSimpleGroupBy
					(
					target_list,
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
		CDXLNode *pdxlnProject = PdxlnProjectNullsForGroupingSets(target_list, pdxlnGroupBy, pbsGroupingSet, phmiulSortgrouprefColIdConsumer, phmiulGroupBy, phmululGrpColPos);

		ULongPtrArray *pdrgpulColIdsOuter = CTranslatorUtils::GetOutputColIdsArray(m_memory_pool, target_list, phmiulGroupBy);
		if (NULL != pdxlnUnionAll)
		{
			GPOS_ASSERT(NULL != pdrgpulColIdsInner);
			ColumnDescrDXLArray *pdrgpdxlcd = CTranslatorUtils::GetColumnDescrDXLArray(m_memory_pool, target_list, pdrgpulColIdsOuter, true /* fKeepResjunked */);

			pdrgpulColIdsOuter->AddRef();

			ULongPtrArray2D *pdrgpdrgulInputColIds = GPOS_NEW(m_memory_pool) ULongPtrArray2D(m_memory_pool);
			pdrgpdrgulInputColIds->Append(pdrgpulColIdsOuter);
			pdrgpdrgulInputColIds->Append(pdrgpulColIdsInner);

			CDXLLogicalSetOp *pdxlopSetop = GPOS_NEW(m_memory_pool) CDXLLogicalSetOp(m_memory_pool, EdxlsetopUnionAll, pdrgpdxlcd, pdrgpdrgulInputColIds, false);
			pdxlnUnionAll = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopSetop, pdxlnProject, pdxlnUnionAll);
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
			ForEach (plcTE, target_list)
			{
				TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);

				INT iSortGrpRef = INT (target_entry->ressortgroupref);
				if (0 < iSortGrpRef && NULL != phmiulSortgrouprefColIdConsumer->Find(&iSortGrpRef))
				{
					// add the mapping information for sorting columns
					AddSortingGroupingColumn(target_entry, phmiulSortgrouprefColId, *(*pdrgpulColIdsInner)[ulTargetEntryPos]);
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
	CDXLLogicalSetOp *pdxlopUnion = CDXLLogicalSetOp::Cast(pdxlnUnionAll->GetOperator());

	ListCell *plcTE = NULL;
	ULONG ulOutputColIndex = 0;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG ulResNo = target_entry->resno;

		// note that all target list entries are kept in union all's output column
		// this is achieved by the fKeepResjunked flag in CTranslatorUtils::GetColumnDescrDXLArray
		const CDXLColDescr *dxl_col_descr = pdxlopUnion->GetColumnDescrAt(ulOutputColIndex);
		const ULONG col_id = dxl_col_descr->Id();
		ulOutputColIndex++;

		if (!target_entry->resjunk)
		{
			// add non-resjunk columns to the hash map that maintains the output columns
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, col_id);
		}
	}

	// cleanup
	pdrgpbs->Release();

	// construct a CTE anchor operator on top of the union all
	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalCTEAnchor(m_memory_pool, ulCTEId), pdxlnUnionAll);
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
	ColumnDescrDXLArray *pdrgpdxlcd = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);

	const CMDTypeBoolGPDB *pmdtypeBool = dynamic_cast<const CMDTypeBoolGPDB *>(m_pmda->PtMDType<IMDTypeBool>(m_sysid));
	const CMDIdGPDB *pmdid = CMDIdGPDB::CastMdid(pmdtypeBool->MDId());

	// empty column name
	CWStringConst strUnnamedCol(GPOS_WSZ_LIT(""));
	CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol);
	CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
										(
										m_memory_pool,
										mdname,
										m_pidgtorCol->next_id(),
										1 /* iAttno */,
										GPOS_NEW(m_memory_pool) CMDIdGPDB(pmdid->OidObjectId()),
										default_type_modifier,
										false /* is_dropped */
										);
	pdrgpdxlcd->Append(dxl_col_descr);

	// create the array of datum arrays
	DXLDatumArrays *pdrgpdrgpdxldatum = GPOS_NEW(m_memory_pool) DXLDatumArrays(m_memory_pool);
	
	// create a datum array
	DXLDatumArray *pdrgpdxldatum = GPOS_NEW(m_memory_pool) DXLDatumArray(m_memory_pool);

	Const *pconst = (Const*) gpdb::PnodeMakeBoolConst(true /*value*/, false /*isnull*/);
	CDXLDatum *datum_dxl = m_psctranslator->GetDatumVal(pconst);
	gpdb::GPDBFree(pconst);

	pdrgpdxldatum->Append(datum_dxl);
	pdrgpdrgpdxldatum->Append(pdrgpdxldatum);

	CDXLLogicalConstTable *pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalConstTable(m_memory_pool, pdrgpdxlcd, pdrgpdrgpdxldatum);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);
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
	List *target_list,
	IntUlongHashMap *phmiulOutputCols
	)
{
	GPOS_ASSERT(IsA(pnodeSetOp, SetOperationStmt));
	SetOperationStmt *psetopstmt = (SetOperationStmt*) pnodeSetOp;
	GPOS_ASSERT(SETOP_NONE != psetopstmt->op);

	EdxlSetOpType edxlsetop = CTranslatorUtils::GetSetOpType(psetopstmt->op, psetopstmt->all);

	// translate the left and right child
	ULongPtrArray *pdrgpulLeft = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	ULongPtrArray *pdrgpulRight = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	MdidPtrArray *pdrgpmdidLeft = GPOS_NEW(m_memory_pool) MdidPtrArray(m_memory_pool);
	MdidPtrArray *pdrgpmdidRight = GPOS_NEW(m_memory_pool) MdidPtrArray(m_memory_pool);

	CDXLNode *pdxlnLeftChild = PdxlnSetOpChild(psetopstmt->larg, pdrgpulLeft, pdrgpmdidLeft, target_list);
	CDXLNode *pdxlnRightChild = PdxlnSetOpChild(psetopstmt->rarg, pdrgpulRight, pdrgpmdidRight, target_list);

	// mark outer references in input columns from left child
	ULONG *pulColId = GPOS_NEW_ARRAY(m_memory_pool, ULONG, pdrgpulLeft->Size());
	BOOL *pfOuterRef = GPOS_NEW_ARRAY(m_memory_pool, BOOL, pdrgpulLeft->Size());
	const ULONG size = pdrgpulLeft->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		pulColId[ul] =  *(*pdrgpulLeft)[ul];
		pfOuterRef[ul] = true;
	}
	CTranslatorUtils::MarkOuterRefs(pulColId, pfOuterRef, size, pdxlnLeftChild);

	ULongPtrArray2D *pdrgpdrgulInputColIds = GPOS_NEW(m_memory_pool) ULongPtrArray2D(m_memory_pool);
	pdrgpdrgulInputColIds->Append(pdrgpulLeft);
	pdrgpdrgulInputColIds->Append(pdrgpulRight);
	
	ULongPtrArray *pdrgpulOutput =  CTranslatorUtils::PdrgpulGenerateColIds
												(
												m_memory_pool,
												target_list,
												pdrgpmdidLeft,
												pdrgpulLeft,
												pfOuterRef,
												m_pidgtorCol
												);
 	GPOS_ASSERT(pdrgpulOutput->Size() == pdrgpulLeft->Size());

 	GPOS_DELETE_ARRAY(pulColId);
 	GPOS_DELETE_ARRAY(pfOuterRef);

	BOOL fCastAcrossInput = FCast(target_list, pdrgpmdidLeft) || FCast(target_list, pdrgpmdidRight);
	
	DXLNodeArray *pdrgpdxlnChildren  = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
	pdrgpdxlnChildren->Append(pdxlnLeftChild);
	pdrgpdxlnChildren->Append(pdxlnRightChild);

	CDXLNode *dxlnode = PdxlnSetOp
						(
						edxlsetop,
						target_list,
						pdrgpulOutput,
						pdrgpdrgulInputColIds,
						pdrgpdxlnChildren,
						fCastAcrossInput,
						false /* fKeepResjunked */
						);

	CDXLLogicalSetOp *pdxlop = CDXLLogicalSetOp::Cast(dxlnode->GetOperator());
	const ColumnDescrDXLArray *pdrgpdxlcd = pdxlop->GetColumnDescrDXLArray();

	ULONG ulOutputColIndex = 0;
	ListCell *plcTE = NULL;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG ulResNo = target_entry->resno;

		if (!target_entry->resjunk)
		{
			const CDXLColDescr *pdxlcdNew = (*pdrgpdxlcd)[ulOutputColIndex];
			ULONG col_id = pdxlcdNew->Id();
			StoreAttnoColIdMapping(phmiulOutputCols, ulResNo, col_id);
			ulOutputColIndex++;
		}
	}

	// clean up
	pdrgpulOutput->Release();
	pdrgpmdidLeft->Release();
	pdrgpmdidRight->Release();

	return dxlnode;
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
	ULongPtrArray *pdrgpulOutput,
	ULongPtrArray2D *pdrgpdrgulInputColIds,
	DXLNodeArray *pdrgpdxlnChildren,
	BOOL fCastAcrossInput,
	BOOL fKeepResjunked
	)
	const
{
	GPOS_ASSERT(NULL != plTargetListOutput);
	GPOS_ASSERT(NULL != pdrgpulOutput);
	GPOS_ASSERT(NULL != pdrgpdrgulInputColIds);
	GPOS_ASSERT(NULL != pdrgpdxlnChildren);
	GPOS_ASSERT(1 < pdrgpdrgulInputColIds->Size());
	GPOS_ASSERT(1 < pdrgpdxlnChildren->Size());

	// positions of output columns in the target list
	ULongPtrArray *pdrgpulTLPos = CTranslatorUtils::PdrgpulPosInTargetList(m_memory_pool, plTargetListOutput, fKeepResjunked);

	const ULONG ulCols = pdrgpulOutput->Size();
	ULongPtrArray *pdrgpulInputFirstChild = (*pdrgpdrgulInputColIds)[0];
	GPOS_ASSERT(ulCols == pdrgpulInputFirstChild->Size());
	GPOS_ASSERT(ulCols == pdrgpulOutput->Size());

	CBitSet *pbs = GPOS_NEW(m_memory_pool) CBitSet(m_memory_pool);

	// project list to maintain the casting of the duplicate input columns
	CDXLNode *pdxlnNewChildScPrL = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	ULongPtrArray *pdrgpulInputFirstChildNew = GPOS_NEW(m_memory_pool) ULongPtrArray (m_memory_pool);
	ColumnDescrDXLArray *pdrgpdxlcdOutput = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		ULONG ulColIdOutput = *(*pdrgpulOutput)[ul];
		ULONG ulColIdInput = *(*pdrgpulInputFirstChild)[ul];

		BOOL fColExists = pbs->Get(ulColIdInput);
		BOOL fCastedCol = (ulColIdOutput != ulColIdInput);

		ULONG ulTLPos = *(*pdrgpulTLPos)[ul];
		TargetEntry *target_entry = (TargetEntry*) gpdb::PvListNth(plTargetListOutput, ulTLPos);
		GPOS_ASSERT(NULL != target_entry);

		CDXLColDescr *pdxlcdOutput = NULL;
		if (!fColExists)
		{
			pbs->ExchangeSet(ulColIdInput);
			pdrgpulInputFirstChildNew->Append(GPOS_NEW(m_memory_pool) ULONG(ulColIdInput));

			pdxlcdOutput = CTranslatorUtils::GetColumnDescrAt(m_memory_pool, target_entry, ulColIdOutput, ul + 1);
		}
		else
		{
			// we add a dummy-cast to distinguish between the output columns of the union
			ULONG ulColIdNew = m_pidgtorCol->next_id();
			pdrgpulInputFirstChildNew->Append(GPOS_NEW(m_memory_pool) ULONG(ulColIdNew));

			ULONG ulColIdUnionOutput = ulColIdNew;
			if (fCastedCol)
			{
				// create new output column id since current colid denotes its duplicate
				ulColIdUnionOutput = m_pidgtorCol->next_id();
			}

			pdxlcdOutput = CTranslatorUtils::GetColumnDescrAt(m_memory_pool, target_entry, ulColIdUnionOutput, ul + 1);
			CDXLNode *pdxlnPrEl = CTranslatorUtils::PdxlnDummyPrElem(m_memory_pool, ulColIdInput, ulColIdNew, pdxlcdOutput);

			pdxlnNewChildScPrL->AddChild(pdxlnPrEl);
		}

		pdrgpdxlcdOutput->Append(pdxlcdOutput);
	}

	pdrgpdrgulInputColIds->Replace(0, pdrgpulInputFirstChildNew);

	if (0 < pdxlnNewChildScPrL->Arity())
	{
		// create a project node for the dummy casted columns
		CDXLNode *pdxlnFirstChild = (*pdrgpdxlnChildren)[0];
		pdxlnFirstChild->AddRef();
		CDXLNode *pdxlnNewChild = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		pdxlnNewChild->AddChild(pdxlnNewChildScPrL);
		pdxlnNewChild->AddChild(pdxlnFirstChild);

		pdrgpdxlnChildren->Replace(0, pdxlnNewChild);
	}
	else
	{
		pdxlnNewChildScPrL->Release();
	}

	CDXLLogicalSetOp *pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalSetOp
											(
											m_memory_pool,
											edxlsetop,
											pdrgpdxlcdOutput,
											pdrgpdrgulInputColIds,
											fCastAcrossInput
											);
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop, pdrgpdxlnChildren);

	pbs->Release();
	pdrgpulTLPos->Release();

	return dxlnode;
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
	List *target_list,
	MdidPtrArray *pdrgpmdid
	)
	const
{
	GPOS_ASSERT(NULL != pdrgpmdid);
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(pdrgpmdid->Size() <= gpdb::ListLength(target_list)); // there may be resjunked columns

	ULONG ulColPos = 0;
	ListCell *plcTE = NULL;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		OID oidExprType = gpdb::OidExprType((Node*) target_entry->expr);
		if (!target_entry->resjunk)
		{
			IMDId *pmdid = (*pdrgpmdid)[ulColPos];
			if (CMDIdGPDB::CastMdid(pmdid)->OidObjectId() != oidExprType)
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
	ULongPtrArray *pdrgpul,
	MdidPtrArray *pdrgpmdid,
	List *target_list
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
			Query *pqueryDerTbl = CTranslatorUtils::PqueryFixUnknownTypeConstant(prte->subquery, target_list);
			GPOS_ASSERT(NULL != pqueryDerTbl);

			CMappingVarColId *var_col_id_mapping = m_pmapvarcolid->CopyMapColId(m_memory_pool);
			CTranslatorQueryToDXL trquerytodxl
					(
					m_memory_pool,
					m_pmda,
					m_pidgtorCol,
					m_pidgtorCTE,
					var_col_id_mapping,
					pqueryDerTbl,
					m_ulQueryLevel + 1,
					FDMLQuery(),
					m_phmulCTEEntries
					);

			// translate query representing the derived table to its DXL representation
			CDXLNode *dxlnode = trquerytodxl.PdxlnFromQueryInternal();
			GPOS_ASSERT(NULL != dxlnode);

			DXLNodeArray *cte_dxlnode_array = trquerytodxl.PdrgpdxlnCTE();
			CUtils::AddRefAppend(m_pdrgpdxlnCTE, cte_dxlnode_array);
			m_fHasDistributedTables = m_fHasDistributedTables || trquerytodxl.FHasDistributedTables();

			// get the output columns of the derived table
			DXLNodeArray *pdrgpdxln = trquerytodxl.PdrgpdxlnQueryOutput();
			GPOS_ASSERT(pdrgpdxln != NULL);
			const ULONG ulLen = pdrgpdxln->Size();
			for (ULONG ul = 0; ul < ulLen; ul++)
			{
				CDXLNode *pdxlnCurr = (*pdrgpdxln)[ul];
				CDXLScalarIdent *dxl_sc_ident = CDXLScalarIdent::Cast(pdxlnCurr->GetOperator());
				ULONG *pulColId = GPOS_NEW(m_memory_pool) ULONG(dxl_sc_ident->MakeDXLColRef()->Id());
				pdrgpul->Append(pulColId);

				IMDId *pmdidCol = dxl_sc_ident->MDIdType();
				GPOS_ASSERT(NULL != pmdidCol);
				pmdidCol->AddRef();
				pdrgpmdid->Append(pmdidCol);
			}

			return dxlnode;
		}
	}
	else if (IsA(pnodeChild, SetOperationStmt))
	{
		IntUlongHashMap *phmiulOutputCols = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
		CDXLNode *dxlnode = PdxlnFromSetOp(pnodeChild, target_list, phmiulOutputCols);

		// cleanup
		phmiulOutputCols->Release();

		const ColumnDescrDXLArray *pdrgpdxlcd = CDXLLogicalSetOp::Cast(dxlnode->GetOperator())->GetColumnDescrDXLArray();
		GPOS_ASSERT(NULL != pdrgpdxlcd);
		const ULONG ulLen = pdrgpdxlcd->Size();
		for (ULONG ul = 0; ul < ulLen; ul++)
		{
			const CDXLColDescr *dxl_col_descr = (*pdrgpdxlcd)[ul];
			ULONG *pulColId = GPOS_NEW(m_memory_pool) ULONG(dxl_col_descr->Id());
			pdrgpul->Append(pulColId);

			IMDId *pmdidCol = dxl_col_descr->MDIdType();
			GPOS_ASSERT(NULL != pmdidCol);
			pmdidCol->AddRef();
			pdrgpmdid->Append(pmdidCol);
		}

		return dxlnode;
	}

	CHAR *sz = (CHAR*) gpdb::SzNodeToString(const_cast<Node*>(pnodeChild));
	CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, sz);

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, str->GetBuffer());
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
	CDXLNode *dxlnode = NULL;

	if (0 == gpdb::ListLength(pfromexpr->fromlist))
	{
		dxlnode = PdxlnConstTableGet();
	}
	else
	{
		if (1 == gpdb::ListLength(pfromexpr->fromlist))
		{
			Node *pnode = (Node*) gpdb::PvListNth(pfromexpr->fromlist, 0);
			GPOS_ASSERT(NULL != pnode);
			dxlnode = PdxlnFromGPDBFromClauseEntry(pnode);
		}
		else
		{
			// In DXL, we represent an n-ary join (where n>2) by an inner join with condition true.
			// The join conditions represented in the FromExpr->quals is translated
			// into a CDXLLogicalSelect on top of the CDXLLogicalJoin

			dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalJoin(m_memory_pool, EdxljtInner));

			ListCell *lc = NULL;
			ForEach (lc, pfromexpr->fromlist)
			{
				Node *pnode = (Node*) lfirst(lc);
				CDXLNode *pdxlnChild = PdxlnFromGPDBFromClauseEntry(pnode);
				dxlnode->AddChild(pdxlnChild);
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

	if (1 >= gpdb::ListLength(pfromexpr->fromlist))
	{
		if (NULL != pdxlnCond)
		{
			CDXLNode *pdxlnSelect = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalSelect(m_memory_pool));
			pdxlnSelect->AddChild(pdxlnCond);
			pdxlnSelect->AddChild(dxlnode);

			dxlnode = pdxlnSelect;
		}
	}
	else //n-ary joins
	{
		if (NULL == pdxlnCond)
		{
			// A cross join (the scalar condition is true)
			pdxlnCond = PdxlnScConstValueTrue();
		}

		dxlnode->AddChild(pdxlnCond);
	}

	return dxlnode;
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
	CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, sz);

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, str->GetBuffer());

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
	if (false == prte->inh)
	{
		GPOS_ASSERT(RTE_RELATION == prte->rtekind);
		// RangeTblEntry::inh is set to false iff there is ONLY in the FROM
		// clause. c.f. transformTableEntry, called from transformFromClauseItem
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("ONLY in the FROM clause"));
	}

	// construct table descriptor for the scan node from the range table entry
	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_pmda, m_pidgtorCol, prte, &m_fHasDistributedTables);

	CDXLLogicalGet *pdxlop = NULL;
	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());
	if (IMDRelation::ErelstorageExternal == pmdrel->GetRelStorageType())
	{
		pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalExternalGet(m_memory_pool, table_descr);
	}
	else
	{
		pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalGet(m_memory_pool, table_descr);
	}

	CDXLNode *pdxlnGet = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// make note of new columns from base relation
	m_pmapvarcolid->LoadTblColumns(m_ulQueryLevel, ulRTIndex, table_descr);

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

	const ULONG ulValues = gpdb::ListLength(plTuples);
	GPOS_ASSERT(0 < ulValues);

	// children of the UNION ALL
	DXLNodeArray *pdrgpdxln = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

	// array of datum arrays for Values
	DXLDatumArrays *pdrgpdrgpdxldatumValues = GPOS_NEW(m_memory_pool) DXLDatumArrays(m_memory_pool);

	// array of input colid arrays
	ULongPtrArray2D *pdrgpdrgulInputColIds = GPOS_NEW(m_memory_pool) ULongPtrArray2D(m_memory_pool);

	// array of column descriptor for the UNION ALL operator
	ColumnDescrDXLArray *pdrgpdxlcd = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);

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
		ULongPtrArray *pdrgpulColIds = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

		// array of project elements (for expression elements)
		DXLNodeArray *pdrgpdxlnPrEl = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

		// array of datum (for datum constant values)
		DXLDatumArray *pdrgpdxldatum = GPOS_NEW(m_memory_pool) DXLDatumArray(m_memory_pool);

		// array of column descriptors for the CTG containing the datum array
		ColumnDescrDXLArray *pdrgpdxlcdCTG = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);

		List *plColnames = prte->eref->colnames;
		GPOS_ASSERT(NULL != plColnames);
		GPOS_ASSERT(gpdb::ListLength(plTuple) == gpdb::ListLength(plColnames));

		// translate the columns
		ULONG ulColPos = 0;
		ListCell *plcColumn = NULL;
		ForEach (plcColumn, plTuple)
		{
			Expr *pexpr = (Expr *) lfirst(plcColumn);

			CHAR *col_name_char_array = (CHAR *) strVal(gpdb::PvListNth(plColnames, ulColPos));
			ULONG col_id = gpos::ulong_max;
			if (IsA(pexpr, Const))
			{
				// extract the datum
				Const *pconst = (Const *) pexpr;
				CDXLDatum *datum_dxl = m_psctranslator->GetDatumVal(pconst);
				pdrgpdxldatum->Append(datum_dxl);

				col_id = m_pidgtorCol->next_id();

				CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, col_name_char_array);
				CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
				GPOS_DELETE(pstrAlias);

				CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
													(
													m_memory_pool,
													mdname,
													col_id,
													ulColPos + 1 /* iAttno */,
													GPOS_NEW(m_memory_pool) CMDIdGPDB(pconst->consttype),
													pconst->consttypmod,
													false /* is_dropped */
													);

				if (0 == ulTuplePos)
				{
					dxl_col_descr->AddRef();
					pdrgpdxlcd->Append(dxl_col_descr);
				}
				pdrgpdxlcdCTG->Append(dxl_col_descr);
			}
			else
			{
				fAllConstant = false;
				// translate the scalar expression into a project element
				CDXLNode *pdxlnPrE = PdxlnPrEFromGPDBExpr(pexpr, col_name_char_array, true /* fInsistNewColIds */ );
				pdrgpdxlnPrEl->Append(pdxlnPrE);
				col_id = CDXLScalarProjElem::Cast(pdxlnPrE->GetOperator())->Id();

				if (0 == ulTuplePos)
				{
					CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, col_name_char_array);
					CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
					GPOS_DELETE(pstrAlias);

					CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
														(
														m_memory_pool,
														mdname,
														col_id,
														ulColPos + 1 /* iAttno */,
														GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::OidExprType((Node*) pexpr)),
														gpdb::IExprTypeMod((Node*) pexpr),
														false /* is_dropped */
														);
					pdrgpdxlcd->Append(dxl_col_descr);
				}
			}

			GPOS_ASSERT(gpos::ulong_max != col_id);

			pdrgpulColIds->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
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
		CDXLLogicalConstTable *pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalConstTable(m_memory_pool, pdrgpdxlcd, pdrgpdrgpdxldatumValues);
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

		// make note of new columns from Value Scan
		m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdxlop->GetColumnDescrDXLArray());

		// cleanup
		pdrgpdxln->Release();
		pdrgpdrgulInputColIds->Release();

		return dxlnode;
	}
	else if (1 < ulValues)
	{
		// create a UNION ALL operator
		CDXLLogicalSetOp *pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalSetOp(m_memory_pool, EdxlsetopUnionAll, pdrgpdxlcd, pdrgpdrgulInputColIds, false);
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop, pdrgpdxln);

		// make note of new columns from UNION ALL
		m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdxlop->GetColumnDescrDXLArray());
		pdrgpdrgpdxldatumValues->Release();

		return dxlnode;
	}

	GPOS_ASSERT(1 == pdrgpdxln->Size());

	CDXLNode *dxlnode = (*pdrgpdxln)[0];
	dxlnode->AddRef();

	// make note of new columns
	m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdrgpdxlcd);	

	//cleanup
	pdrgpdrgpdxldatumValues->Release();
	pdrgpdxln->Release();
	pdrgpdrgulInputColIds->Release();
	pdrgpdxlcd->Release();

	return dxlnode;
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
	DXLDatumArray *pdrgpdxldatumCTG,
	ColumnDescrDXLArray *pdrgpdxlcdCTG,
	DXLNodeArray *pdrgpdxlnPrEl
	)
	const
{
	GPOS_ASSERT(NULL != pdrgpdxldatumCTG);
	GPOS_ASSERT(NULL != pdrgpdxlnPrEl);
	
	CDXLNode *pdxlnCTG = NULL;
	if (0 == pdrgpdxldatumCTG->Size())
	{
		// add a dummy CTG
		pdxlnCTG = PdxlnConstTableGet();
	}
	else 
	{
		// create the array of datum arrays
		DXLDatumArrays *pdrgpdrgpdxldatumCTG = GPOS_NEW(m_memory_pool) DXLDatumArrays(m_memory_pool);
		
		pdrgpdxldatumCTG->AddRef();
		pdrgpdrgpdxldatumCTG->Append(pdrgpdxldatumCTG);
		
		pdrgpdxlcdCTG->AddRef();
		CDXLLogicalConstTable *pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalConstTable(m_memory_pool, pdrgpdxlcdCTG, pdrgpdrgpdxldatumCTG);
		
		pdxlnCTG = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);
	}

	if (0 == pdrgpdxlnPrEl->Size())
	{
		return pdxlnCTG;
	}

	// create a project node for the list of project elements
	pdrgpdxlnPrEl->AddRef();
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode
										(
										m_memory_pool,
										GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool),
										pdrgpdxlnPrEl
										);
	
	CDXLNode *pdxlnProject = GPOS_NEW(m_memory_pool) CDXLNode
											(
											m_memory_pool,
											GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool),
											project_list_dxl,
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

		CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr((Expr *) prte->funcexpr, prte->eref->aliasname, true /* fInsistNewColIds */);
		project_list_dxl->AddChild(pdxlnPrEl);

		CDXLNode *pdxlnProject = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		pdxlnProject->AddChild(project_list_dxl);
		pdxlnProject->AddChild(pdxlnCTG);

		m_pmapvarcolid->LoadProjectElements(m_ulQueryLevel, ulRTIndex, project_list_dxl);

		return pdxlnProject;
	}

	CDXLLogicalTVF *pdxlopTVF = CTranslatorUtils::Pdxltvf(m_memory_pool, m_pmda, m_pidgtorCol, prte);
	CDXLNode *pdxlnTVF = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopTVF);

	// make note of new columns from function
	m_pmapvarcolid->LoadColumns(m_ulQueryLevel, ulRTIndex, pdxlopTVF->GetColumnDescrDXLArray());

	FuncExpr *pfuncexpr = (FuncExpr *) prte->funcexpr;
	BOOL fSubqueryInArgs = false;

	// check if arguments contain SIRV functions
	if (NIL != pfuncexpr->args && FHasSirvFunctions((Node *) pfuncexpr->args))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}

	ListCell *lc = NULL;
	ForEach (lc, pfuncexpr->args)
	{
		Node *pnodeArg = (Node *) lfirst(lc);
		fSubqueryInArgs = fSubqueryInArgs || CTranslatorUtils::FHasSubquery(pnodeArg);
		CDXLNode *pdxlnFuncExprArg =
				m_psctranslator->PdxlnScOpFromExpr((Expr *) pnodeArg, m_pmapvarcolid, &m_fHasDistributedTables);
		GPOS_ASSERT(NULL != pdxlnFuncExprArg);
		pdxlnTVF->AddChild(pdxlnFuncExprArg);
	}

	CMDIdGPDB *mdid_func = GPOS_NEW(m_memory_pool) CMDIdGPDB(pfuncexpr->funcid);
	const IMDFunction *pmdfunc = m_pmda->Pmdfunc(mdid_func);
	if (fSubqueryInArgs && IMDFunction::EfsVolatile == pmdfunc->GetFuncStability())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Volatile functions with subqueries in arguments"));
	}
	mdid_func->Release();

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
	const CCTEListEntry *pctelistentry = m_phmulCTEEntries->Find(&ulCteQueryLevel);
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

	const CDXLNode *pdxlnCTEProducer = pctelistentry->GetCTEProducer(prte->ctename);
	const List *plCTEProducerTargetList = pctelistentry->GetCTEProducerTargetList(prte->ctename);
	
	GPOS_ASSERT(NULL != pdxlnCTEProducer && NULL != plCTEProducerTargetList);

	CDXLLogicalCTEProducer *pdxlopProducer = CDXLLogicalCTEProducer::Cast(pdxlnCTEProducer->GetOperator());
	ULONG ulCTEId = pdxlopProducer->Id();
	ULongPtrArray *pdrgpulCTEProducer = pdxlopProducer->GetOutputColIdsArray();
	
	// construct output column array
	ULongPtrArray *pdrgpulCTEConsumer = PdrgpulGenerateColIds(m_memory_pool, pdrgpulCTEProducer->Size());
			
	// load the new columns from the CTE
	m_pmapvarcolid->LoadCTEColumns(ulCurrQueryLevel, ulRTIndex, pdrgpulCTEConsumer, const_cast<List *>(plCTEProducerTargetList));

	CDXLLogicalCTEConsumer *pdxlopCTEConsumer = GPOS_NEW(m_memory_pool) CDXLLogicalCTEConsumer(m_memory_pool, ulCTEId, pdrgpulCTEConsumer);
	CDXLNode *pdxlnCTE = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopCTEConsumer);

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

	CMappingVarColId *var_col_id_mapping = m_pmapvarcolid->CopyMapColId(m_memory_pool);

	CTranslatorQueryToDXL trquerytodxl
		(
		m_memory_pool,
		m_pmda,
		m_pidgtorCol,
		m_pidgtorCTE,
		var_col_id_mapping,
		pqueryDerTbl,
		m_ulQueryLevel + 1,
		FDMLQuery(),
		m_phmulCTEEntries
		);

	// translate query representing the derived table to its DXL representation
	CDXLNode *pdxlnDerTbl = trquerytodxl.PdxlnFromQueryInternal();

	// get the output columns of the derived table
	DXLNodeArray *pdrgpdxlnQueryOutputDerTbl = trquerytodxl.PdrgpdxlnQueryOutput();
	DXLNodeArray *cte_dxlnode_array = trquerytodxl.PdrgpdxlnCTE();
	GPOS_ASSERT(NULL != pdxlnDerTbl && pdrgpdxlnQueryOutputDerTbl != NULL);

	CUtils::AddRefAppend(m_pdrgpdxlnCTE, cte_dxlnode_array);
	
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
	EdxlJoinType join_type = CTranslatorUtils::EdxljtFromJoinType(pjoinexpr->jointype);
	CDXLNode *pdxlnJoin = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalJoin(m_memory_pool, join_type));

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
	GPOS_ASSERT(NULL != palias->colnames && 0 < gpdb::ListLength(palias->colnames));
	GPOS_ASSERT(gpdb::ListLength(prte->joinaliasvars) == gpdb::ListLength(palias->colnames));

	CDXLNode *pdxlnPrLComputedColumns = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for each entry in the joinaliasvars
	ListCell *plcNode = NULL;
	ListCell *plcColName = NULL;
	ForBoth (plcNode, prte->joinaliasvars,
			plcColName, palias->colnames)
	{
		Node *pnodeJoinAlias = (Node *) lfirst(plcNode);
		GPOS_ASSERT(IsA(pnodeJoinAlias, Var) || IsA(pnodeJoinAlias, CoalesceExpr));
		Value *value = (Value *) lfirst(plcColName);
		CHAR *col_name_char_array = strVal(value);

		// create the DXL node holding the target list entry and add it to proj list
		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr( (Expr*) pnodeJoinAlias, col_name_char_array);
		project_list_dxl->AddChild(pdxlnPrEl);

		if (IsA(pnodeJoinAlias, CoalesceExpr))
		{
			// add coalesce expression to the computed columns
			pdxlnPrEl->AddRef();
			pdxlnPrLComputedColumns->AddChild(pdxlnPrEl);
		}
	}
	m_pmapvarcolid->LoadProjectElements(m_ulQueryLevel, ulRtIndex, project_list_dxl);
	project_list_dxl->Release();

	if (0 == pdxlnPrLComputedColumns->Arity())
	{
		pdxlnPrLComputedColumns->Release();
		return pdxlnJoin;
	}

	CDXLNode *pdxlnProject = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
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
	List *target_list,
	CDXLNode *pdxlnChild,
	IntUlongHashMap *phmiulSortgrouprefColId,
	IntUlongHashMap *phmiulOutputCols,
	List *plgrpcl,
	BOOL fExpandAggrefExpr
	)
{
	BOOL fGroupBy = (0 != gpdb::ListLength(m_pquery->groupClause) || m_pquery->hasAggs);

	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for each entry in the target list
	ListCell *plcTE = NULL;
	
	// target entries that are result of flattening join alias
	// and are equivalent to a defined grouping column target entry
	List *plOmittedTE = NIL; 

	// list for all vars used in aggref expressions
	List *plVars = NULL;
	ULONG ulResno = 0;
	ForEach(plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ulResno = target_entry->resno;

		BOOL fGroupingCol = CTranslatorUtils::FGroupingColumn(target_entry, plgrpcl);
		if (!fGroupBy || (fGroupBy && fGroupingCol))
		{
			CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr(target_entry->expr, target_entry->resname);
			ULONG col_id = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator())->Id();

			AddSortingGroupingColumn(target_entry, phmiulSortgrouprefColId, col_id);

			// add column to the list of output columns of the query
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);

			if (!IsA(target_entry->expr, Var))
			{
				// only add computed columns to the project list
				project_list_dxl->AddChild(pdxlnPrEl);
			}
			else
			{
				pdxlnPrEl->Release();
			}
		}
		else if (fExpandAggrefExpr && IsA(target_entry->expr, Aggref))
		{
			plVars = gpdb::PlConcat(plVars, gpdb::PlExtractNodesExpression((Node *) target_entry->expr, T_Var, false /*descendIntoSubqueries*/));
		}
		else if (!IsA(target_entry->expr, Aggref))
		{
			plOmittedTE = gpdb::PlAppendElement(plOmittedTE, target_entry);
		}
	}

	// process target entries that are a result of flattening join alias
	plcTE = NULL;
	ForEach(plcTE, plOmittedTE)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		INT iSortGroupRef = (INT) target_entry->ressortgroupref;

		TargetEntry *pteGroupingColumn = CTranslatorUtils::PteGroupingColumn( (Node*) target_entry->expr, plgrpcl, target_list);
		if (NULL != pteGroupingColumn)
		{
			const ULONG col_id = CTranslatorUtils::GetColId((INT) pteGroupingColumn->ressortgroupref, phmiulSortgrouprefColId);
			StoreAttnoColIdMapping(phmiulOutputCols, target_entry->resno, col_id);
			if (0 < iSortGroupRef && 0 < col_id && NULL == phmiulSortgrouprefColId->Find(&iSortGroupRef))
			{
				AddSortingGroupingColumn(target_entry, phmiulSortgrouprefColId, col_id);
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
		Var *var = (Var *) lfirst(plcVar);

		// TODO: Dec 28, 2012; figure out column's name
		CDXLNode *pdxlnPrEl =  PdxlnPrEFromGPDBExpr((Expr*) var, "?col?");
		
		ULONG col_id = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator())->Id();

		// add column to the list of output columns of the query
		StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);
		
		pdxlnPrEl->Release();
	}
	
	if (0 < project_list_dxl->Arity())
	{
		// create a node with the CDXLLogicalProject operator and add as its children:
		// the CDXLProjectList node and the node representing the input to the project node
		CDXLNode *pdxlnProject = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		pdxlnProject->AddChild(project_list_dxl);
		pdxlnProject->AddChild(pdxlnChild);
		GPOS_ASSERT(NULL != pdxlnProject);
		return pdxlnProject;
	}

	project_list_dxl->Release();
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
	List *target_list,
	CDXLNode *pdxlnChild,
	CBitSet *pbs,					// group by columns
	IntUlongHashMap *phmiulSortgrouprefCols,	// mapping of sorting and grouping columns
	IntUlongHashMap *phmiulOutputCols,		// mapping of output columns
	UlongUlongHashMap *phmululGrpColPos		// mapping of unique grouping col positions
	)
	const
{
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for those non-aggregate entries in the target list which
	// are not included in the grouping set
	ListCell *plcTE = NULL;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		BOOL fGroupingCol = pbs->Get(target_entry->ressortgroupref);
		ULONG ulResno = target_entry->resno;

		ULONG col_id = 0;
		
		if (IsA(target_entry->expr, GroupingFunc))
		{
			col_id = m_pidgtorCol->next_id();
			CDXLNode *pdxlnGroupingFunc = PdxlnGroupingFunc(target_entry->expr, pbs, phmululGrpColPos);
			CMDName *pmdnameAlias = NULL;

			if (NULL == target_entry->resname)
			{
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("grouping"));
				pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol);
			}
			else
			{
				CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
				pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
				GPOS_DELETE(pstrAlias);
			}
			CDXLNode *pdxlnPrEl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, col_id, pmdnameAlias), pdxlnGroupingFunc);
			project_list_dxl->AddChild(pdxlnPrEl);
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);
		}
		else if (!fGroupingCol && !IsA(target_entry->expr, Aggref))
		{
			OID oidType = gpdb::OidExprType((Node *) target_entry->expr);

			col_id = m_pidgtorCol->next_id();

			CMDIdGPDB *pmdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(oidType);
			CDXLNode *pdxlnPrEl = CTranslatorUtils::PdxlnPrElNull(m_memory_pool, m_pmda, pmdid, col_id, target_entry->resname);
			pmdid->Release();
			
			project_list_dxl->AddChild(pdxlnPrEl);
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);
		}
		
		INT iSortGroupRef = INT (target_entry->ressortgroupref);
		
		GPOS_ASSERT_IMP(0 == phmiulSortgrouprefCols, NULL != phmiulSortgrouprefCols->Find(&iSortGroupRef) && "Grouping column with no mapping");
		
		if (0 < iSortGroupRef && 0 < col_id && NULL == phmiulSortgrouprefCols->Find(&iSortGroupRef))
		{
			AddSortingGroupingColumn(target_entry, phmiulSortgrouprefCols, col_id);
		}
	}

	if (0 == project_list_dxl->Arity())
	{
		// no project necessary
		project_list_dxl->Release();
		return pdxlnChild;
	}

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool), project_list_dxl, pdxlnChild);
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
	List *target_list,
	CDXLNode *pdxlnChild,
	CBitSet *pbs,
	IntUlongHashMap *phmiulOutputCols,
	UlongUlongHashMap *phmululGrpColPos,
	IntUlongHashMap *phmiulSortgrouprefColId
	)
	const
{
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for those non-aggregate entries in the target list which
	// are not included in the grouping set
	ListCell *plcTE = NULL;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		ULONG ulResno = target_entry->resno;

		if (IsA(target_entry->expr, GroupingFunc))
		{
			ULONG col_id = m_pidgtorCol->next_id();
			CDXLNode *pdxlnGroupingFunc = PdxlnGroupingFunc(target_entry->expr, pbs, phmululGrpColPos);
			CMDName *pmdnameAlias = NULL;

			if (NULL == target_entry->resname)
			{
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("grouping"));
				pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol);
			}
			else
			{
				CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
				pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
				GPOS_DELETE(pstrAlias);
			}
			CDXLNode *pdxlnPrEl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, col_id, pmdnameAlias), pdxlnGroupingFunc);
			project_list_dxl->AddChild(pdxlnPrEl);
			StoreAttnoColIdMapping(phmiulOutputCols, ulResno, col_id);
			AddSortingGroupingColumn(target_entry, phmiulSortgrouprefColId, col_id);
		}
	}

	if (0 == project_list_dxl->Arity())
	{
		// no project necessary
		project_list_dxl->Release();
		return pdxlnChild;
	}

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool), project_list_dxl, pdxlnChild);
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
	IntUlongHashMap *phmiul,
	INT iAttno,
	ULONG col_id
	)
	const
{
	GPOS_ASSERT(NULL != phmiul);

	INT *piKey = GPOS_NEW(m_memory_pool) INT(iAttno);
	ULONG *pulValue = GPOS_NEW(m_memory_pool) ULONG(col_id);
	BOOL result = phmiul->Insert(piKey, pulValue);

	if (!result)
	{
		GPOS_DELETE(piKey);
		GPOS_DELETE(pulValue);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::PdrgpdxlnConstructOutputCols
//
//	@doc:
//		Construct an array of DXL nodes representing the query output
//
//---------------------------------------------------------------------------
DXLNodeArray *
CTranslatorQueryToDXL::PdrgpdxlnConstructOutputCols
	(
	List *target_list,
	IntUlongHashMap *phmiulAttnoColId
	)
	const
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != phmiulAttnoColId);

	DXLNodeArray *pdrgpdxln = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG ulResNo = target_entry->resno;

		if (target_entry->resjunk)
		{
			continue;
		}

		GPOS_ASSERT(NULL != target_entry);
		CMDName *mdname = NULL;
		if (NULL == target_entry->resname)
		{
			CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
			mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol);
		}
		else
		{
			CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
			mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
			// CName constructor copies string
			GPOS_DELETE(pstrAlias);
		}

		const ULONG col_id = CTranslatorUtils::GetColId(ulResNo, phmiulAttnoColId);

		// create a column reference
		IMDId *mdid_type = GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::OidExprType( (Node*) target_entry->expr));
		INT type_modifier = gpdb::IExprTypeMod((Node*) target_entry->expr);
		CDXLColRef *dxl_colref = GPOS_NEW(m_memory_pool) CDXLColRef(m_memory_pool, mdname, col_id, mdid_type, type_modifier);
		CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_memory_pool) CDXLScalarIdent
												(
												m_memory_pool,
												dxl_colref
												);

		// create the DXL node holding the scalar ident operator
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopIdent);

		pdrgpdxln->Append(dxlnode);
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
		pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol);
	}
	else
	{
		CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, szAliasName);
		pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, pstrAlias);
		GPOS_DELETE(pstrAlias);
	}

	if (IsA(pexpr, Var) && !fInsistNewColIds)
	{
		// project elem is a a reference to a column - use the colref id
		GPOS_ASSERT(EdxlopScalarIdent == pdxlnChild->GetOperator()->GetDXLOperator());
		CDXLScalarIdent *pdxlopIdent = (CDXLScalarIdent *) pdxlnChild->GetOperator();
		ulPrElId = pdxlopIdent->MakeDXLColRef()->Id();
	}
	else
	{
		// project elem is a defined column - get a new id
		ulPrElId = m_pidgtorCol->next_id();
	}

	CDXLNode *pdxlnPrEl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, ulPrElId, pmdnameAlias));
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
	CDXLNode *dxlnode = PdxlnScFromGPDBExpr((Expr*) pconst);
	gpdb::GPDBFree(pconst);

	return dxlnode;
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
	UlongUlongHashMap *phmululGrpColPos
	)
	const
{
	GPOS_ASSERT(IsA(pexpr, GroupingFunc));
	GPOS_ASSERT(NULL != phmululGrpColPos);

	const GroupingFunc *pgroupingfunc = (GroupingFunc *) pexpr;

	if (1 < gpdb::ListLength(pgroupingfunc->args))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Grouping function with multiple arguments"));
	}
	
	Node *pnode = (Node *) gpdb::PvListNth(pgroupingfunc->args, 0);
	ULONG ulGroupingIndex = gpdb::IValue(pnode);
		
	// generate a constant value for the result of the grouping function as follows:
	// if the grouping function argument is a group-by column, result is 0
	// otherwise, the result is 1 
	LINT lValue = 0;
	
	ULONG *pulSortGrpRef = phmululGrpColPos->Find(&ulGroupingIndex);
	GPOS_ASSERT(NULL != pulSortGrpRef);
	BOOL fGroupingCol = pbs->Get(*pulSortGrpRef);
	if (!fGroupingCol)
	{
		// not a grouping column
		lValue = 1; 
	}

	const IMDType *pmdtype = m_pmda->PtMDType<IMDTypeInt8>(m_sysid);
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(*pmdidMDC);
	
	CDXLDatum *datum_dxl = GPOS_NEW(m_memory_pool) CDXLDatumInt8(m_memory_pool, pmdid, false /* is_null */, lValue);
	CDXLScalarConstValue *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarConstValue(m_memory_pool, datum_dxl);
	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);
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
	
	ListCell *lc = NULL;
	
	ForEach (lc, plCTE)
	{
		CommonTableExpr *pcte = (CommonTableExpr *) lfirst(lc);
		GPOS_ASSERT(IsA(pcte->ctequery, Query));
		
		if (pcte->cterecursive)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("WITH RECURSIVE"));
		}

		Query *pqueryCte = CQueryMutators::PqueryNormalize(m_memory_pool, m_pmda, (Query *) pcte->ctequery, ulCteQueryLevel + 1);
		
		// the query representing the cte can only access variables defined in the current level as well as
		// those defined at prior query levels
		CMappingVarColId *var_col_id_mapping = m_pmapvarcolid->CopyMapColId(ulCteQueryLevel);

		CTranslatorQueryToDXL trquerytodxl
			(
			m_memory_pool,
			m_pmda,
			m_pidgtorCol,
			m_pidgtorCTE,
			var_col_id_mapping,
			pqueryCte,
			ulCteQueryLevel + 1,
			FDMLQuery(),
			m_phmulCTEEntries
			);

		// translate query representing the cte table to its DXL representation
		CDXLNode *pdxlnCteChild = trquerytodxl.PdxlnFromQueryInternal();

		// get the output columns of the cte table
		DXLNodeArray *pdrgpdxlnQueryOutputCte = trquerytodxl.PdrgpdxlnQueryOutput();
		DXLNodeArray *cte_dxlnode_array = trquerytodxl.PdrgpdxlnCTE();
		m_fHasDistributedTables = m_fHasDistributedTables || trquerytodxl.FHasDistributedTables();

		GPOS_ASSERT(NULL != pdxlnCteChild && NULL != pdrgpdxlnQueryOutputCte && NULL != cte_dxlnode_array);
		
		// append any nested CTE
		CUtils::AddRefAppend(m_pdrgpdxlnCTE, cte_dxlnode_array);
		
		ULongPtrArray *pdrgpulColIds = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
		
		const ULONG ulOutputCols = pdrgpdxlnQueryOutputCte->Size();
		for (ULONG ul = 0; ul < ulOutputCols; ul++)
		{
			CDXLNode *pdxlnOutputCol = (*pdrgpdxlnQueryOutputCte)[ul];
			CDXLScalarIdent *dxl_sc_ident = CDXLScalarIdent::Cast(pdxlnOutputCol->GetOperator());
			pdrgpulColIds->Append(GPOS_NEW(m_memory_pool) ULONG(dxl_sc_ident->MakeDXLColRef()->Id()));
		}
		
		CDXLLogicalCTEProducer *pdxlop = GPOS_NEW(m_memory_pool) CDXLLogicalCTEProducer(m_memory_pool, m_pidgtorCTE->next_id(), pdrgpulColIds);
		CDXLNode *pdxlnCTEProducer = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop, pdxlnCteChild);
		
		m_pdrgpdxlnCTE->Append(pdxlnCTEProducer);
#ifdef GPOS_DEBUG
		BOOL result =
#endif
		m_phmulfCTEProducers->Insert(GPOS_NEW(m_memory_pool) ULONG(pdxlop->Id()), GPOS_NEW(m_memory_pool) BOOL(true));
		GPOS_ASSERT(result);
		
		// update CTE producer mappings
		CCTEListEntry *pctelistentry = m_phmulCTEEntries->Find(&ulCteQueryLevel);
		if (NULL == pctelistentry)
		{
			pctelistentry = GPOS_NEW(m_memory_pool) CCTEListEntry (m_memory_pool, ulCteQueryLevel, pcte, pdxlnCTEProducer);
#ifdef GPOS_DEBUG
		BOOL fRes =
#endif
			m_phmulCTEEntries->Insert(GPOS_NEW(m_memory_pool) ULONG(ulCteQueryLevel), pctelistentry);
			GPOS_ASSERT(fRes);
		}
		else
		{
			pctelistentry->AddCTEProducer(m_memory_pool, pcte, pdxlnCTEProducer);
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
	DXLNodeArray *pdrgpdxln,
	CDXLNode **ppdxlnCTEAnchorTop,
	CDXLNode **ppdxlnCTEAnchorBottom
	)
{
	GPOS_ASSERT(NULL == *ppdxlnCTEAnchorTop);
	GPOS_ASSERT(NULL == *ppdxlnCTEAnchorBottom);

	if (NULL == pdrgpdxln || 0 == pdrgpdxln->Size())
	{
		return;
	}
	
	const ULONG ulCTEs = pdrgpdxln->Size();
	
	for (ULONG ul = ulCTEs; ul > 0; ul--)
	{
		// construct a new CTE anchor on top of the previous one
		CDXLNode *pdxlnCTEProducer = (*pdrgpdxln)[ul-1];
		CDXLLogicalCTEProducer *pdxlopCTEProducer = CDXLLogicalCTEProducer::Cast(pdxlnCTEProducer->GetOperator());
		ULONG ulCTEProducerId = pdxlopCTEProducer->Id();
		
		if (NULL == m_phmulfCTEProducers->Find(&ulCTEProducerId))
		{
			// cte not defined at this level: CTE anchor was already added
			continue;
		}
		
		CDXLNode *pdxlnCTEAnchorNew = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalCTEAnchor(m_memory_pool, ulCTEProducerId));
		
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
ULongPtrArray *
CTranslatorQueryToDXL::PdrgpulGenerateColIds
	(
	IMemoryPool *memory_pool,
	ULONG size
	)
	const
{
	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	
	for (ULONG ul = 0; ul < size; ul++)
	{
		pdrgpul->Append(GPOS_NEW(memory_pool) ULONG(m_pidgtorCol->next_id()));
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
ULongPtrArray *
CTranslatorQueryToDXL::PdrgpulExtractColIds
	(
	IMemoryPool *memory_pool,
	IntUlongHashMap *phmiul
	)
	const
{
	UlongUlongHashMap *old_new_col_mapping = GPOS_NEW(memory_pool) UlongUlongHashMap(memory_pool);
	
	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	
	HMIUlIter mi(phmiul);
	while (mi.Advance())
	{
		ULONG col_id = *(mi.Value());
		
		// do not insert colid if already inserted
		if (NULL == old_new_col_mapping->Find(&col_id))
		{
			pdrgpul->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
			old_new_col_mapping->Insert(GPOS_NEW(m_memory_pool) ULONG(col_id), GPOS_NEW(m_memory_pool) ULONG(col_id));
		}
	}
		
	old_new_col_mapping->Release();
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
IntUlongHashMap *
CTranslatorQueryToDXL::PhmiulRemapColIds
	(
	IMemoryPool *memory_pool,
	IntUlongHashMap *phmiul,
	ULongPtrArray *pdrgpulFrom,
	ULongPtrArray *pdrgpulTo
	)
	const
{
	GPOS_ASSERT(NULL != phmiul);
	GPOS_ASSERT(NULL != pdrgpulFrom && NULL != pdrgpulTo);
	GPOS_ASSERT(pdrgpulFrom->Size() == pdrgpulTo->Size());
	
	// compute a map of the positions in the from array
	UlongUlongHashMap *old_new_col_mapping = GPOS_NEW(memory_pool) UlongUlongHashMap(memory_pool);
	const ULONG size = pdrgpulFrom->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
#ifdef GPOS_DEBUG
		BOOL result = 
#endif // GPOS_DEBUG
		old_new_col_mapping->Insert(GPOS_NEW(memory_pool) ULONG(*((*pdrgpulFrom)[ul])), GPOS_NEW(memory_pool) ULONG(*((*pdrgpulTo)[ul])));
		GPOS_ASSERT(result);
	}

	IntUlongHashMap *phmiulResult = GPOS_NEW(memory_pool) IntUlongHashMap(memory_pool);
	HMIUlIter mi(phmiul);
	while (mi.Advance())
	{
		INT *piKey = GPOS_NEW(memory_pool) INT(*(mi.Key()));
		const ULONG *pulValue = mi.Value();
		GPOS_ASSERT(NULL != pulValue);
		
		ULONG *pulValueRemapped = GPOS_NEW(memory_pool) ULONG(*(old_new_col_mapping->Find(pulValue)));
		phmiulResult->Insert(piKey, pulValueRemapped);
	}
		
	old_new_col_mapping->Release();
	
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
