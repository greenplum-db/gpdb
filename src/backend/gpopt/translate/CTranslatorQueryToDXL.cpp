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
static const OID lead_func_oids[] =
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
static const OID lag_func_oids[] =
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
	CIdGenerator *m_colid_counter,
	CIdGenerator *cte_id_counter,
	CMappingVarColId *var_col_id_mapping,
	Query *query,
	ULONG query_level,
	BOOL is_top_query_dml,
	HMUlCTEListEntry *query_level_to_cte_map
	)
	:
	m_memory_pool(memory_pool),
	m_sysid(IMDId::EmdidGPDB, GPMD_GPDB_SYSID),
	m_md_accessor(md_accessor),
	m_colid_counter(m_colid_counter),
	m_cte_id_counter(cte_id_counter),
	m_var_to_colid_map(var_col_id_mapping),
	m_query_level(query_level),
	m_has_distributed_tables(false),
	m_is_top_query_dml(is_top_query_dml),
	m_is_ctas_query(false),
	m_query_level_to_cte_map(NULL),
	m_dxl_query_output_cols(NULL),
	m_dxl_cte_producers(NULL),
	m_cteid_at_current_query_level_map(NULL)
{
	GPOS_ASSERT(NULL != query);
	CheckSupportedCmdType(query);
	
	m_query_level_to_cte_map = GPOS_NEW(m_memory_pool) HMUlCTEListEntry(m_memory_pool);
	m_dxl_cte_producers = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
	m_cteid_at_current_query_level_map = GPOS_NEW(m_memory_pool) UlongBoolHashMap(m_memory_pool);
	
	if (NULL != query_level_to_cte_map)
	{
		HMIterUlCTEListEntry cte_list_hashmap_iter(query_level_to_cte_map);

		while (cte_list_hashmap_iter.Advance())
		{
			ULONG cte_query_level = *(cte_list_hashmap_iter.Key());

			CCTEListEntry *cte_list_entry =  const_cast<CCTEListEntry *>(cte_list_hashmap_iter.Value());

			// CTE's that have been defined before the m_query_level
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

			if (cte_query_level < query_level && NULL != cte_list_entry)
			{
				cte_list_entry->AddRef();
#ifdef GPOS_DEBUG
				BOOL is_res =
#endif
				m_query_level_to_cte_map->Insert(GPOS_NEW(memory_pool) ULONG(cte_query_level), cte_list_entry);
				GPOS_ASSERT(is_res);
			}
		}
	}

	// check if the query has any unsupported node types
	CheckUnsupportedNodeTypes(query);

	// check if the query has SIRV functions in the targetlist without a FROM clause
	CheckSirvFuncsWithoutFromClause(query);

	// first normalize the query
	m_query = CQueryMutators::NormalizeQuery(m_memory_pool, m_md_accessor, query, query_level);

	if (NULL != m_query->cteList)
	{
		ConstructCTEProducerList(m_query->cteList, query_level);
	}

	m_scalar_translator = GPOS_NEW(m_memory_pool) CTranslatorScalarToDXL
									(
									m_memory_pool,
									m_md_accessor,
									m_colid_counter,
									m_cte_id_counter,
									m_query_level,
									true, /* m_fQuery */
									m_query_level_to_cte_map,
									m_dxl_cte_producers
									);

}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::QueryToDXLInstance
//
//	@doc:
//		Factory function
//
//---------------------------------------------------------------------------
CTranslatorQueryToDXL *
CTranslatorQueryToDXL::QueryToDXLInstance
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *m_colid_counter,
	CIdGenerator *cte_id_counter,
	CMappingVarColId *var_col_id_mapping,
	Query *query,
	ULONG query_level,
	HMUlCTEListEntry *query_level_to_cte_map
	)
{
	return GPOS_NEW(memory_pool) CTranslatorQueryToDXL
		(
		memory_pool,
		md_accessor,
		m_colid_counter,
		cte_id_counter,
		var_col_id_mapping,
		query,
		query_level,
		false,  // is_top_query_dml
		query_level_to_cte_map
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
	GPOS_DELETE(m_scalar_translator);
	GPOS_DELETE(m_var_to_colid_map);
	gpdb::GPDBFree(m_query);
	m_query_level_to_cte_map->Release();
	m_dxl_cte_producers->Release();
	m_cteid_at_current_query_level_map->Release();
	CRefCount::SafeRelease(m_dxl_query_output_cols);
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
	static const SUnsupportedFeature unsupported_features[] =
	{
		{T_RowExpr, GPOS_WSZ_LIT("ROW EXPRESSION")},
		{T_RowCompareExpr, GPOS_WSZ_LIT("ROW COMPARE")},
		{T_FieldSelect, GPOS_WSZ_LIT("FIELDSELECT")},
		{T_FieldStore, GPOS_WSZ_LIT("FIELDSTORE")},
		{T_CoerceToDomainValue, GPOS_WSZ_LIT("COERCETODOMAINVALUE")},
		{T_GroupId, GPOS_WSZ_LIT("GROUPID")},
		{T_CurrentOfExpr, GPOS_WSZ_LIT("CURRENT OF")},
	};

	List *unsupported_list = NIL;
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(unsupported_features); ul++)
	{
		unsupported_list = gpdb::LAppendInt(unsupported_list, unsupported_features[ul].node_tag);
	}

	INT unsupported_node = gpdb::FindNodes((Node *) query, unsupported_list);
	gpdb::GPDBFree(unsupported_list);

	if (0 <= unsupported_node)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, unsupported_features[unsupported_node].m_feature_name);
	}

	// GDPB_91_MERGE_FIXME: collation
	INT non_default_collation = gpdb::CheckCollation((Node *) query);

	if (0 < non_default_collation)
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
	if (HasSirvFunctions((Node *) query->targetList))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::HasSirvFunctions
//
//	@doc:
//		Check for SIRV functions in the tree rooted at the given node
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::HasSirvFunctions
	(
	Node *node
	)
	const
{
	GPOS_ASSERT(NULL != node);

	List *function_list =	gpdb::ExtractNodesExpression(node, T_FuncExpr, true /*descendIntoSubqueries*/);
	ListCell *lc = NULL;

	BOOL has_sirv = false;
	ForEach (lc, function_list)
	{
		FuncExpr *func_expr = (FuncExpr *) lfirst(lc);
		if (CTranslatorUtils::IsSirvFunc(m_memory_pool, m_md_accessor, func_expr->funcid))
		{
			has_sirv = true;
			break;
		}
	}
	gpdb::ListFree(function_list);

	return has_sirv;
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

	static const SCmdNameElem unsupported_commands[] =
		{
		{CMD_UTILITY, GPOS_WSZ_LIT("UTILITY command")}
		};

	const ULONG length = GPOS_ARRAY_SIZE(unsupported_commands);
	for (ULONG ul = 0; ul < length; ul++)
	{
		SCmdNameElem mapelem = unsupported_commands[ul];
		if (mapelem.m_cmd_type == query->commandType)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, mapelem.m_cmd_name);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::GetQueryOutputCols
//
//	@doc:
//		Return the list of query output columns
//
//---------------------------------------------------------------------------
DXLNodeArray *
CTranslatorQueryToDXL::GetQueryOutputCols() const
{
	return m_dxl_query_output_cols;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::GetCTEs
//
//	@doc:
//		Return the list of CTEs
//
//---------------------------------------------------------------------------
DXLNodeArray *
CTranslatorQueryToDXL::GetCTEs() const
{
	return m_dxl_cte_producers;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateSelectQueryToDXL
//
//	@doc:
//		Translates a Query into a DXL tree. The function allocates memory in
//		the translator memory pool, and caller is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateSelectQueryToDXL()
{
	// The parsed query contains an RTE for the view, which is maintained all the way through planned statement.
	// This entries is annotated as requiring SELECT permissions for the current user.
	// In Orca, we only keep range table entries for the base tables in the planned statement, but not for the view itself.
	// Since permissions are only checked during ExecutorStart, we lose track of the permissions required for the view and the select goes through successfully.
	// We therefore need to check permissions before we go into optimization for all RTEs, including the ones not explicitly referred in the query, e.g. views.
	CTranslatorUtils::CheckRTEPermissions(m_query->rtable);

	// RETURNING is not supported yet.
	if (m_query->returningList)
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("RETURNING clause"));

	CDXLNode *dxl_node_child = NULL;
	IntUlongHashMap *sort_group_attno_to_colid_mapping =  GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
	IntUlongHashMap *output_attno_to_colid_mapping = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);

	// construct CTEAnchor operators for the CTEs defined at the top level
	CDXLNode *dxl_cte_anchor_top = NULL;
	CDXLNode *dxl_cte_anchor_bottom = NULL;
	ConstructCTEAnchors(m_dxl_cte_producers, &dxl_cte_anchor_top, &dxl_cte_anchor_bottom);
	GPOS_ASSERT_IMP(m_dxl_cte_producers == NULL || 0 < m_dxl_cte_producers->Size(),
					NULL != dxl_cte_anchor_top && NULL != dxl_cte_anchor_bottom);
	
	GPOS_ASSERT_IMP(NULL != m_query->setOperations, 0 == gpdb::ListLength(m_query->windowClause));
	if (NULL != m_query->setOperations)
	{
		List *target_list = m_query->targetList;
		// translate set operations
		dxl_node_child = TranslateSetOpToDXL(m_query->setOperations, target_list, output_attno_to_colid_mapping);

		CDXLLogicalSetOp *dxlop = CDXLLogicalSetOp::Cast(dxl_node_child->GetOperator());
		const ColumnDescrDXLArray *col_descr_dxl_array = dxlop->GetColumnDescrDXLArray();
		ListCell *lc = NULL;
		ULONG resno = 1;
		ForEach (lc, target_list)
		{
			TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
			if (0 < target_entry->ressortgroupref)
			{
				ULONG col_id = ((*col_descr_dxl_array)[resno - 1])->Id();
				AddSortingGroupingColumn(target_entry, sort_group_attno_to_colid_mapping, col_id);
			}
			resno++;
		}
	}
	else if (0 != gpdb::ListLength(m_query->windowClause)) // translate window clauses
	{
		CDXLNode *dxlnode = TranslateFromExprToDXL(m_query->jointree);
		GPOS_ASSERT(NULL == m_query->groupClause);
		dxl_node_child = TranslateWindowToDXL
						(
						dxlnode,
						m_query->targetList,
						m_query->windowClause,
						m_query->sortClause,
						sort_group_attno_to_colid_mapping,
						output_attno_to_colid_mapping
						);
	}
	else
	{
		dxl_node_child = TranslateGroupingSets(m_query->jointree, m_query->targetList, m_query->groupClause, m_query->hasAggs, sort_group_attno_to_colid_mapping, output_attno_to_colid_mapping);
	}

	// translate limit clause
	CDXLNode *limit_dxlnode = TranslateLimitToDXLGroupBy(m_query->sortClause, m_query->limitCount, m_query->limitOffset, dxl_node_child, sort_group_attno_to_colid_mapping);


	if (NULL == m_query->targetList)
	{
		m_dxl_query_output_cols = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
	}
	else
	{
		m_dxl_query_output_cols = CreateDXLOutputCols(m_query->targetList, output_attno_to_colid_mapping);
	}

	// cleanup
	CRefCount::SafeRelease(sort_group_attno_to_colid_mapping);

	output_attno_to_colid_mapping->Release();
	
	// add CTE anchors if needed
	CDXLNode *result_dxlnode = limit_dxlnode;
	
	if (NULL != dxl_cte_anchor_top)
	{
		GPOS_ASSERT(NULL != dxl_cte_anchor_bottom);
		dxl_cte_anchor_bottom->AddChild(result_dxlnode);
		result_dxlnode = dxl_cte_anchor_top;
	}
	
	return result_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateSelectProjectJoinToDXL
//
//	@doc:
//		Construct a DXL SPJ tree from the given query parts
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateSelectProjectJoinToDXL
	(
	List *target_list,
	FromExpr *from_expr,
	IntUlongHashMap *sort_group_attno_to_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping,
	List *group_clause
	)
{
	CDXLNode *join_tree_dxlnode = TranslateFromExprToDXL(from_expr);

	// translate target list entries into a logical project
	return TranslateTargetListToDXLProject(target_list, join_tree_dxlnode, sort_group_attno_to_colid_mapping, output_attno_to_colid_mapping, group_clause);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateSelectProjectJoinForGrpSetsToDXL
//
//	@doc:
//		Construct a DXL SPJ tree from the given query parts, and keep variables
//		appearing in aggregates in the project list
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateSelectProjectJoinForGrpSetsToDXL
	(
	List *target_list,
	FromExpr *from_expr,
	IntUlongHashMap *sort_group_attno_to_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping,
	List *group_clause
	)
{
	CDXLNode *join_tree_dxlnode = TranslateFromExprToDXL(from_expr);

	// translate target list entries into a logical project
	return TranslateTargetListToDXLProject(target_list, join_tree_dxlnode, sort_group_attno_to_colid_mapping, output_attno_to_colid_mapping, group_clause, true /*fExpandAggrefExpr*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateQueryToDXL
//
//	@doc:
//		Main driver
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateQueryToDXL()
{
	CAutoTimer at("\n[OPT]: Query To DXL Translation Time", GPOS_FTRACE(EopttracePrintOptimizationStatistics));

	switch (m_query->commandType)
	{
		case CMD_SELECT:
			if (NULL == m_query->intoClause)
			{
				return TranslateSelectQueryToDXL();
			}

			return TranslateCTASToDXL();
			
		case CMD_INSERT:
			return TranslateInsertQueryToDXL();

		case CMD_DELETE:
			return TranslateDeleteQueryToDXL();

		case CMD_UPDATE:
			return TranslateUpdateQueryToDXL();

		default:
			GPOS_ASSERT(!"Statement type not supported");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateInsertQueryToDXL
//
//	@doc:
//		Translate an insert stmt
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateInsertQueryToDXL()
{
	GPOS_ASSERT(CMD_INSERT == m_query->commandType);
	GPOS_ASSERT(0 < m_query->resultRelation);

	CDXLNode *query_dxlnode = TranslateSelectQueryToDXL();
	const RangeTblEntry *rte = (RangeTblEntry *) gpdb::ListNth(m_query->rtable, m_query->resultRelation - 1);

	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_md_accessor, m_colid_counter, rte, &m_has_distributed_tables);
	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::RelHasTriggers(m_memory_pool, m_md_accessor, md_rel, Edxldmlinsert))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("INSERT with triggers"));
	}

	BOOL rel_has_constraints = CTranslatorUtils::RelHasConstraints(md_rel);
	if (!optimizer_enable_dml_constraints && rel_has_constraints)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("INSERT with constraints"));
	}
	
	const ULONG num_table_columns = CTranslatorUtils::GetNumNonSystemColumns(md_rel);
	const ULONG target_list_length = gpdb::ListLength(m_query->targetList);
	GPOS_ASSERT(num_table_columns >= target_list_length);
	GPOS_ASSERT(target_list_length == m_dxl_query_output_cols->Size());

	CDXLNode *project_list_dxl = NULL;
	
	const ULONG num_system_cols = md_rel->ColumnCount() - num_table_columns;
	const ULONG num_non_dropped_cols = md_rel->NonDroppedColsCount() - num_system_cols;
	if (num_non_dropped_cols > target_list_length)
	{
		// missing target list entries
		project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));
	}

	ULongPtrArray *source_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	ULONG target_list_pos = 0;
	for (ULONG ul = 0; ul < num_table_columns; ul++)
	{
		const IMDColumn *mdcol = md_rel->GetMdCol(ul);
		GPOS_ASSERT(!mdcol->IsSystemColumn());
		
		if (mdcol->IsDropped())
		{
			continue;
		}
		
		if (target_list_pos < target_list_length)
		{
			INT attno = mdcol->AttrNum();
			
			TargetEntry *target_entry = (TargetEntry *) gpdb::ListNth(m_query->targetList, target_list_pos);
			AttrNumber resno = target_entry->resno;

			if (attno == resno)
			{
				CDXLNode *dxl_column = (*m_dxl_query_output_cols)[target_list_pos];
				CDXLScalarIdent *dxl_ident = CDXLScalarIdent::Cast(dxl_column->GetOperator());
				source_array->Append(GPOS_NEW(m_memory_pool) ULONG(dxl_ident->MakeDXLColRef()->Id()));
				target_list_pos++;
				continue;
			}
		}

		// target entry corresponding to the tables column not found, therefore
		// add a project element with null value scalar child
		CDXLNode *proj_elem_dxlnode = CTranslatorUtils::CreateDXLProjElemConstNULL(m_memory_pool, m_md_accessor, m_colid_counter, mdcol);
		ULONG col_id = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->Id();
 	 	project_list_dxl->AddChild(proj_elem_dxlnode);
	 	source_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	CDXLLogicalInsert *insert_dxlnode = GPOS_NEW(m_memory_pool) CDXLLogicalInsert(m_memory_pool, table_descr, source_array);

	if (NULL != project_list_dxl)
	{
		GPOS_ASSERT(0 < project_list_dxl->Arity());
		
		CDXLNode *project_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		project_dxlnode->AddChild(project_list_dxl);
		project_dxlnode->AddChild(query_dxlnode);
		query_dxlnode = project_dxlnode;
	}

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, insert_dxlnode, query_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateCTASToDXL
//
//	@doc:
//		Translate a CTAS
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateCTASToDXL()
{
	GPOS_ASSERT(CMD_SELECT == m_query->commandType);
	GPOS_ASSERT(NULL != m_query->intoClause);

	m_is_ctas_query = true;
	CDXLNode *query_dxlnode = TranslateSelectQueryToDXL();
	
	IntoClause *into_clause = m_query->intoClause;
		
	CMDName *md_relname = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, into_clause->rel->relname);
	
	ColumnDescrDXLArray *col_descr_dxl_array = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);
	
	const ULONG num_columns = gpdb::ListLength(m_query->targetList);

	ULongPtrArray *source_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	IntPtrArray* var_typmods = GPOS_NEW(m_memory_pool) IntPtrArray(m_memory_pool);
	
	List *col_names = into_clause->colNames;
	for (ULONG ul = 0; ul < num_columns; ul++)
	{
		TargetEntry *target_entry = (TargetEntry *) gpdb::ListNth(m_query->targetList, ul);
		if (target_entry->resjunk)
		{
			continue;
		}
		AttrNumber resno = target_entry->resno;
		int var_typmod = gpdb::ExprTypeMod((Node*)target_entry->expr);
		var_typmods->Append(GPOS_NEW(m_memory_pool) INT(var_typmod));

		CDXLNode *dxl_column = (*m_dxl_query_output_cols)[ul];
		CDXLScalarIdent *dxl_ident = CDXLScalarIdent::Cast(dxl_column->GetOperator());
		source_array->Append(GPOS_NEW(m_memory_pool) ULONG(dxl_ident->MakeDXLColRef()->Id()));
		
		CMDName *md_colname = NULL;
		if (NULL != col_names && ul < gpdb::ListLength(col_names))
		{
			ColumnDef *col_def = (ColumnDef *) gpdb::ListNth(col_names, ul);
			md_colname = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, col_def->colname);
		}
		else
		{
			md_colname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, dxl_ident->MakeDXLColRef()->MdName()->GetMDName());
		}
		
		GPOS_ASSERT(NULL != md_colname);
		IMDId *mdid = dxl_ident->MDIdType();
		mdid->AddRef();
		CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
											(
											m_memory_pool,
											md_colname,
											m_colid_counter->next_id(),
											resno /* attno */,
											mdid,
											dxl_ident->TypeModifier(),
											false /* is_dropped */
											);
		col_descr_dxl_array->Append(dxl_col_descr);
	}

	IMDRelation::Ereldistrpolicy rel_distr_policy = IMDRelation::EreldistrRandom;
	ULongPtrArray *distribution_colids = NULL;
	
	if (NULL != m_query->intoPolicy)
	{
		rel_distr_policy = CTranslatorRelcacheToDXL::GetRelDistribution(m_query->intoPolicy);
		
		if (IMDRelation::EreldistrHash == rel_distr_policy)
		{
			distribution_colids = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

			for (ULONG ul = 0; ul < (ULONG) m_query->intoPolicy->nattrs; ul++)
			{
				AttrNumber attno = m_query->intoPolicy->attrs[ul];
				GPOS_ASSERT(0 < attno);
				distribution_colids->Append(GPOS_NEW(m_memory_pool) ULONG(attno - 1));
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
	m_has_distributed_tables = true;

	// TODO: Mar 5, 2014; reserve an OID
	OID oid = 1;
	CMDIdGPDB *mdid = GPOS_NEW(m_memory_pool) CMDIdGPDBCtas(oid);
	
	CMDName *md_tablespace_name = NULL;
	if (NULL != into_clause->tableSpaceName)
	{
		md_tablespace_name = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, into_clause->tableSpaceName);
	}
	
	CMDName *md_schema_name = NULL;
	if (NULL != into_clause->rel->schemaname)
	{
		md_schema_name = CDXLUtils::CreateMDNameFromCharArray(m_memory_pool, into_clause->rel->schemaname);
	}
	
	CDXLCtasStorageOptions::ECtasOnCommitAction ctas_commit_action = (CDXLCtasStorageOptions::ECtasOnCommitAction) into_clause->onCommit;
	
	IMDRelation::Erelstoragetype rel_storage_type = IMDRelation::ErelstorageHeap;
	CDXLCtasStorageOptions::DXLCtasOptionArray *ctas_storage_options = GetDXLCtasOptionArray(into_clause->options, &rel_storage_type);
	
	BOOL has_oids = gpdb::InterpretOidsOption(into_clause->options);
	CDXLLogicalCTAS *ctas_dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalCTAS
									(
									m_memory_pool,
									mdid,
									md_schema_name,
									md_relname, 
									col_descr_dxl_array, 
									GPOS_NEW(m_memory_pool) CDXLCtasStorageOptions(md_tablespace_name, ctas_commit_action, ctas_storage_options),
									rel_distr_policy,
									distribution_colids,  
									into_clause->rel->relpersistence == RELPERSISTENCE_TEMP,
									has_oids,
									rel_storage_type,
									source_array,
									var_typmods
									);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, ctas_dxlop, query_dxlnode);
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
	List *options,
	IMDRelation::Erelstoragetype *storage_type // output parameter: storage type
	)
{
	if (NULL == options)
	{
		return NULL;
	}

	GPOS_ASSERT(NULL != storage_type);
	
	CDXLCtasStorageOptions::DXLCtasOptionArray *ctas_storage_options = GPOS_NEW(m_memory_pool) CDXLCtasStorageOptions::DXLCtasOptionArray(m_memory_pool);
	ListCell *lc = NULL;
	BOOL is_ao_table = false;
	BOOL is_AOCO = false;
	BOOL is_parquet = false;
	
	CWStringConst str_append_only(GPOS_WSZ_LIT("appendonly"));
	CWStringConst str_orientation(GPOS_WSZ_LIT("orientation"));
	CWStringConst str_orientation_parquet(GPOS_WSZ_LIT("parquet"));
	CWStringConst str_orientation_column(GPOS_WSZ_LIT("column"));
	
	ForEach (lc, options)
	{
		DefElem *def_elem = (DefElem *) lfirst(lc);
		CWStringDynamic *name_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, def_elem->defname);
		CWStringDynamic *value_str = NULL;
		
		BOOL fNullArg = (NULL == def_elem->arg);

		// def_elem->arg is NULL for queries of the form "create table t with (oids) as ... "
		if (fNullArg)
		{
			// we represent null options as an empty arg string and set the IsNull flag on
			value_str = GPOS_NEW(m_memory_pool) CWStringDynamic(m_memory_pool);
		}
		else
		{
			value_str = ExtractStorageOptionStr(def_elem);

			if (name_str->Equals(&str_append_only) && value_str->Equals(CDXLTokens::GetDXLTokenStr(EdxltokenTrue)))
			{
				is_ao_table = true;
			}
			
			if (name_str->Equals(&str_orientation) && value_str->Equals(&str_orientation_column))
			{
				GPOS_ASSERT(!is_parquet);
				is_AOCO = true;
			}

			if (name_str->Equals(&str_orientation) && value_str->Equals(&str_orientation_parquet))
			{
				GPOS_ASSERT(!is_AOCO);
				is_parquet = true;
			}
		}

		NodeTag arg_type = T_Null;
		if (!fNullArg)
		{
			arg_type = def_elem->arg->type;
		}

		CDXLCtasStorageOptions::CDXLCtasOption *dxl_ctas_storage_option =
				GPOS_NEW(m_memory_pool) CDXLCtasStorageOptions::CDXLCtasOption(arg_type, name_str, value_str, fNullArg);
		ctas_storage_options->Append(dxl_ctas_storage_option);
	}
	if (is_AOCO)
	{
		*storage_type = IMDRelation::ErelstorageAppendOnlyCols;
	}
	else if (is_ao_table)
	{
		*storage_type = IMDRelation::ErelstorageAppendOnlyRows;
	}
	else if (is_parquet)
	{
		*storage_type = IMDRelation::ErelstorageAppendOnlyParquet;
	}
	
	return ctas_storage_options;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::ExtractStorageOptionStr
//
//	@doc:
//		Extract value for storage option
//
//---------------------------------------------------------------------------
CWStringDynamic *
CTranslatorQueryToDXL::ExtractStorageOptionStr
	(
	DefElem *def_elem
	)
{
	GPOS_ASSERT(NULL != def_elem);

	CHAR *value = gpdb::DefGetString(def_elem);

	CWStringDynamic *result_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, value);
	
	return result_str;
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
	ULONG *ctid,
	ULONG *segment_id
	)
{
	// ctid column id
	IMDId *mdid = CTranslatorUtils::GetSystemColType(m_memory_pool, SelfItemPointerAttributeNumber);
	*ctid = CTranslatorUtils::GetColId(m_query_level, m_query->resultRelation, SelfItemPointerAttributeNumber, mdid, m_var_to_colid_map);
	mdid->Release();

	// segmentid column id
	mdid = CTranslatorUtils::GetSystemColType(m_memory_pool, GpSegmentIdAttributeNumber);
	*segment_id = CTranslatorUtils::GetColId(m_query_level, m_query->resultRelation, GpSegmentIdAttributeNumber, mdid, m_var_to_colid_map);
	mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::GetTupleOidColId
//
//	@doc:
//		Obtains the id of the tuple oid column for the target table of a DML
//		update
//
//---------------------------------------------------------------------------
ULONG
CTranslatorQueryToDXL::GetTupleOidColId()
{
	IMDId *mdid = CTranslatorUtils::GetSystemColType(m_memory_pool, ObjectIdAttributeNumber);
	ULONG tuple_oid_colid = CTranslatorUtils::GetColId(m_query_level, m_query->resultRelation, ObjectIdAttributeNumber, mdid, m_var_to_colid_map);
	mdid->Release();
	return tuple_oid_colid;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateDeleteQueryToDXL
//
//	@doc:
//		Translate a delete stmt
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateDeleteQueryToDXL()
{
	GPOS_ASSERT(CMD_DELETE == m_query->commandType);
	GPOS_ASSERT(0 < m_query->resultRelation);

	CDXLNode *query_dxlnode = TranslateSelectQueryToDXL();
	const RangeTblEntry *rte = (RangeTblEntry *) gpdb::ListNth(m_query->rtable, m_query->resultRelation - 1);

	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_md_accessor, m_colid_counter, rte, &m_has_distributed_tables);
	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::RelHasTriggers(m_memory_pool, m_md_accessor, md_rel, Edxldmldelete))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("DELETE with triggers"));
	}

	ULONG ctid_colid = 0;
	ULONG segid_colid = 0;
	GetCtidAndSegmentId(&ctid_colid, &segid_colid);

	ULongPtrArray *delete_colid_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	const ULONG num_of_non_sys_cols = md_rel->ColumnCount();
	for (ULONG ul = 0; ul < num_of_non_sys_cols; ul++)
	{
		const IMDColumn *mdcol = md_rel->GetMdCol(ul);
		if (mdcol->IsSystemColumn() || mdcol->IsDropped())
		{
			continue;
		}

		ULONG col_id = CTranslatorUtils::GetColId(m_query_level, m_query->resultRelation, mdcol->AttrNum(), mdcol->MDIdType(), m_var_to_colid_map);
		delete_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	CDXLLogicalDelete *delete_dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalDelete(m_memory_pool, table_descr, ctid_colid, segid_colid, delete_colid_array);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, delete_dxlop, query_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateUpdateQueryToDXL
//
//	@doc:
//		Translate an update stmt
//
//---------------------------------------------------------------------------

CDXLNode *
CTranslatorQueryToDXL::TranslateUpdateQueryToDXL()
{
	GPOS_ASSERT(CMD_UPDATE == m_query->commandType);
	GPOS_ASSERT(0 < m_query->resultRelation);

	CDXLNode *query_dxlnode = TranslateSelectQueryToDXL();
	const RangeTblEntry *rte = (RangeTblEntry *) gpdb::ListNth(m_query->rtable, m_query->resultRelation - 1);

	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_md_accessor, m_colid_counter, rte, &m_has_distributed_tables);
	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());
	if (!optimizer_enable_dml_triggers && CTranslatorUtils::RelHasTriggers(m_memory_pool, m_md_accessor, md_rel, Edxldmlupdate))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("UPDATE with triggers"));
	}
	
	if (!optimizer_enable_dml_constraints && CTranslatorUtils::RelHasConstraints(md_rel))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("UPDATE with constraints"));
	}
	

	ULONG ctid_colid = 0;
	ULONG segmentid_colid = 0;
	GetCtidAndSegmentId(&ctid_colid, &segmentid_colid);
	
	ULONG tuple_oid_colid = 0;
	

	BOOL has_oids = md_rel->HasOids();
	if (has_oids)
	{
		tuple_oid_colid = GetTupleOidColId();
	}

	// get (resno -> colId) mapping of columns to be updated
	IntUlongHashMap *update_column_map = UpdatedColumnMapping();

	const ULONG num_of_non_sys_cols = md_rel->ColumnCount();
	ULongPtrArray *insert_colid_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	ULongPtrArray *delete_colid_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	for (ULONG ul = 0; ul < num_of_non_sys_cols; ul++)
	{
		const IMDColumn *mdcol = md_rel->GetMdCol(ul);
		if (mdcol->IsSystemColumn() || mdcol->IsDropped())
		{
			continue;
		}

		INT attno = mdcol->AttrNum();
		ULONG *updated_colid = update_column_map->Find(&attno);

		ULONG col_id = CTranslatorUtils::GetColId(m_query_level, m_query->resultRelation, attno, mdcol->MDIdType(), m_var_to_colid_map);

		// if the column is in the query outputs then use it
		// otherwise get the column id created by the child query
		if (NULL != updated_colid)
		{
			insert_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(*updated_colid));
		}
		else
		{
			insert_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
		}

		delete_colid_array->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	update_column_map->Release();
	CDXLLogicalUpdate *pdxlopupdate = GPOS_NEW(m_memory_pool) CDXLLogicalUpdate(m_memory_pool, table_descr, ctid_colid, segmentid_colid, delete_colid_array, insert_colid_array, has_oids, tuple_oid_colid);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopupdate, query_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::UpdatedColumnMapping
//
//	@doc:
// 		Return resno -> colId mapping of columns to be updated
//
//---------------------------------------------------------------------------
IntUlongHashMap *
CTranslatorQueryToDXL::UpdatedColumnMapping()
{
	IntUlongHashMap *update_column_map = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);

	ListCell *lc = NULL;
	ULONG ul = 0;
	ULONG output_columns = 0;
	ForEach (lc, m_query->targetList)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		ULONG resno = target_entry->resno;
		GPOS_ASSERT(0 < resno);

		// resjunk true columns may be now existing in the query tree, for instance
		// ctid column in case of relations, see rewriteTargetListUD in GPDB.
		// In ORCA, resjunk true columns (ex ctid) required to identify the tuple
		// are included later, so, its safe to not include them here in the output query list.
		// In planner, a MODIFYTABLE node is created on top of the plan instead of DML node,
		// once we plan generating MODIFYTABLE node from ORCA, we may revisit it.
		if (!target_entry->resjunk)
		{
			CDXLNode *dxl_column = (*m_dxl_query_output_cols)[ul];
			CDXLScalarIdent *dxl_ident = CDXLScalarIdent::Cast(
					dxl_column->GetOperator());
			ULONG col_id = dxl_ident->MakeDXLColRef()->Id();

			StoreAttnoColIdMapping(update_column_map, resno, col_id);
			output_columns++;
		}
		ul++;
	}

	GPOS_ASSERT(output_columns == m_dxl_query_output_cols->Size());
	return update_column_map;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::OIDFound
//
//	@doc:
// 		Helper to check if OID is included in given array of OIDs
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::OIDFound
	(
	OID oid,
	const OID oids[],
	ULONG size
	)
{
	BOOL found = false;
	for (ULONG ul = 0; !found && ul < size; ul++)
	{
		found = (oids[ul] == oid);
	}

	return found;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::IsLeadWindowFunc
//
//	@doc:
// 		Check if given operator is LEAD window function
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::IsLeadWindowFunc
	(
	CDXLOperator *dxlop
	)
{
	BOOL is_lead_func = false;
	if (EdxlopScalarWindowRef == dxlop->GetDXLOperator())
	{
		CDXLScalarWindowRef *winref_dxlop = CDXLScalarWindowRef::Cast(dxlop);
		const CMDIdGPDB *mdid_gpdb = CMDIdGPDB::CastMdid(winref_dxlop->FuncMdId());
		OID oid = mdid_gpdb->OidObjectId();
		is_lead_func =  OIDFound(oid, lead_func_oids, GPOS_ARRAY_SIZE(lead_func_oids));
	}

	return is_lead_func;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::IsLagWindowFunc
//
//	@doc:
// 		Check if given operator is LAG window function
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::IsLagWindowFunc
	(
	CDXLOperator *dxlop
	)
{
	BOOL is_lag = false;
	if (EdxlopScalarWindowRef == dxlop->GetDXLOperator())
	{
		CDXLScalarWindowRef *winref_dxlop = CDXLScalarWindowRef::Cast(dxlop);
		const CMDIdGPDB *mdid_gpdb = CMDIdGPDB::CastMdid(winref_dxlop->FuncMdId());
		OID oid = mdid_gpdb->OidObjectId();
		is_lag =  OIDFound(oid, lag_func_oids, GPOS_ARRAY_SIZE(lag_func_oids));
	}

	return is_lag;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateWindowFramForLeadLag
//
//	@doc:
// 		Manufacture window frame for lead/lag functions
//
//---------------------------------------------------------------------------
CDXLWindowFrame *
CTranslatorQueryToDXL::CreateWindowFramForLeadLag
	(
	BOOL is_lead_func,
	CDXLNode *dxl_offset
	)
	const
{
	EdxlFrameBoundary dxl_frame_lead = EdxlfbBoundedFollowing;
	EdxlFrameBoundary dxl_frame_trail = EdxlfbBoundedFollowing;
	if (!is_lead_func)
	{
		dxl_frame_lead = EdxlfbBoundedPreceding;
		dxl_frame_trail = EdxlfbBoundedPreceding;
	}

	CDXLNode *dxl_lead_edge = NULL;
	CDXLNode *dxl_trail_edge = NULL;
	if (NULL == dxl_offset)
	{
		dxl_lead_edge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, true /* fLeading */, dxl_frame_lead));
		dxl_trail_edge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, false /* fLeading */, dxl_frame_trail));

		dxl_lead_edge->AddChild(CTranslatorUtils::CreateDXLProjElemFromInt8Const(m_memory_pool, m_md_accessor, 1 /*iVal*/));
		dxl_trail_edge->AddChild(CTranslatorUtils::CreateDXLProjElemFromInt8Const(m_memory_pool, m_md_accessor, 1 /*iVal*/));
	}
	else
	{
		// overwrite frame edge types based on specified offset type
		if (EdxlopScalarConstValue != dxl_offset->GetOperator()->GetDXLOperator())
		{
			if (is_lead_func)
			{
				dxl_frame_lead = EdxlfbDelayedBoundedFollowing;
				dxl_frame_trail = EdxlfbDelayedBoundedFollowing;
			}
			else
			{
				dxl_frame_lead = EdxlfbDelayedBoundedPreceding;
				dxl_frame_trail = EdxlfbDelayedBoundedPreceding;
			}
		}
		dxl_lead_edge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, true /* fLeading */, dxl_frame_lead));
		dxl_trail_edge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, false /* fLeading */, dxl_frame_trail));

		dxl_offset->AddRef();
		dxl_lead_edge->AddChild(dxl_offset);
		dxl_offset->AddRef();
		dxl_trail_edge->AddChild(dxl_offset);
	}

	// manufacture a frame for LEAD/LAG function
	return GPOS_NEW(m_memory_pool) CDXLWindowFrame
							(
							m_memory_pool,
							EdxlfsRow, // frame specification
							EdxlfesNulls, // frame exclusion strategy is set to exclude NULLs in GPDB
							dxl_lead_edge,
							dxl_trail_edge
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
		CDXLNode *dxl_node_child = (*(*project_list_dxl)[ul])[0];
		CDXLOperator *dxlop = dxl_node_child->GetOperator();
		BOOL is_lead_func = IsLeadWindowFunc(dxlop);
		BOOL is_lag = IsLagWindowFunc(dxlop);
		if (is_lead_func || is_lag)
		{
			CDXLScalarWindowRef *winref_dxlop = CDXLScalarWindowRef::Cast(dxlop);
			CDXLWindowSpec *window_spec_dxlnode = (*window_spec_array)[winref_dxlop->GetWindSpecPos()];
			CMDName *mdname = NULL;
			if (NULL != window_spec_dxlnode->MdName())
			{
				mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, window_spec_dxlnode->MdName()->GetMDName());
			}

			// find if an offset is specified
			CDXLNode *dxl_offset = NULL;
			if (1 < dxl_node_child->Arity())
			{
				dxl_offset = (*dxl_node_child)[1];
			}

			// create LEAD/LAG frame
			CDXLWindowFrame *window_frame = CreateWindowFramForLeadLag(is_lead_func, dxl_offset);

			// create new window spec object
			window_spec_dxlnode->GetPartitionByColIdArray()->AddRef();
			window_spec_dxlnode->GetSortColListDXL()->AddRef();
			CDXLWindowSpec *pdxlwsNew =
				GPOS_NEW(m_memory_pool) CDXLWindowSpec
					(
					m_memory_pool,
					window_spec_dxlnode->GetPartitionByColIdArray(),
					mdname,
					window_spec_dxlnode->GetSortColListDXL(),
					window_frame
					);
			window_spec_array->Append(pdxlwsNew);

			// update win spec pos of LEAD/LAG function
			winref_dxlop->SetWinSpecPos(window_spec_array->Size() - 1);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateWindowSpecToDXL
//
//	@doc:
//		Translate window specs
//
//---------------------------------------------------------------------------
DXLWindowSpecArray *
CTranslatorQueryToDXL::TranslateWindowSpecToDXL
	(
	List *window_clause,
	IntUlongHashMap *sort_col_attno_to_colid_mapping,
	CDXLNode *project_list_dxl_node
	)
{
	GPOS_ASSERT(NULL != window_clause);
	GPOS_ASSERT(NULL != sort_col_attno_to_colid_mapping);
	GPOS_ASSERT(NULL != project_list_dxl_node);

	DXLWindowSpecArray *window_spec_array = GPOS_NEW(m_memory_pool) DXLWindowSpecArray(m_memory_pool);

	// translate window specification
	ListCell *lc;
	ForEach (lc, window_clause)
	{
		WindowClause *wc = (WindowClause *) lfirst(lc);
		ULongPtrArray *part_columns = TranslatePartColumns(wc->partitionClause, sort_col_attno_to_colid_mapping);

		CDXLNode *sort_col_list_dxl = NULL;
		CMDName *mdname = NULL;
		CDXLWindowFrame *window_frame = NULL;

		if (NULL != wc->name)
		{
			CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, wc->name);
			mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
			GPOS_DELETE(alias_str);
		}

		if (0 < gpdb::ListLength(wc->orderClause))
		{
			// create a sorting col list
			sort_col_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSortColList(m_memory_pool));

			DXLNodeArray *dxl_sort_cols = TranslateSortColumsToDXL(wc->orderClause, sort_col_attno_to_colid_mapping);
			const ULONG size = dxl_sort_cols->Size();
			for (ULONG ul = 0; ul < size; ul++)
			{
				CDXLNode *dxl_sort_clause = (*dxl_sort_cols)[ul];
				dxl_sort_clause->AddRef();
				sort_col_list_dxl->AddChild(dxl_sort_clause);
			}
			dxl_sort_cols->Release();
		}

		window_frame = m_scalar_translator->GetWindowFrame(wc->frameOptions,
						 wc->startOffset,
						 wc->endOffset,
						 m_var_to_colid_map,
						 project_list_dxl_node,
						 &m_has_distributed_tables);

		CDXLWindowSpec *window_spec_dxlnode = GPOS_NEW(m_memory_pool) CDXLWindowSpec(m_memory_pool, part_columns, mdname, sort_col_list_dxl, window_frame);
		window_spec_array->Append(window_spec_dxlnode);
	}

	return window_spec_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateWindowToDXL
//
//	@doc:
//		Translate a window operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateWindowToDXL
	(
	CDXLNode *dxl_node_child,
	List *target_list,
	List *window_clause,
	List *sort_clause,
	IntUlongHashMap *sort_col_attno_to_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping
	)
{
	if (0 == gpdb::ListLength(window_clause))
	{
		return dxl_node_child;
	}

	// translate target list entries
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	CDXLNode *new_child_proj_list_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));
	ListCell *lc = NULL;
	ULONG resno = 1;

	// target entries that are result of flattening join alias and 
	// are equivalent to a defined Window specs target entry
	List *omitted_target_entries = NIL; 
	List *resno_list = NIL;
	
	ForEach (lc, target_list)
	{
		BOOL insert_sort_info = true;
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		// create the DXL node holding the target list entry
		CDXLNode *proj_elem_dxlnode =  TranslateExprToDXLProject(target_entry->expr, target_entry->resname);
		ULONG col_id = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->Id();

		if (IsA(target_entry->expr, WindowFunc))
		{
			CTranslatorUtils::CheckAggregateWindowFn((Node*) target_entry->expr);
		}
		if (!target_entry->resjunk)
		{
			if (IsA(target_entry->expr, Var) || IsA(target_entry->expr, WindowFunc))
			{
				// add window functions and non-computed columns to the project list of the window operator
				project_list_dxl->AddChild(proj_elem_dxlnode);

				StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
			}
			else if (CTranslatorUtils::IsWindowSpec(target_entry, window_clause))
			{
				// add computed column used in window specification needed in the output columns
				// to the child's project list
				new_child_proj_list_dxlnode->AddChild(proj_elem_dxlnode);

				// construct a scalar identifier that points to the computed column and
				// add it to the project list of the window operator
				CMDName *mdname_alias = GPOS_NEW(m_memory_pool) CMDName
													(
													m_memory_pool,
													CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->GetMdNameAlias()->GetMDName()
													);
				CDXLNode *new_project_elem_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
													(
													m_memory_pool,
													GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, col_id, mdname_alias)
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
																					GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, mdname_alias->GetMDName()),
																					col_id,
																					GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::ExprType((Node*) target_entry->expr)),
																					gpdb::ExprTypeMod((Node*) target_entry->expr)
																					)
																		)
															);
				new_project_elem_dxlnode->AddChild(pdxlnPrElNewChild);
				project_list_dxl->AddChild(new_project_elem_dxlnode);

				StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
			}
			else
			{
				insert_sort_info = false;
				omitted_target_entries = gpdb::LAppend(omitted_target_entries, target_entry);
				resno_list = gpdb::LAppendInt(resno_list, resno);

				proj_elem_dxlnode->Release();
			}
		}
		else if (IsA(target_entry->expr, WindowFunc))
		{
			// computed columns used in the order by clause
			project_list_dxl->AddChild(proj_elem_dxlnode);
		}
		else if (!IsA(target_entry->expr, Var))
		{
			GPOS_ASSERT(CTranslatorUtils::IsWindowSpec(target_entry, window_clause));
			// computed columns used in the window specification
			new_child_proj_list_dxlnode->AddChild(proj_elem_dxlnode);
		}
		else
		{
			proj_elem_dxlnode->Release();
		}

		if (insert_sort_info)
		{
			AddSortingGroupingColumn(target_entry, sort_col_attno_to_colid_mapping, col_id);
		}

		resno++;
	}

	lc = NULL;

	// process target entries that are a result of flattening join alias
	ListCell *plcResno = NULL;
	ForBoth (lc, omitted_target_entries,
			plcResno, resno_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		INT resno = (INT) lfirst_int(plcResno);

		TargetEntry *te_window_spec = CTranslatorUtils::GetWindowSpecTargetEntry( (Node*) target_entry->expr, window_clause, target_list);
		if (NULL != te_window_spec)
		{
			const ULONG col_id = CTranslatorUtils::GetColId( (INT) te_window_spec->ressortgroupref, sort_col_attno_to_colid_mapping);
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
			AddSortingGroupingColumn(target_entry, sort_col_attno_to_colid_mapping, col_id);
		}
	}
	if (NIL != omitted_target_entries)
	{
		gpdb::GPDBFree(omitted_target_entries);
	}

	// translate window spec
	DXLWindowSpecArray *window_spec_array = TranslateWindowSpecToDXL(window_clause, sort_col_attno_to_colid_mapping, new_child_proj_list_dxlnode);

	CDXLNode *new_child_dxlnode = NULL;

	if (0 < new_child_proj_list_dxlnode->Arity())
	{
		// create a project list for the computed columns used in the window specification
		new_child_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		new_child_dxlnode->AddChild(new_child_proj_list_dxlnode);
		new_child_dxlnode->AddChild(dxl_node_child);
		dxl_node_child = new_child_dxlnode;
	}
	else
	{
		// clean up
		new_child_proj_list_dxlnode->Release();
	}

	if (!CTranslatorUtils::HasProjElem(project_list_dxl, EdxlopScalarWindowRef))
	{
		project_list_dxl->Release();
		window_spec_array->Release();

		return dxl_node_child;
	}

	// update window spec positions of LEAD/LAG functions
	UpdateLeadLagWinSpecPos(project_list_dxl, window_spec_array);

	CDXLLogicalWindow *dxl_window_op = GPOS_NEW(m_memory_pool) CDXLLogicalWindow(m_memory_pool, window_spec_array);
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxl_window_op);

	dxlnode->AddChild(project_list_dxl);
	dxlnode->AddChild(dxl_node_child);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslatePartColumns
//
//	@doc:
//		Translate the list of partition-by column identifiers
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorQueryToDXL::TranslatePartColumns
	(
	List *partition_by_clause,
	IntUlongHashMap *col_attno_colid_mapping
	)
	const
{
	ULongPtrArray *partcols = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);

	ListCell *lc = NULL;
	ForEach (lc, partition_by_clause)
	{
		Node *partition_clause = (Node*) lfirst(lc);
		GPOS_ASSERT(NULL != partition_clause);

		GPOS_ASSERT(IsA(partition_clause, SortGroupClause));
		SortGroupClause *sort_group_clause = (SortGroupClause *) partition_clause;

		// get the colid of the partition-by column
		ULONG col_id = CTranslatorUtils::GetColId((INT) sort_group_clause->tleSortGroupRef, col_attno_colid_mapping);

		partcols->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
	}

	return partcols;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateSortColumsToDXL
//
//	@doc:
//		Translate the list of sorting columns
//
//---------------------------------------------------------------------------
DXLNodeArray *
CTranslatorQueryToDXL::TranslateSortColumsToDXL
	(
	List *sort_clause,
	IntUlongHashMap *col_attno_colid_mapping
	)
	const
{
	DXLNodeArray *dxl_nodes = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

	ListCell *lc = NULL;
	ForEach (lc, sort_clause)
	{
		Node *pnodeSortCl = (Node*) lfirst(lc);
		GPOS_ASSERT(NULL != pnodeSortCl);

		GPOS_ASSERT(IsA(pnodeSortCl, SortGroupClause));

		SortGroupClause *sort_group_clause = (SortGroupClause *) pnodeSortCl;

		// get the colid of the sorting column
		const ULONG col_id = CTranslatorUtils::GetColId((INT) sort_group_clause->tleSortGroupRef, col_attno_colid_mapping);

		OID oid = sort_group_clause->sortop;

		// get operator name
		CMDIdGPDB *op_mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(oid);
		const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(op_mdid);

		const CWStringConst *str = md_scalar_op->Mdname().GetMDName();
		GPOS_ASSERT(NULL != str);

		CDXLScalarSortCol *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarSortCol
												(
												m_memory_pool,
												col_id,
												op_mdid,
												GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()),
												sort_group_clause->nulls_first
												);

		// create the DXL node holding the sorting col
		CDXLNode *sort_col_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

		dxl_nodes->Append(sort_col_dxlnode);
	}

	return dxl_nodes;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateLimitToDXLGroupBy
//
//	@doc:
//		Translate the list of sorting columns, limit offset and limit count
//		into a CDXLLogicalGroupBy node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateLimitToDXLGroupBy
	(
	List *sort_clause,
	Node *limit_count,
	Node *limit_offset_node,
	CDXLNode *dxl_node_child,
	IntUlongHashMap *grpcols_to_colid_mapping
	)
{
	if (0 == gpdb::ListLength(sort_clause) && NULL == limit_count && NULL == limit_offset_node)
	{
		return dxl_node_child;
	}

	// do not remove limit if it is immediately under a DML (JIRA: GPSQL-2669)
	// otherwise we may increase the storage size because there are less opportunities for compression
	BOOL is_limit_top_level = (m_is_top_query_dml && 1 == m_query_level) || (m_is_ctas_query && 0 == m_query_level);
	CDXLNode *limit_dxlnode =
			GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalLimit(m_memory_pool, is_limit_top_level));

	// create a sorting col list
	CDXLNode *sort_col_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSortColList(m_memory_pool));

	DXLNodeArray *dxl_sort_cols = TranslateSortColumsToDXL(sort_clause, grpcols_to_colid_mapping);
	const ULONG size = dxl_sort_cols->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CDXLNode *sort_col_dxlnode = (*dxl_sort_cols)[ul];
		sort_col_dxlnode->AddRef();
		sort_col_list_dxl->AddChild(sort_col_dxlnode);
	}
	dxl_sort_cols->Release();

	// create limit count
	CDXLNode *limit_count_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
										(
										m_memory_pool,
										GPOS_NEW(m_memory_pool) CDXLScalarLimitCount(m_memory_pool)
										);

	if (NULL != limit_count)
	{
		limit_count_dxlnode->AddChild(TranslateExprToDXL((Expr*) limit_count));
	}

	// create limit offset
	CDXLNode *limit_offset_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
										(
										m_memory_pool,
										GPOS_NEW(m_memory_pool) CDXLScalarLimitOffset(m_memory_pool)
										);

	if (NULL != limit_offset_node)
	{
		limit_offset_dxlnode->AddChild(TranslateExprToDXL((Expr*) limit_offset_node));
	}

	limit_dxlnode->AddChild(sort_col_list_dxl);
	limit_dxlnode->AddChild(limit_count_dxlnode);
	limit_dxlnode->AddChild(limit_offset_dxlnode);
	limit_dxlnode->AddChild(dxl_node_child);

	return limit_dxlnode;
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
	IntUlongHashMap *sort_grpref_to_colid_mapping,
	ULONG col_id
	)
	const
{
	if (0 < target_entry->ressortgroupref)
	{
		INT *key = GPOS_NEW(m_memory_pool) INT(target_entry->ressortgroupref);
		ULONG *value = GPOS_NEW(m_memory_pool) ULONG(col_id);

		// insert idx-colid mapping in the hash map
#ifdef GPOS_DEBUG
		BOOL is_res =
#endif // GPOS_DEBUG
				sort_grpref_to_colid_mapping->Insert(key, value);

		GPOS_ASSERT(is_res);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateSimpleGroupBy
//
//	@doc:
//		Translate a query with grouping clause into a CDXLLogicalGroupBy node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::CreateSimpleGroupBy
	(
	List *target_list,
	List *group_clause,
	CBitSet *grpby_cols_bitset,
	BOOL has_aggs,
	BOOL has_grouping_sets,
	CDXLNode *dxl_node_child,
	IntUlongHashMap *sort_grpref_to_colid_mapping,
	IntUlongHashMap *child_attno_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping
	)
{
	if (NULL == grpby_cols_bitset)
	{ 
		GPOS_ASSERT(!has_aggs);
		if (!has_grouping_sets)
		{
			// no group by needed and not part of a grouping sets query: 
			// propagate child columns to output columns
			IntUlongHashmapIter mi(child_attno_colid_mapping);
			while (mi.Advance())
			{
	#ifdef GPOS_DEBUG
				BOOL result =
	#endif // GPOS_DEBUG
				output_attno_to_colid_mapping->Insert(GPOS_NEW(m_memory_pool) INT(*(mi.Key())), GPOS_NEW(m_memory_pool) ULONG(*(mi.Value())));
				GPOS_ASSERT(result);
			}
		}
		// else:
		// in queries with grouping sets we may generate a branch corresponding to GB grouping sets ();
		// in that case do not propagate the child columns to the output hash map, as later
		// processing may introduce NULLs for those

		return dxl_node_child;
	}

	List *dqa_list = NIL;
	// construct the project list of the group-by operator
	CDXLNode *proj_list_grpby_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	ListCell *lc = NULL;
	ULONG num_dqa = 0;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG resno = target_entry->resno;

		TargetEntry *te_equivalent = CTranslatorUtils::GetGroupingColumnTargetEntry( (Node *) target_entry->expr, group_clause, target_list);

		BOOL is_grouping_col = grpby_cols_bitset->Get(target_entry->ressortgroupref) || (NULL != te_equivalent && grpby_cols_bitset->Get(te_equivalent->ressortgroupref));
		ULONG col_id = 0;

		if (is_grouping_col)
		{
			// find colid for grouping column
			col_id = CTranslatorUtils::GetColId(resno, child_attno_colid_mapping);
		}
		else if (IsA(target_entry->expr, Aggref))
		{
			if (IsA(target_entry->expr, Aggref) && ((Aggref *) target_entry->expr)->aggdistinct && !IsDuplicateDqaArg(dqa_list, (Aggref *) target_entry->expr))
			{
				dqa_list = gpdb::LAppend(dqa_list, gpdb::CopyObject(target_entry->expr));
				num_dqa++;
			}

			// create a project element for aggregate
			CDXLNode *proj_elem_dxlnode = TranslateExprToDXLProject(target_entry->expr, target_entry->resname);
			proj_list_grpby_dxlnode->AddChild(proj_elem_dxlnode);
			col_id = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->Id();
			AddSortingGroupingColumn(target_entry, sort_grpref_to_colid_mapping, col_id);
		}

		if (is_grouping_col || IsA(target_entry->expr, Aggref))
		{
			// add to the list of output columns
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
		}
		else if (0 == grpby_cols_bitset->Size() && !has_grouping_sets && !has_aggs)
		{
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
		}
	}

	if (1 < num_dqa && !optimizer_enable_multiple_distinct_aggs)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple Distinct Qualified Aggregates are disabled in the optimizer"));
	}

	// initialize the array of grouping columns
	ULongPtrArray *grouping_cols = CTranslatorUtils::GetGroupingColidArray(m_memory_pool, grpby_cols_bitset, sort_grpref_to_colid_mapping);

	// clean up
	if (NIL != dqa_list)
	{
		gpdb::ListFree(dqa_list);
	}

	return GPOS_NEW(m_memory_pool) CDXLNode
						(
						m_memory_pool,
						GPOS_NEW(m_memory_pool) CDXLLogicalGroupBy(m_memory_pool, grouping_cols),
						proj_list_grpby_dxlnode,
						dxl_node_child
						);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::IsDuplicateDqaArg
//
//	@doc:
//		Check if the argument of a DQA has already being used by another DQA
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::IsDuplicateDqaArg
	(
	List *dqa_list,
	Aggref *aggref
	)
{
	GPOS_ASSERT(NULL != aggref);

	if (NIL == dqa_list || 0 == gpdb::ListLength(dqa_list))
	{
		return false;
	}

	ListCell *lc = NULL;
	ForEach (lc, dqa_list)
	{
		Node *node = (Node *) lfirst(lc);
		GPOS_ASSERT(IsA(node, Aggref));

		if (gpdb::Equals(aggref->args, ((Aggref *) node)->args))
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateGroupingSets
//
//	@doc:
//		Translate a query with grouping sets
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateGroupingSets
	(
	FromExpr *from_expr,
	List *target_list,
	List *group_clause,
	BOOL has_aggs,
	IntUlongHashMap *sort_grpref_to_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping
	)
{
	const ULONG num_of_cols = gpdb::ListLength(target_list) + 1;

	if (NULL == group_clause)
	{
		IntUlongHashMap *child_attno_colid_mapping = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);

		CDXLNode *select_project_join_dxlnode = TranslateSelectProjectJoinToDXL(target_list, from_expr, sort_grpref_to_colid_mapping, child_attno_colid_mapping, group_clause);

		CBitSet *bitset = NULL;
		if (has_aggs)
		{ 
			bitset = GPOS_NEW(m_memory_pool) CBitSet(m_memory_pool);
		}
		
		// in case of aggregates, construct a group by operator
		CDXLNode *result_dxlnode = CreateSimpleGroupBy
								(
								target_list,
								group_clause,
								bitset,
								has_aggs,
								false, // has_grouping_sets
								select_project_join_dxlnode,
								sort_grpref_to_colid_mapping,
								child_attno_colid_mapping,
								output_attno_to_colid_mapping
								);

		// cleanup
		child_attno_colid_mapping->Release();
		CRefCount::SafeRelease(bitset);
		return result_dxlnode;
	}

	// grouping functions refer to grouping col positions, so construct a map pos->grouping column
	// while processing the grouping clause
	UlongUlongHashMap *grpcol_index_to_colid_mapping = GPOS_NEW(m_memory_pool) UlongUlongHashMap(m_memory_pool);
	CBitSet *unique_grp_cols_bitset = GPOS_NEW(m_memory_pool) CBitSet(m_memory_pool, num_of_cols);
	DrgPbs *bitset_array = CTranslatorUtils::GetColumnAttnosForGroupBy(m_memory_pool, group_clause, num_of_cols, grpcol_index_to_colid_mapping, unique_grp_cols_bitset);

	const ULONG num_of_grouping_sets = bitset_array->Size();

	if (1 == num_of_grouping_sets)
	{
		// simple group by
		IntUlongHashMap *child_attno_colid_mapping = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
		CDXLNode *select_project_join_dxlnode = TranslateSelectProjectJoinToDXL(target_list, from_expr, sort_grpref_to_colid_mapping, child_attno_colid_mapping, group_clause);

		// translate the groupby clauses into a logical group by operator
		CBitSet *bitset = (*bitset_array)[0];


		CDXLNode *groupby_dxlnode = CreateSimpleGroupBy
								(
								target_list,
								group_clause,
								bitset,
								has_aggs,
								false, // has_grouping_sets
								select_project_join_dxlnode,
								sort_grpref_to_colid_mapping,
								child_attno_colid_mapping,
								output_attno_to_colid_mapping
								);
		
		CDXLNode *result_dxlnode = CreateDXLProjectGroupingFuncs
									(
									target_list,
									groupby_dxlnode,
									bitset,
									output_attno_to_colid_mapping,
									grpcol_index_to_colid_mapping,
									sort_grpref_to_colid_mapping
									);

		child_attno_colid_mapping->Release();
		bitset_array->Release();
		unique_grp_cols_bitset->Release();
		grpcol_index_to_colid_mapping->Release();
		
		return result_dxlnode;
	}
	
	CDXLNode *result_dxlnode = CreateDXLUnionAllForGroupingSets
			(
			from_expr,
			target_list,
			group_clause,
			has_aggs,
			bitset_array,
			sort_grpref_to_colid_mapping,
			output_attno_to_colid_mapping,
			grpcol_index_to_colid_mapping
			);

	unique_grp_cols_bitset->Release();
	grpcol_index_to_colid_mapping->Release();
	
	return result_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateDXLUnionAllForGroupingSets
//
//	@doc:
//		Construct a union all for the given grouping sets
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::CreateDXLUnionAllForGroupingSets
	(
	FromExpr *from_expr,
	List *target_list,
	List *group_clause,
	BOOL has_aggs,
	DrgPbs *bitset_array,
	IntUlongHashMap *sort_grpref_to_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping,
	UlongUlongHashMap *grpcol_index_to_colid_mapping		// mapping pos->unique grouping columns for grouping func arguments
	)
{
	GPOS_ASSERT(NULL != bitset_array);
	GPOS_ASSERT(1 < bitset_array->Size());

	const ULONG num_of_grouping_sets = bitset_array->Size();
	CDXLNode *unionall_dxlnode = NULL;
	ULongPtrArray *colid_array_inner = NULL;

	const ULONG cte_id = m_cte_id_counter->next_id();
	
	// construct a CTE producer on top of the SPJ query
	IntUlongHashMap *phmiulSPJ = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
	IntUlongHashMap *phmiulSortgrouprefColIdProducer = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
	CDXLNode *select_project_join_dxlnode = TranslateSelectProjectJoinForGrpSetsToDXL(target_list, from_expr, phmiulSortgrouprefColIdProducer, phmiulSPJ, group_clause);

	// construct output colids
	ULongPtrArray *op_colid_array_cte_producer = ExtractColIds(m_memory_pool, phmiulSPJ);

	GPOS_ASSERT (NULL != m_dxl_cte_producers);
	
	CDXLLogicalCTEProducer *dxl_cte_prod_op = GPOS_NEW(m_memory_pool) CDXLLogicalCTEProducer(m_memory_pool, cte_id, op_colid_array_cte_producer);
	CDXLNode *cte_producer_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxl_cte_prod_op, select_project_join_dxlnode);
	m_dxl_cte_producers->Append(cte_producer_dxlnode);
	
	CMappingVarColId *pmapvarcolidOriginal = m_var_to_colid_map->CopyMapColId(m_memory_pool);
	
	for (ULONG ul = 0; ul < num_of_grouping_sets; ul++)
	{
		CBitSet *grouping_set_bitset = (*bitset_array)[ul];

		// remap columns
		ULongPtrArray *colid_array_cte_consumer = GenerateColIds(m_memory_pool, op_colid_array_cte_producer->Size());
		
		// reset col mapping with new consumer columns
		GPOS_DELETE(m_var_to_colid_map);
		m_var_to_colid_map = pmapvarcolidOriginal->CopyRemapColId(m_memory_pool, op_colid_array_cte_producer, colid_array_cte_consumer);
		
		IntUlongHashMap *phmiulSPJConsumer = RemapColIds(m_memory_pool, phmiulSPJ, op_colid_array_cte_producer, colid_array_cte_consumer);
		IntUlongHashMap *phmiulSortgrouprefColIdConsumer = RemapColIds(m_memory_pool, phmiulSortgrouprefColIdProducer, op_colid_array_cte_producer, colid_array_cte_consumer);

		// construct a CTE consumer
		CDXLNode *cte_consumer_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalCTEConsumer(m_memory_pool, cte_id, colid_array_cte_consumer));

		IntUlongHashMap *phmiulGroupBy = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
		CDXLNode *groupby_dxlnode = CreateSimpleGroupBy
					(
					target_list,
					group_clause,
					grouping_set_bitset,
					has_aggs,
					true, // has_grouping_sets
					cte_consumer_dxlnode,
					phmiulSortgrouprefColIdConsumer,
					phmiulSPJConsumer,
					phmiulGroupBy
					);

		// add a project list for the NULL values
		CDXLNode *project_dxlnode = CreateDXLProjectNullsForGroupingSets(target_list, groupby_dxlnode, grouping_set_bitset, phmiulSortgrouprefColIdConsumer, phmiulGroupBy, grpcol_index_to_colid_mapping);

		ULongPtrArray *pdrgpulColIdsOuter = CTranslatorUtils::GetOutputColIdsArray(m_memory_pool, target_list, phmiulGroupBy);
		if (NULL != unionall_dxlnode)
		{
			GPOS_ASSERT(NULL != colid_array_inner);
			ColumnDescrDXLArray *col_descr_dxl_array = CTranslatorUtils::GetColumnDescrDXLArray(m_memory_pool, target_list, pdrgpulColIdsOuter, true /* keep_res_junked */);

			pdrgpulColIdsOuter->AddRef();

			ULongPtrArray2D *input_col_ids = GPOS_NEW(m_memory_pool) ULongPtrArray2D(m_memory_pool);
			input_col_ids->Append(pdrgpulColIdsOuter);
			input_col_ids->Append(colid_array_inner);

			CDXLLogicalSetOp *dxl_setop = GPOS_NEW(m_memory_pool) CDXLLogicalSetOp(m_memory_pool, EdxlsetopUnionAll, col_descr_dxl_array, input_col_ids, false);
			unionall_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxl_setop, project_dxlnode, unionall_dxlnode);
		}
		else
		{
			unionall_dxlnode = project_dxlnode;
		}

		colid_array_inner = pdrgpulColIdsOuter;
		
		if (ul == num_of_grouping_sets - 1)
		{
			// add the sortgroup columns to output map of the last column
			ULONG te_pos = 0;
			ListCell *lc = NULL;
			ForEach (lc, target_list)
			{
				TargetEntry *target_entry = (TargetEntry *) lfirst(lc);

				INT sortgroupref = INT (target_entry->ressortgroupref);
				if (0 < sortgroupref && NULL != phmiulSortgrouprefColIdConsumer->Find(&sortgroupref))
				{
					// add the mapping information for sorting columns
					AddSortingGroupingColumn(target_entry, sort_grpref_to_colid_mapping, *(*colid_array_inner)[te_pos]);
				}

				te_pos++;
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
	colid_array_inner->Release();

	// compute output columns
	CDXLLogicalSetOp *pdxlopUnion = CDXLLogicalSetOp::Cast(unionall_dxlnode->GetOperator());

	ListCell *lc = NULL;
	ULONG output_col_idx = 0;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG resno = target_entry->resno;

		// note that all target list entries are kept in union all's output column
		// this is achieved by the keep_res_junked flag in CTranslatorUtils::GetColumnDescrDXLArray
		const CDXLColDescr *dxl_col_descr = pdxlopUnion->GetColumnDescrAt(output_col_idx);
		const ULONG col_id = dxl_col_descr->Id();
		output_col_idx++;

		if (!target_entry->resjunk)
		{
			// add non-resjunk columns to the hash map that maintains the output columns
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
		}
	}

	// cleanup
	bitset_array->Release();

	// construct a CTE anchor operator on top of the union all
	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalCTEAnchor(m_memory_pool, cte_id), unionall_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::DXLDummyConstTableGet
//
//	@doc:
//		Create a dummy constant table get (CTG) with a boolean true value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::DXLDummyConstTableGet() const
{

	// construct the schema of the const table
	ColumnDescrDXLArray *col_descr_dxl_array = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);

	const CMDTypeBoolGPDB *pmdtypeBool = dynamic_cast<const CMDTypeBoolGPDB *>(m_md_accessor->PtMDType<IMDTypeBool>(m_sysid));
	const CMDIdGPDB *mdid = CMDIdGPDB::CastMdid(pmdtypeBool->MDId());

	// empty column name
	CWStringConst str_unnamed_col(GPOS_WSZ_LIT(""));
	CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &str_unnamed_col);
	CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
										(
										m_memory_pool,
										mdname,
										m_colid_counter->next_id(),
										1 /* attno */,
										GPOS_NEW(m_memory_pool) CMDIdGPDB(mdid->OidObjectId()),
										default_type_modifier,
										false /* is_dropped */
										);
	col_descr_dxl_array->Append(dxl_col_descr);

	// create the array of datum arrays
	DXLDatumArrays *dispatch_identifier_datum_arrays = GPOS_NEW(m_memory_pool) DXLDatumArrays(m_memory_pool);
	
	// create a datum array
	DXLDatumArray *dxl_datum_array = GPOS_NEW(m_memory_pool) DXLDatumArray(m_memory_pool);

	Const *const_expr = (Const*) gpdb::MakeBoolConst(true /*value*/, false /*isnull*/);
	CDXLDatum *datum_dxl = m_scalar_translator->GetDatumVal(const_expr);
	gpdb::GPDBFree(const_expr);

	dxl_datum_array->Append(datum_dxl);
	dispatch_identifier_datum_arrays->Append(dxl_datum_array);

	CDXLLogicalConstTable *dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalConstTable(m_memory_pool, col_descr_dxl_array, dispatch_identifier_datum_arrays);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateSetOpToDXL
//
//	@doc:
//		Translate a set operation
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateSetOpToDXL
	(
	Node *setop_node,
	List *target_list,
	IntUlongHashMap *output_attno_to_colid_mapping
	)
{
	GPOS_ASSERT(IsA(setop_node, SetOperationStmt));
	SetOperationStmt *psetopstmt = (SetOperationStmt*) setop_node;
	GPOS_ASSERT(SETOP_NONE != psetopstmt->op);

	EdxlSetOpType setop_type = CTranslatorUtils::GetSetOpType(psetopstmt->op, psetopstmt->all);

	// translate the left and right child
	ULongPtrArray *leftchild_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	ULongPtrArray *rightchild_array = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
	MdidPtrArray *mdid_array_leftchild = GPOS_NEW(m_memory_pool) MdidPtrArray(m_memory_pool);
	MdidPtrArray *mdid_array_rightchild = GPOS_NEW(m_memory_pool) MdidPtrArray(m_memory_pool);

	CDXLNode *left_child_dxlnode = TranslateSetOpChild(psetopstmt->larg, leftchild_array, mdid_array_leftchild, target_list);
	CDXLNode *right_child_dxlnode = TranslateSetOpChild(psetopstmt->rarg, rightchild_array, mdid_array_rightchild, target_list);

	// mark outer references in input columns from left child
	ULONG *colid = GPOS_NEW_ARRAY(m_memory_pool, ULONG, leftchild_array->Size());
	BOOL *outer_ref_array = GPOS_NEW_ARRAY(m_memory_pool, BOOL, leftchild_array->Size());
	const ULONG size = leftchild_array->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		colid[ul] =  *(*leftchild_array)[ul];
		outer_ref_array[ul] = true;
	}
	CTranslatorUtils::MarkOuterRefs(colid, outer_ref_array, size, left_child_dxlnode);

	ULongPtrArray2D *input_col_ids = GPOS_NEW(m_memory_pool) ULongPtrArray2D(m_memory_pool);
	input_col_ids->Append(leftchild_array);
	input_col_ids->Append(rightchild_array);
	
	ULongPtrArray *output_col_ids =  CTranslatorUtils::GenerateColIds
												(
												m_memory_pool,
												target_list,
												mdid_array_leftchild,
												leftchild_array,
												outer_ref_array,
												m_colid_counter
												);
 	GPOS_ASSERT(output_col_ids->Size() == leftchild_array->Size());

 	GPOS_DELETE_ARRAY(colid);
 	GPOS_DELETE_ARRAY(outer_ref_array);

	BOOL is_cast_across_input = SetOpNeedsCast(target_list, mdid_array_leftchild) || SetOpNeedsCast(target_list, mdid_array_rightchild);
	
	DXLNodeArray *children_dxl_nodes  = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
	children_dxl_nodes->Append(left_child_dxlnode);
	children_dxl_nodes->Append(right_child_dxlnode);

	CDXLNode *dxlnode = CreateDXLSetOpFromColumns
						(
						setop_type,
						target_list,
						output_col_ids,
						input_col_ids,
						children_dxl_nodes,
						is_cast_across_input,
						false /* keep_res_junked */
						);

	CDXLLogicalSetOp *dxlop = CDXLLogicalSetOp::Cast(dxlnode->GetOperator());
	const ColumnDescrDXLArray *col_descr_dxl_array = dxlop->GetColumnDescrDXLArray();

	ULONG output_col_idx = 0;
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG resno = target_entry->resno;

		if (!target_entry->resjunk)
		{
			const CDXLColDescr *pdxlcdNew = (*col_descr_dxl_array)[output_col_idx];
			ULONG col_id = pdxlcdNew->Id();
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
			output_col_idx++;
		}
	}

	// clean up
	output_col_ids->Release();
	mdid_array_leftchild->Release();
	mdid_array_rightchild->Release();

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
CTranslatorQueryToDXL::CreateDXLSetOpFromColumns
	(
	EdxlSetOpType setop_type,
	List *output_target_list,
	ULongPtrArray *output_col_ids,
	ULongPtrArray2D *input_col_ids,
	DXLNodeArray *children_dxl_nodes,
	BOOL is_cast_across_input,
	BOOL keep_res_junked
	)
	const
{
	GPOS_ASSERT(NULL != output_target_list);
	GPOS_ASSERT(NULL != output_col_ids);
	GPOS_ASSERT(NULL != input_col_ids);
	GPOS_ASSERT(NULL != children_dxl_nodes);
	GPOS_ASSERT(1 < input_col_ids->Size());
	GPOS_ASSERT(1 < children_dxl_nodes->Size());

	// positions of output columns in the target list
	ULongPtrArray *output_col_pos = CTranslatorUtils::GetPosInTargetList(m_memory_pool, output_target_list, keep_res_junked);

	const ULONG num_of_cols = output_col_ids->Size();
	ULongPtrArray *input_first_child_array = (*input_col_ids)[0];
	GPOS_ASSERT(num_of_cols == input_first_child_array->Size());
	GPOS_ASSERT(num_of_cols == output_col_ids->Size());

	CBitSet *bitset = GPOS_NEW(m_memory_pool) CBitSet(m_memory_pool);

	// project list to maintain the casting of the duplicate input columns
	CDXLNode *new_child_proj_list_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	ULongPtrArray *pdrgpulInputFirstChildNew = GPOS_NEW(m_memory_pool) ULongPtrArray (m_memory_pool);
	ColumnDescrDXLArray *pdrgpdxlcdOutput = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);
	for (ULONG ul = 0; ul < num_of_cols; ul++)
	{
		ULONG colid_output = *(*output_col_ids)[ul];
		ULONG colid_input = *(*input_first_child_array)[ul];

		BOOL is_col_exists = bitset->Get(colid_input);
		BOOL is_casted_col = (colid_output != colid_input);

		ULONG target_list_pos = *(*output_col_pos)[ul];
		TargetEntry *target_entry = (TargetEntry*) gpdb::ListNth(output_target_list, target_list_pos);
		GPOS_ASSERT(NULL != target_entry);

		CDXLColDescr *pdxlcdOutput = NULL;
		if (!is_col_exists)
		{
			bitset->ExchangeSet(colid_input);
			pdrgpulInputFirstChildNew->Append(GPOS_NEW(m_memory_pool) ULONG(colid_input));

			pdxlcdOutput = CTranslatorUtils::GetColumnDescrAt(m_memory_pool, target_entry, colid_output, ul + 1);
		}
		else
		{
			// we add a dummy-cast to distinguish between the output columns of the union
			ULONG ulColIdNew = m_colid_counter->next_id();
			pdrgpulInputFirstChildNew->Append(GPOS_NEW(m_memory_pool) ULONG(ulColIdNew));

			ULONG ulColIdUnionOutput = ulColIdNew;
			if (is_casted_col)
			{
				// create new output column id since current colid denotes its duplicate
				ulColIdUnionOutput = m_colid_counter->next_id();
			}

			pdxlcdOutput = CTranslatorUtils::GetColumnDescrAt(m_memory_pool, target_entry, ulColIdUnionOutput, ul + 1);
			CDXLNode *proj_elem_dxlnode = CTranslatorUtils::CreateDummyProjectElem(m_memory_pool, colid_input, ulColIdNew, pdxlcdOutput);

			new_child_proj_list_dxlnode->AddChild(proj_elem_dxlnode);
		}

		pdrgpdxlcdOutput->Append(pdxlcdOutput);
	}

	input_col_ids->Replace(0, pdrgpulInputFirstChildNew);

	if (0 < new_child_proj_list_dxlnode->Arity())
	{
		// create a project node for the dummy casted columns
		CDXLNode *pdxlnFirstChild = (*children_dxl_nodes)[0];
		pdxlnFirstChild->AddRef();
		CDXLNode *new_child_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		new_child_dxlnode->AddChild(new_child_proj_list_dxlnode);
		new_child_dxlnode->AddChild(pdxlnFirstChild);

		children_dxl_nodes->Replace(0, new_child_dxlnode);
	}
	else
	{
		new_child_proj_list_dxlnode->Release();
	}

	CDXLLogicalSetOp *dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalSetOp
											(
											m_memory_pool,
											setop_type,
											pdrgpdxlcdOutput,
											input_col_ids,
											is_cast_across_input
											);
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop, children_dxl_nodes);

	bitset->Release();
	output_col_pos->Release();

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::SetOpNeedsCast
//
//	@doc:
//		Check if the set operation need to cast any of its input columns
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::SetOpNeedsCast
	(
	List *target_list,
	MdidPtrArray *input_col_mdids
	)
	const
{
	GPOS_ASSERT(NULL != input_col_mdids);
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(input_col_mdids->Size() <= gpdb::ListLength(target_list)); // there may be resjunked columns

	ULONG col_pos_idx = 0;
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		OID oidExprType = gpdb::ExprType((Node*) target_entry->expr);
		if (!target_entry->resjunk)
		{
			IMDId *mdid = (*input_col_mdids)[col_pos_idx];
			if (CMDIdGPDB::CastMdid(mdid)->OidObjectId() != oidExprType)
			{
				return true;
			}
			col_pos_idx++;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateSetOpChild
//
//	@doc:
//		Translate the child of a set operation
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateSetOpChild
	(
	Node *child_node,
	ULongPtrArray *pdrgpul,
	MdidPtrArray *input_col_mdids,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != pdrgpul);
	GPOS_ASSERT(NULL != input_col_mdids);

	if (IsA(child_node, RangeTblRef))
	{
		RangeTblRef *prtref = (RangeTblRef*) child_node;
		const ULONG rti = prtref->rtindex;
		const RangeTblEntry *rte = (RangeTblEntry *) gpdb::ListNth(m_query->rtable, rti - 1);

		if (RTE_SUBQUERY == rte->rtekind)
		{
			Query *pqueryDerTbl = CTranslatorUtils::FixUnknownTypeConstant(rte->subquery, target_list);
			GPOS_ASSERT(NULL != pqueryDerTbl);

			CMappingVarColId *var_col_id_mapping = m_var_to_colid_map->CopyMapColId(m_memory_pool);
			CTranslatorQueryToDXL trquerytodxl
					(
					m_memory_pool,
					m_md_accessor,
					m_colid_counter,
					m_cte_id_counter,
					var_col_id_mapping,
					pqueryDerTbl,
					m_query_level + 1,
					IsDMLQuery(),
					m_query_level_to_cte_map
					);

			// translate query representing the derived table to its DXL representation
			CDXLNode *dxlnode = trquerytodxl.TranslateSelectQueryToDXL();
			GPOS_ASSERT(NULL != dxlnode);

			DXLNodeArray *cte_dxlnode_array = trquerytodxl.GetCTEs();
			CUtils::AddRefAppend(m_dxl_cte_producers, cte_dxlnode_array);
			m_has_distributed_tables = m_has_distributed_tables || trquerytodxl.HasDistributedTables();

			// get the output columns of the derived table
			DXLNodeArray *dxl_nodes = trquerytodxl.GetQueryOutputCols();
			GPOS_ASSERT(dxl_nodes != NULL);
			const ULONG length = dxl_nodes->Size();
			for (ULONG ul = 0; ul < length; ul++)
			{
				CDXLNode *pdxlnCurr = (*dxl_nodes)[ul];
				CDXLScalarIdent *dxl_sc_ident = CDXLScalarIdent::Cast(pdxlnCurr->GetOperator());
				ULONG *colid = GPOS_NEW(m_memory_pool) ULONG(dxl_sc_ident->MakeDXLColRef()->Id());
				pdrgpul->Append(colid);

				IMDId *pmdidCol = dxl_sc_ident->MDIdType();
				GPOS_ASSERT(NULL != pmdidCol);
				pmdidCol->AddRef();
				input_col_mdids->Append(pmdidCol);
			}

			return dxlnode;
		}
	}
	else if (IsA(child_node, SetOperationStmt))
	{
		IntUlongHashMap *output_attno_to_colid_mapping = GPOS_NEW(m_memory_pool) IntUlongHashMap(m_memory_pool);
		CDXLNode *dxlnode = TranslateSetOpToDXL(child_node, target_list, output_attno_to_colid_mapping);

		// cleanup
		output_attno_to_colid_mapping->Release();

		const ColumnDescrDXLArray *col_descr_dxl_array = CDXLLogicalSetOp::Cast(dxlnode->GetOperator())->GetColumnDescrDXLArray();
		GPOS_ASSERT(NULL != col_descr_dxl_array);
		const ULONG length = col_descr_dxl_array->Size();
		for (ULONG ul = 0; ul < length; ul++)
		{
			const CDXLColDescr *dxl_col_descr = (*col_descr_dxl_array)[ul];
			ULONG *colid = GPOS_NEW(m_memory_pool) ULONG(dxl_col_descr->Id());
			pdrgpul->Append(colid);

			IMDId *pmdidCol = dxl_col_descr->MDIdType();
			GPOS_ASSERT(NULL != pmdidCol);
			pmdidCol->AddRef();
			input_col_mdids->Append(pmdidCol);
		}

		return dxlnode;
	}

	CHAR *sz = (CHAR*) gpdb::NodeToString(const_cast<Node*>(child_node));
	CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, sz);

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, str->GetBuffer());
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateFromExprToDXL
//
//	@doc:
//		Translate the FromExpr on a GPDB query into either a CDXLLogicalJoin
//		or a CDXLLogicalGet
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateFromExprToDXL
	(
	FromExpr *from_expr
	)
{
	CDXLNode *dxlnode = NULL;

	if (0 == gpdb::ListLength(from_expr->fromlist))
	{
		dxlnode = DXLDummyConstTableGet();
	}
	else
	{
		if (1 == gpdb::ListLength(from_expr->fromlist))
		{
			Node *node = (Node*) gpdb::ListNth(from_expr->fromlist, 0);
			GPOS_ASSERT(NULL != node);
			dxlnode = TranslateFromClauseToDXL(node);
		}
		else
		{
			// In DXL, we represent an n-ary join (where n>2) by an inner join with condition true.
			// The join conditions represented in the FromExpr->quals is translated
			// into a CDXLLogicalSelect on top of the CDXLLogicalJoin

			dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalJoin(m_memory_pool, EdxljtInner));

			ListCell *lc = NULL;
			ForEach (lc, from_expr->fromlist)
			{
				Node *node = (Node*) lfirst(lc);
				CDXLNode *dxl_node_child = TranslateFromClauseToDXL(node);
				dxlnode->AddChild(dxl_node_child);
			}
		}
	}

	// translate the quals
	Node *pnodeQuals = from_expr->quals;
	CDXLNode *pdxlnCond = NULL;
	if (NULL != pnodeQuals)
	{
		pdxlnCond = TranslateExprToDXL( (Expr*) pnodeQuals);
	}

	if (1 >= gpdb::ListLength(from_expr->fromlist))
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
			pdxlnCond = CreateDXLConstValueTrue();
		}

		dxlnode->AddChild(pdxlnCond);
	}

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateFromClauseToDXL
//
//	@doc:
//		Returns a CDXLNode representing a from clause entry which can either be
//		(1) a fromlist entry in the FromExpr or (2) left/right child of a JoinExpr
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateFromClauseToDXL
	(
	Node *node
	)
{
	GPOS_ASSERT(NULL != node);

	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *prtref = (RangeTblRef *) node;
		ULONG rti = prtref->rtindex ;
		const RangeTblEntry *rte = (RangeTblEntry *) gpdb::ListNth(m_query->rtable, rti - 1);
		GPOS_ASSERT(NULL != rte);

		if (rte->forceDistRandom)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("gp_dist_random"));
		}

		static const SRTETranslator dxlop_translator_func_mapping_array[] =
		{
			{RTE_RELATION, &CTranslatorQueryToDXL::TranslateRTEToDXLLogicalGet},
			{RTE_VALUES, &CTranslatorQueryToDXL::TranslateValueScanRTEToDXL},
			{RTE_CTE, &CTranslatorQueryToDXL::TranslateCTEToDXL},
			{RTE_SUBQUERY, &CTranslatorQueryToDXL::TranslateDerivedTablesToDXL},
			{RTE_FUNCTION, &CTranslatorQueryToDXL::TranslateTVFToDXL},
		};
		
		const ULONG num_of_translators = GPOS_ARRAY_SIZE(dxlop_translator_func_mapping_array);
		
		// find translator for the rtekind
		DXLNodeToLogicalFunc dxlnode_to_logical_funct = NULL;
		for (ULONG ul = 0; ul < num_of_translators; ul++)
		{
			SRTETranslator elem = dxlop_translator_func_mapping_array[ul];
			if (rte->rtekind == elem.m_rtekind)
			{
				dxlnode_to_logical_funct = elem.dxlnode_to_logical_funct;
				break;
			}
		}
		
		if (NULL == dxlnode_to_logical_funct)
		{
			UnsupportedRTEKind(rte->rtekind);

			return NULL;
		}
		
		return (this->*dxlnode_to_logical_funct)(rte, rti, m_query_level);
	}

	if (IsA(node, JoinExpr))
	{
		return TranslateJoinExprInFromToDXL((JoinExpr*) node);
	}

	CHAR *sz = (CHAR*) gpdb::NodeToString(const_cast<Node*>(node));
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

	static const SRTENameElem rte_name_map[] =
		{
		{RTE_JOIN, GPOS_WSZ_LIT("RangeTableEntry of type Join")},
		{RTE_VOID, GPOS_WSZ_LIT("RangeTableEntry of type Void")},
		{RTE_TABLEFUNCTION, GPOS_WSZ_LIT("RangeTableEntry of type Table Function")}
		};

	const ULONG length = GPOS_ARRAY_SIZE(rte_name_map);
	for (ULONG ul = 0; ul < length; ul++)
	{
		SRTENameElem mapelem = rte_name_map[ul];

		if (mapelem.m_rtekind == rtekind)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, mapelem.m_rte_name);
		}
	}

	GPOS_ASSERT(!"Unrecognized RTE kind");
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateRTEToDXLLogicalGet
//
//	@doc:
//		Returns a CDXLNode representing a from relation range table entry
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateRTEToDXLLogicalGet
	(
	const RangeTblEntry *rte,
	ULONG rti,
	ULONG //current_query_level 
	)
{
	if (false == rte->inh)
	{
		GPOS_ASSERT(RTE_RELATION == rte->rtekind);
		// RangeTblEntry::inh is set to false iff there is ONLY in the FROM
		// clause. c.f. transformTableEntry, called from transformFromClauseItem
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("ONLY in the FROM clause"));
	}

	// construct table descriptor for the scan node from the range table entry
	CDXLTableDescr *table_descr = CTranslatorUtils::GetTableDescr(m_memory_pool, m_md_accessor, m_colid_counter, rte, &m_has_distributed_tables);

	CDXLLogicalGet *dxlop = NULL;
	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());
	if (IMDRelation::ErelstorageExternal == md_rel->RetrieveRelStorageType())
	{
		dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalExternalGet(m_memory_pool, table_descr);
	}
	else
	{
		dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalGet(m_memory_pool, table_descr);
	}

	CDXLNode *pdxlnGet = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// make note of new columns from base relation
	m_var_to_colid_map->LoadTblColumns(m_query_level, rti, table_descr);

	return pdxlnGet;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateValueScanRTEToDXL
//
//	@doc:
//		Returns a CDXLNode representing a range table entry of values
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateValueScanRTEToDXL
	(
	const RangeTblEntry *rte,
	ULONG rti,
	ULONG current_query_level
	)
{
	List *plTuples = rte->values_lists;
	GPOS_ASSERT(NULL != plTuples);

	const ULONG ulValues = gpdb::ListLength(plTuples);
	GPOS_ASSERT(0 < ulValues);

	// children of the UNION ALL
	DXLNodeArray *dxl_nodes = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

	// array of datum arrays for Values
	DXLDatumArrays *pdrgpdrgpdxldatumValues = GPOS_NEW(m_memory_pool) DXLDatumArrays(m_memory_pool);

	// array of input colid arrays
	ULongPtrArray2D *input_col_ids = GPOS_NEW(m_memory_pool) ULongPtrArray2D(m_memory_pool);

	// array of column descriptor for the UNION ALL operator
	ColumnDescrDXLArray *col_descr_dxl_array = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);

	// translate the tuples in the value scan
	ULONG ulTuplePos = 0;
	ListCell *plcTuple = NULL;
	GPOS_ASSERT(NULL != rte->eref);

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
		DXLDatumArray *dxl_datum_array = GPOS_NEW(m_memory_pool) DXLDatumArray(m_memory_pool);

		// array of column descriptors for the CTG containing the datum array
		ColumnDescrDXLArray *dxl_column_descriptors = GPOS_NEW(m_memory_pool) ColumnDescrDXLArray(m_memory_pool);

		List *col_names = rte->eref->colnames;
		GPOS_ASSERT(NULL != col_names);
		GPOS_ASSERT(gpdb::ListLength(plTuple) == gpdb::ListLength(col_names));

		// translate the columns
		ULONG col_pos_idx = 0;
		ListCell *plcColumn = NULL;
		ForEach (plcColumn, plTuple)
		{
			Expr *expr = (Expr *) lfirst(plcColumn);

			CHAR *col_name_char_array = (CHAR *) strVal(gpdb::ListNth(col_names, col_pos_idx));
			ULONG col_id = gpos::ulong_max;
			if (IsA(expr, Const))
			{
				// extract the datum
				Const *const_expr = (Const *) expr;
				CDXLDatum *datum_dxl = m_scalar_translator->GetDatumVal(const_expr);
				dxl_datum_array->Append(datum_dxl);

				col_id = m_colid_counter->next_id();

				CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, col_name_char_array);
				CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
				GPOS_DELETE(alias_str);

				CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
													(
													m_memory_pool,
													mdname,
													col_id,
													col_pos_idx + 1 /* attno */,
													GPOS_NEW(m_memory_pool) CMDIdGPDB(const_expr->consttype),
													const_expr->consttypmod,
													false /* is_dropped */
													);

				if (0 == ulTuplePos)
				{
					dxl_col_descr->AddRef();
					col_descr_dxl_array->Append(dxl_col_descr);
				}
				dxl_column_descriptors->Append(dxl_col_descr);
			}
			else
			{
				fAllConstant = false;
				// translate the scalar expression into a project element
				CDXLNode *proj_elem_dxlnode = TranslateExprToDXLProject(expr, col_name_char_array, true /* insist_new_colids */ );
				pdrgpdxlnPrEl->Append(proj_elem_dxlnode);
				col_id = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->Id();

				if (0 == ulTuplePos)
				{
					CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, col_name_char_array);
					CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
					GPOS_DELETE(alias_str);

					CDXLColDescr *dxl_col_descr = GPOS_NEW(m_memory_pool) CDXLColDescr
														(
														m_memory_pool,
														mdname,
														col_id,
														col_pos_idx + 1 /* attno */,
														GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::ExprType((Node*) expr)),
														gpdb::ExprTypeMod((Node*) expr),
														false /* is_dropped */
														);
					col_descr_dxl_array->Append(dxl_col_descr);
				}
			}

			GPOS_ASSERT(gpos::ulong_max != col_id);

			pdrgpulColIds->Append(GPOS_NEW(m_memory_pool) ULONG(col_id));
			col_pos_idx++;
		}

		dxl_nodes->Append(TranslateColumnValuesToDXL(dxl_datum_array, dxl_column_descriptors, pdrgpdxlnPrEl));
		if (fAllConstant)
		{
			dxl_datum_array->AddRef();
			pdrgpdrgpdxldatumValues->Append(dxl_datum_array);
		}

		input_col_ids->Append(pdrgpulColIds);
		ulTuplePos++;

		// cleanup
		dxl_datum_array->Release();
		pdrgpdxlnPrEl->Release();
		dxl_column_descriptors->Release();
	}

	GPOS_ASSERT(NULL != col_descr_dxl_array);

	if (fAllConstant)
	{
		// create Const Table DXL Node
		CDXLLogicalConstTable *dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalConstTable(m_memory_pool, col_descr_dxl_array, pdrgpdrgpdxldatumValues);
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

		// make note of new columns from Value Scan
		m_var_to_colid_map->LoadColumns(m_query_level, rti, dxlop->GetColumnDescrDXLArray());

		// cleanup
		dxl_nodes->Release();
		input_col_ids->Release();

		return dxlnode;
	}
	else if (1 < ulValues)
	{
		// create a UNION ALL operator
		CDXLLogicalSetOp *dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalSetOp(m_memory_pool, EdxlsetopUnionAll, col_descr_dxl_array, input_col_ids, false);
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop, dxl_nodes);

		// make note of new columns from UNION ALL
		m_var_to_colid_map->LoadColumns(m_query_level, rti, dxlop->GetColumnDescrDXLArray());
		pdrgpdrgpdxldatumValues->Release();

		return dxlnode;
	}

	GPOS_ASSERT(1 == dxl_nodes->Size());

	CDXLNode *dxlnode = (*dxl_nodes)[0];
	dxlnode->AddRef();

	// make note of new columns
	m_var_to_colid_map->LoadColumns(m_query_level, rti, col_descr_dxl_array);

	//cleanup
	pdrgpdrgpdxldatumValues->Release();
	dxl_nodes->Release();
	input_col_ids->Release();
	col_descr_dxl_array->Release();

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateColumnValuesToDXL
//
//	@doc:
//		Generate a DXL node from column values, where each column value is 
//		either a datum or scalar expression represented as project element.
//		Each datum is associated with a column descriptors used by the CTG
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateColumnValuesToDXL
	(
	DXLDatumArray *pdrgpdxldatumCTG,
	ColumnDescrDXLArray *dxl_column_descriptors,
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
		pdxlnCTG = DXLDummyConstTableGet();
	}
	else 
	{
		// create the array of datum arrays
		DXLDatumArrays *pdrgpdrgpdxldatumCTG = GPOS_NEW(m_memory_pool) DXLDatumArrays(m_memory_pool);
		
		pdrgpdxldatumCTG->AddRef();
		pdrgpdrgpdxldatumCTG->Append(pdrgpdxldatumCTG);
		
		dxl_column_descriptors->AddRef();
		CDXLLogicalConstTable *dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalConstTable(m_memory_pool, dxl_column_descriptors, pdrgpdrgpdxldatumCTG);
		
		pdxlnCTG = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);
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
	
	CDXLNode *project_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
											(
											m_memory_pool,
											GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool),
											project_list_dxl,
											pdxlnCTG
											);

	return project_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateTVFToDXL
//
//	@doc:
//		Returns a CDXLNode representing a from relation range table entry
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateTVFToDXL
	(
	const RangeTblEntry *rte,
	ULONG rti,
	ULONG //current_query_level
	)
{
	GPOS_ASSERT(NULL != rte->funcexpr);

	// if this is a folded function expression, generate a project over a CTG
	if (!IsA(rte->funcexpr, FuncExpr))
	{
		CDXLNode *pdxlnCTG = DXLDummyConstTableGet();

		CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

		CDXLNode *proj_elem_dxlnode =  TranslateExprToDXLProject((Expr *) rte->funcexpr, rte->eref->aliasname, true /* insist_new_colids */);
		project_list_dxl->AddChild(proj_elem_dxlnode);

		CDXLNode *project_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		project_dxlnode->AddChild(project_list_dxl);
		project_dxlnode->AddChild(pdxlnCTG);

		m_var_to_colid_map->LoadProjectElements(m_query_level, rti, project_list_dxl);

		return project_dxlnode;
	}

	CDXLLogicalTVF *pdxlopTVF = CTranslatorUtils::ConvertToCDXLLogicalTVF(m_memory_pool, m_md_accessor, m_colid_counter, rte);
	CDXLNode *tvf_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopTVF);

	// make note of new columns from function
	m_var_to_colid_map->LoadColumns(m_query_level, rti, pdxlopTVF->GetColumnDescrDXLArray());

	FuncExpr *func_expr = (FuncExpr *) rte->funcexpr;
	BOOL fSubqueryInArgs = false;

	// check if arguments contain SIRV functions
	if (NIL != func_expr->args && HasSirvFunctions((Node *) func_expr->args))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}

	ListCell *lc = NULL;
	ForEach (lc, func_expr->args)
	{
		Node *pnodeArg = (Node *) lfirst(lc);
		fSubqueryInArgs = fSubqueryInArgs || CTranslatorUtils::HasSubquery(pnodeArg);
		CDXLNode *pdxlnFuncExprArg =
				m_scalar_translator->CreateScalarOpFromExpr((Expr *) pnodeArg, m_var_to_colid_map, &m_has_distributed_tables);
		GPOS_ASSERT(NULL != pdxlnFuncExprArg);
		tvf_dxlnode->AddChild(pdxlnFuncExprArg);
	}

	CMDIdGPDB *mdid_func = GPOS_NEW(m_memory_pool) CMDIdGPDB(func_expr->funcid);
	const IMDFunction *pmdfunc = m_md_accessor->RetrieveFunc(mdid_func);
	if (fSubqueryInArgs && IMDFunction::EfsVolatile == pmdfunc->GetFuncStability())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Volatile functions with subqueries in arguments"));
	}
	mdid_func->Release();

	return tvf_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateCTEToDXL
//
//	@doc:
//		Translate a common table expression into CDXLNode
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateCTEToDXL
	(
	const RangeTblEntry *rte,
	ULONG rti,
	ULONG current_query_level
	)
{
	const ULONG ulCteQueryLevel = current_query_level - rte->ctelevelsup;
	const CCTEListEntry *cte_list_entry = m_query_level_to_cte_map->Find(&ulCteQueryLevel);
	if (NULL == cte_list_entry)
	{
		// TODO: Sept 09 2013, remove temporary fix  (revert exception to assert) to avoid crash during algebrization
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiQuery2DXLError,
			GPOS_WSZ_LIT("No CTE")
			);
	}

	const CDXLNode *cte_producer_dxlnode = cte_list_entry->GetCTEProducer(rte->ctename);
	const List *plCTEProducerTargetList = cte_list_entry->GetCTEProducerTargetList(rte->ctename);
	
	GPOS_ASSERT(NULL != cte_producer_dxlnode && NULL != plCTEProducerTargetList);

	CDXLLogicalCTEProducer *pdxlopProducer = CDXLLogicalCTEProducer::Cast(cte_producer_dxlnode->GetOperator());
	ULONG cte_id = pdxlopProducer->Id();
	ULongPtrArray *op_colid_array_cte_producer = pdxlopProducer->GetOutputColIdsArray();
	
	// construct output column array
	ULongPtrArray *colid_array_cte_consumer = GenerateColIds(m_memory_pool, op_colid_array_cte_producer->Size());
			
	// load the new columns from the CTE
	m_var_to_colid_map->LoadCTEColumns(current_query_level, rti, colid_array_cte_consumer, const_cast<List *>(plCTEProducerTargetList));

	CDXLLogicalCTEConsumer *dxl_cte_consumer_op = GPOS_NEW(m_memory_pool) CDXLLogicalCTEConsumer(m_memory_pool, cte_id, colid_array_cte_consumer);
	CDXLNode *pdxlnCTE = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxl_cte_consumer_op);

	return pdxlnCTE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateDerivedTablesToDXL
//
//	@doc:
//		Translate a derived table into CDXLNode
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateDerivedTablesToDXL
	(
	const RangeTblEntry *rte,
	ULONG rti,
	ULONG current_query_level
	)
{
	Query *pqueryDerTbl = rte->subquery;
	GPOS_ASSERT(NULL != pqueryDerTbl);

	CMappingVarColId *var_col_id_mapping = m_var_to_colid_map->CopyMapColId(m_memory_pool);

	CTranslatorQueryToDXL trquerytodxl
		(
		m_memory_pool,
		m_md_accessor,
		m_colid_counter,
		m_cte_id_counter,
		var_col_id_mapping,
		pqueryDerTbl,
		m_query_level + 1,
		IsDMLQuery(),
		m_query_level_to_cte_map
		);

	// translate query representing the derived table to its DXL representation
	CDXLNode *pdxlnDerTbl = trquerytodxl.TranslateSelectQueryToDXL();

	// get the output columns of the derived table
	DXLNodeArray *pdrgpdxlnQueryOutputDerTbl = trquerytodxl.GetQueryOutputCols();
	DXLNodeArray *cte_dxlnode_array = trquerytodxl.GetCTEs();
	GPOS_ASSERT(NULL != pdxlnDerTbl && pdrgpdxlnQueryOutputDerTbl != NULL);

	CUtils::AddRefAppend(m_dxl_cte_producers, cte_dxlnode_array);
	
	m_has_distributed_tables = m_has_distributed_tables || trquerytodxl.HasDistributedTables();

	// make note of new columns from derived table
	m_var_to_colid_map->LoadDerivedTblColumns(current_query_level, rti, pdrgpdxlnQueryOutputDerTbl, trquerytodxl.Pquery()->targetList);

	return pdxlnDerTbl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateExprToDXL
//
//	@doc:
//		Translate the Expr into a CDXLScalar node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateExprToDXL
	(
	Expr *expr
	)
{
	CDXLNode *pdxlnScalar = m_scalar_translator->CreateScalarOpFromExpr(expr, m_var_to_colid_map, &m_has_distributed_tables);
	GPOS_ASSERT(NULL != pdxlnScalar);

	return pdxlnScalar;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateJoinExprInFromToDXL
//
//	@doc:
//		Translate the JoinExpr on a GPDB query into a CDXLLogicalJoin node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateJoinExprInFromToDXL
	(
	JoinExpr *pjoinexpr
	)
{
	GPOS_ASSERT(NULL != pjoinexpr);

	CDXLNode *left_child_dxlnode = TranslateFromClauseToDXL(pjoinexpr->larg);
	CDXLNode *right_child_dxlnode = TranslateFromClauseToDXL(pjoinexpr->rarg);
	EdxlJoinType join_type = CTranslatorUtils::ConvertToDXLJoinType(pjoinexpr->jointype);
	CDXLNode *pdxlnJoin = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalJoin(m_memory_pool, join_type));

	GPOS_ASSERT(NULL != left_child_dxlnode && NULL != right_child_dxlnode);

	pdxlnJoin->AddChild(left_child_dxlnode);
	pdxlnJoin->AddChild(right_child_dxlnode);

	Node* node = pjoinexpr->quals;

	// translate the join condition
	if (NULL != node)
	{
		pdxlnJoin->AddChild(TranslateExprToDXL( (Expr*) node));
	}
	else
	{
		// a cross join therefore add a CDXLScalarConstValue representing the value "true"
		pdxlnJoin->AddChild(CreateDXLConstValueTrue());
	}

	// extract the range table entry for the join expr to:
	// 1. Process the alias names of the columns
	// 2. Generate a project list for the join expr and maintain it in our hash map

	const ULONG ulRtIndex = pjoinexpr->rtindex;
	RangeTblEntry *rte = (RangeTblEntry *) gpdb::ListNth(m_query->rtable, ulRtIndex - 1);
	GPOS_ASSERT(NULL != rte);

	Alias *alias = rte->eref;
	GPOS_ASSERT(NULL != alias);
	GPOS_ASSERT(NULL != alias->colnames && 0 < gpdb::ListLength(alias->colnames));
	GPOS_ASSERT(gpdb::ListLength(rte->joinaliasvars) == gpdb::ListLength(alias->colnames));

	CDXLNode *pdxlnPrLComputedColumns = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for each entry in the joinaliasvars
	ListCell *plcNode = NULL;
	ListCell *plcColName = NULL;
	ForBoth (plcNode, rte->joinaliasvars,
			plcColName, alias->colnames)
	{
		Node *pnodeJoinAlias = (Node *) lfirst(plcNode);
		GPOS_ASSERT(IsA(pnodeJoinAlias, Var) || IsA(pnodeJoinAlias, CoalesceExpr));
		Value *value = (Value *) lfirst(plcColName);
		CHAR *col_name_char_array = strVal(value);

		// create the DXL node holding the target list entry and add it to proj list
		CDXLNode *proj_elem_dxlnode =  TranslateExprToDXLProject( (Expr*) pnodeJoinAlias, col_name_char_array);
		project_list_dxl->AddChild(proj_elem_dxlnode);

		if (IsA(pnodeJoinAlias, CoalesceExpr))
		{
			// add coalesce expression to the computed columns
			proj_elem_dxlnode->AddRef();
			pdxlnPrLComputedColumns->AddChild(proj_elem_dxlnode);
		}
	}
	m_var_to_colid_map->LoadProjectElements(m_query_level, ulRtIndex, project_list_dxl);
	project_list_dxl->Release();

	if (0 == pdxlnPrLComputedColumns->Arity())
	{
		pdxlnPrLComputedColumns->Release();
		return pdxlnJoin;
	}

	CDXLNode *project_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
	project_dxlnode->AddChild(pdxlnPrLComputedColumns);
	project_dxlnode->AddChild(pdxlnJoin);

	return project_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateTargetListToDXLProject
//
//	@doc:
//		Create a DXL project list from the target list. The function allocates
//		memory in the translator memory pool and caller responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateTargetListToDXLProject
	(
	List *target_list,
	CDXLNode *dxl_node_child,
	IntUlongHashMap *sort_grpref_to_colid_mapping,
	IntUlongHashMap *output_attno_to_colid_mapping,
	List *plgrpcl,
	BOOL fExpandAggrefExpr
	)
{
	BOOL fGroupBy = (0 != gpdb::ListLength(m_query->groupClause) || m_query->hasAggs);

	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for each entry in the target list
	ListCell *lc = NULL;
	
	// target entries that are result of flattening join alias
	// and are equivalent to a defined grouping column target entry
	List *plOmittedTE = NIL; 

	// list for all vars used in aggref expressions
	List *plVars = NULL;
	ULONG resno = 0;
	ForEach(lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));
		GPOS_ASSERT(0 < target_entry->resno);
		resno = target_entry->resno;

		BOOL is_grouping_col = CTranslatorUtils::IsGroupingColumn(target_entry, plgrpcl);
		if (!fGroupBy || (fGroupBy && is_grouping_col))
		{
			CDXLNode *proj_elem_dxlnode =  TranslateExprToDXLProject(target_entry->expr, target_entry->resname);
			ULONG col_id = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->Id();

			AddSortingGroupingColumn(target_entry, sort_grpref_to_colid_mapping, col_id);

			// add column to the list of output columns of the query
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);

			if (!IsA(target_entry->expr, Var))
			{
				// only add computed columns to the project list
				project_list_dxl->AddChild(proj_elem_dxlnode);
			}
			else
			{
				proj_elem_dxlnode->Release();
			}
		}
		else if (fExpandAggrefExpr && IsA(target_entry->expr, Aggref))
		{
			plVars = gpdb::ListConcat(plVars, gpdb::ExtractNodesExpression((Node *) target_entry->expr, T_Var, false /*descendIntoSubqueries*/));
		}
		else if (!IsA(target_entry->expr, Aggref))
		{
			plOmittedTE = gpdb::LAppend(plOmittedTE, target_entry);
		}
	}

	// process target entries that are a result of flattening join alias
	lc = NULL;
	ForEach(lc, plOmittedTE)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		INT iSortGroupRef = (INT) target_entry->ressortgroupref;

		TargetEntry *pteGroupingColumn = CTranslatorUtils::GetGroupingColumnTargetEntry( (Node*) target_entry->expr, plgrpcl, target_list);
		if (NULL != pteGroupingColumn)
		{
			const ULONG col_id = CTranslatorUtils::GetColId((INT) pteGroupingColumn->ressortgroupref, sort_grpref_to_colid_mapping);
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, target_entry->resno, col_id);
			if (0 < iSortGroupRef && 0 < col_id && NULL == sort_grpref_to_colid_mapping->Find(&iSortGroupRef))
			{
				AddSortingGroupingColumn(target_entry, sort_grpref_to_colid_mapping, col_id);
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
		resno++;
		Var *var = (Var *) lfirst(plcVar);

		// TODO: Dec 28, 2012; figure out column's name
		CDXLNode *proj_elem_dxlnode =  TranslateExprToDXLProject((Expr*) var, "?col?");
		
		ULONG col_id = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator())->Id();

		// add column to the list of output columns of the query
		StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
		
		proj_elem_dxlnode->Release();
	}
	
	if (0 < project_list_dxl->Arity())
	{
		// create a node with the CDXLLogicalProject operator and add as its children:
		// the CDXLProjectList node and the node representing the input to the project node
		CDXLNode *project_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool));
		project_dxlnode->AddChild(project_list_dxl);
		project_dxlnode->AddChild(dxl_node_child);
		GPOS_ASSERT(NULL != project_dxlnode);
		return project_dxlnode;
	}

	project_list_dxl->Release();
	return dxl_node_child;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateDXLProjectNullsForGroupingSets
//
//	@doc:
//		Construct a DXL project node projecting NULL values for the columns in the
//		given bitset
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::CreateDXLProjectNullsForGroupingSets
	(
	List *target_list,
	CDXLNode *dxl_node_child,
	CBitSet *bitset,					// group by columns
	IntUlongHashMap *phmiulSortgrouprefCols,	// mapping of sorting and grouping columns
	IntUlongHashMap *output_attno_to_colid_mapping,		// mapping of output columns
	UlongUlongHashMap *grpcol_index_to_colid_mapping		// mapping of unique grouping col positions
	)
	const
{
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for those non-aggregate entries in the target list which
	// are not included in the grouping set
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		BOOL is_grouping_col = bitset->Get(target_entry->ressortgroupref);
		ULONG resno = target_entry->resno;

		ULONG col_id = 0;
		
		if (IsA(target_entry->expr, GroupingFunc))
		{
			col_id = m_colid_counter->next_id();
			CDXLNode *pdxlnGroupingFunc = TranslateGroupingFuncToDXL(target_entry->expr, bitset, grpcol_index_to_colid_mapping);
			CMDName *mdname_alias = NULL;

			if (NULL == target_entry->resname)
			{
				CWStringConst str_unnamed_col(GPOS_WSZ_LIT("grouping"));
				mdname_alias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &str_unnamed_col);
			}
			else
			{
				CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
				mdname_alias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
				GPOS_DELETE(alias_str);
			}
			CDXLNode *proj_elem_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, col_id, mdname_alias), pdxlnGroupingFunc);
			project_list_dxl->AddChild(proj_elem_dxlnode);
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
		}
		else if (!is_grouping_col && !IsA(target_entry->expr, Aggref))
		{
			OID oid_type = gpdb::ExprType((Node *) target_entry->expr);

			col_id = m_colid_counter->next_id();

			CMDIdGPDB *mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(oid_type);
			CDXLNode *proj_elem_dxlnode = CTranslatorUtils::CreateDXLProjElemConstNULL(m_memory_pool, m_md_accessor, mdid, col_id, target_entry->resname);
			mdid->Release();
			
			project_list_dxl->AddChild(proj_elem_dxlnode);
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
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
		return dxl_node_child;
	}

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool), project_list_dxl, dxl_node_child);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateDXLProjectGroupingFuncs
//
//	@doc:
//		Construct a DXL project node projecting values for the grouping funcs in
//		the target list 
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::CreateDXLProjectGroupingFuncs
	(
	List *target_list,
	CDXLNode *dxl_node_child,
	CBitSet *bitset,
	IntUlongHashMap *output_attno_to_colid_mapping,
	UlongUlongHashMap *grpcol_index_to_colid_mapping,
	IntUlongHashMap *sort_grpref_to_colid_mapping
	)
	const
{
	CDXLNode *project_list_dxl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjList(m_memory_pool));

	// construct a proj element node for those non-aggregate entries in the target list which
	// are not included in the grouping set
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		ULONG resno = target_entry->resno;

		if (IsA(target_entry->expr, GroupingFunc))
		{
			ULONG col_id = m_colid_counter->next_id();
			CDXLNode *pdxlnGroupingFunc = TranslateGroupingFuncToDXL(target_entry->expr, bitset, grpcol_index_to_colid_mapping);
			CMDName *mdname_alias = NULL;

			if (NULL == target_entry->resname)
			{
				CWStringConst str_unnamed_col(GPOS_WSZ_LIT("grouping"));
				mdname_alias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &str_unnamed_col);
			}
			else
			{
				CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
				mdname_alias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
				GPOS_DELETE(alias_str);
			}
			CDXLNode *proj_elem_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, col_id, mdname_alias), pdxlnGroupingFunc);
			project_list_dxl->AddChild(proj_elem_dxlnode);
			StoreAttnoColIdMapping(output_attno_to_colid_mapping, resno, col_id);
			AddSortingGroupingColumn(target_entry, sort_grpref_to_colid_mapping, col_id);
		}
	}

	if (0 == project_list_dxl->Arity())
	{
		// no project necessary
		project_list_dxl->Release();
		return dxl_node_child;
	}

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalProject(m_memory_pool), project_list_dxl, dxl_node_child);
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
	IntUlongHashMap *attno_to_colid_mapping,
	INT attno,
	ULONG col_id
	)
	const
{
	GPOS_ASSERT(NULL != attno_to_colid_mapping);

	INT *key = GPOS_NEW(m_memory_pool) INT(attno);
	ULONG *value = GPOS_NEW(m_memory_pool) ULONG(col_id);
	BOOL result = attno_to_colid_mapping->Insert(key, value);

	if (!result)
	{
		GPOS_DELETE(key);
		GPOS_DELETE(value);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateDXLOutputCols
//
//	@doc:
//		Construct an array of DXL nodes representing the query output
//
//---------------------------------------------------------------------------
DXLNodeArray *
CTranslatorQueryToDXL::CreateDXLOutputCols
	(
	List *target_list,
	IntUlongHashMap *attno_to_colid_mapping
	)
	const
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != attno_to_colid_mapping);

	DXLNodeArray *dxl_nodes = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);

	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(0 < target_entry->resno);
		ULONG resno = target_entry->resno;

		if (target_entry->resjunk)
		{
			continue;
		}

		GPOS_ASSERT(NULL != target_entry);
		CMDName *mdname = NULL;
		if (NULL == target_entry->resname)
		{
			CWStringConst str_unnamed_col(GPOS_WSZ_LIT("?column?"));
			mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &str_unnamed_col);
		}
		else
		{
			CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
			mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
			// CName constructor copies string
			GPOS_DELETE(alias_str);
		}

		const ULONG col_id = CTranslatorUtils::GetColId(resno, attno_to_colid_mapping);

		// create a column reference
		IMDId *mdid_type = GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::ExprType( (Node*) target_entry->expr));
		INT type_modifier = gpdb::ExprTypeMod((Node*) target_entry->expr);
		CDXLColRef *dxl_colref = GPOS_NEW(m_memory_pool) CDXLColRef(m_memory_pool, mdname, col_id, mdid_type, type_modifier);
		CDXLScalarIdent *dxl_ident = GPOS_NEW(m_memory_pool) CDXLScalarIdent
												(
												m_memory_pool,
												dxl_colref
												);

		// create the DXL node holding the scalar ident operator
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxl_ident);

		dxl_nodes->Append(dxlnode);
	}

	return dxl_nodes;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateExprToDXLProject
//
//	@doc:
//		Create a DXL project element node from the target list entry or var.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateExprToDXLProject
	(
	Expr *expr,
	const CHAR *alias_name,
	BOOL insist_new_colids
	)
{
	GPOS_ASSERT(NULL != expr);

	// construct a scalar operator
	CDXLNode *dxl_node_child = TranslateExprToDXL(expr);

	// get the id and alias for the proj elem
	ULONG ulPrElId;
	CMDName *mdname_alias = NULL;

	if (NULL == alias_name)
	{
		CWStringConst str_unnamed_col(GPOS_WSZ_LIT("?column?"));
		mdname_alias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &str_unnamed_col);
	}
	else
	{
		CWStringDynamic *alias_str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, alias_name);
		mdname_alias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, alias_str);
		GPOS_DELETE(alias_str);
	}

	if (IsA(expr, Var) && !insist_new_colids)
	{
		// project elem is a a reference to a column - use the colref id
		GPOS_ASSERT(EdxlopScalarIdent == dxl_node_child->GetOperator()->GetDXLOperator());
		CDXLScalarIdent *dxl_ident = (CDXLScalarIdent *) dxl_node_child->GetOperator();
		ulPrElId = dxl_ident->MakeDXLColRef()->Id();
	}
	else
	{
		// project elem is a defined column - get a new id
		ulPrElId = m_colid_counter->next_id();
	}

	CDXLNode *proj_elem_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, ulPrElId, mdname_alias));
	proj_elem_dxlnode->AddChild(dxl_node_child);

	return proj_elem_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::CreateDXLConstValueTrue
//
//	@doc:
//		Returns a CDXLNode representing scalar condition "true"
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::CreateDXLConstValueTrue()
{
	Const *const_expr = (Const*) gpdb::MakeBoolConst(true /*value*/, false /*isnull*/);
	CDXLNode *dxlnode = TranslateExprToDXL((Expr*) const_expr);
	gpdb::GPDBFree(const_expr);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::TranslateGroupingFuncToDXL
//
//	@doc:
//		Translate grouping func
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorQueryToDXL::TranslateGroupingFuncToDXL
	(
	const Expr *expr,
	CBitSet *bitset,
	UlongUlongHashMap *grpcol_index_to_colid_mapping
	)
	const
{
	GPOS_ASSERT(IsA(expr, GroupingFunc));
	GPOS_ASSERT(NULL != grpcol_index_to_colid_mapping);

	const GroupingFunc *pgroupingfunc = (GroupingFunc *) expr;

	if (1 < gpdb::ListLength(pgroupingfunc->args))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Grouping function with multiple arguments"));
	}
	
	Node *node = (Node *) gpdb::ListNth(pgroupingfunc->args, 0);
	ULONG ulGroupingIndex = gpdb::GetIntFromValue(node);
		
	// generate a constant value for the result of the grouping function as follows:
	// if the grouping function argument is a group-by column, result is 0
	// otherwise, the result is 1 
	LINT lValue = 0;
	
	ULONG *pulSortGrpRef = grpcol_index_to_colid_mapping->Find(&ulGroupingIndex);
	GPOS_ASSERT(NULL != pulSortGrpRef);
	BOOL is_grouping_col = bitset->Get(*pulSortGrpRef);
	if (!is_grouping_col)
	{
		// not a grouping column
		lValue = 1; 
	}

	const IMDType *md_type = m_md_accessor->PtMDType<IMDTypeInt8>(m_sysid);
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(*pmdidMDC);
	
	CDXLDatum *datum_dxl = GPOS_NEW(m_memory_pool) CDXLDatumInt8(m_memory_pool, mdid, false /* is_null */, lValue);
	CDXLScalarConstValue *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarConstValue(m_memory_pool, datum_dxl);
	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);
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
	List *cte_list,
	ULONG ulCteQueryLevel
	)
{
	GPOS_ASSERT(NULL != m_dxl_cte_producers && "CTE Producer list not initialized"); 
	
	if (NULL == cte_list)
	{
		return;
	}
	
	ListCell *lc = NULL;
	
	ForEach (lc, cte_list)
	{
		CommonTableExpr *pcte = (CommonTableExpr *) lfirst(lc);
		GPOS_ASSERT(IsA(pcte->ctequery, Query));
		
		if (pcte->cterecursive)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("WITH RECURSIVE"));
		}

		Query *pqueryCte = CQueryMutators::NormalizeQuery(m_memory_pool, m_md_accessor, (Query *) pcte->ctequery, ulCteQueryLevel + 1);
		
		// the query representing the cte can only access variables defined in the current level as well as
		// those defined at prior query levels
		CMappingVarColId *var_col_id_mapping = m_var_to_colid_map->CopyMapColId(ulCteQueryLevel);

		CTranslatorQueryToDXL trquerytodxl
			(
			m_memory_pool,
			m_md_accessor,
			m_colid_counter,
			m_cte_id_counter,
			var_col_id_mapping,
			pqueryCte,
			ulCteQueryLevel + 1,
			IsDMLQuery(),
			m_query_level_to_cte_map
			);

		// translate query representing the cte table to its DXL representation
		CDXLNode *pdxlnCteChild = trquerytodxl.TranslateSelectQueryToDXL();

		// get the output columns of the cte table
		DXLNodeArray *pdrgpdxlnQueryOutputCte = trquerytodxl.GetQueryOutputCols();
		DXLNodeArray *cte_dxlnode_array = trquerytodxl.GetCTEs();
		m_has_distributed_tables = m_has_distributed_tables || trquerytodxl.HasDistributedTables();

		GPOS_ASSERT(NULL != pdxlnCteChild && NULL != pdrgpdxlnQueryOutputCte && NULL != cte_dxlnode_array);
		
		// append any nested CTE
		CUtils::AddRefAppend(m_dxl_cte_producers, cte_dxlnode_array);
		
		ULongPtrArray *pdrgpulColIds = GPOS_NEW(m_memory_pool) ULongPtrArray(m_memory_pool);
		
		const ULONG output_columns = pdrgpdxlnQueryOutputCte->Size();
		for (ULONG ul = 0; ul < output_columns; ul++)
		{
			CDXLNode *pdxlnOutputCol = (*pdrgpdxlnQueryOutputCte)[ul];
			CDXLScalarIdent *dxl_sc_ident = CDXLScalarIdent::Cast(pdxlnOutputCol->GetOperator());
			pdrgpulColIds->Append(GPOS_NEW(m_memory_pool) ULONG(dxl_sc_ident->MakeDXLColRef()->Id()));
		}
		
		CDXLLogicalCTEProducer *dxlop = GPOS_NEW(m_memory_pool) CDXLLogicalCTEProducer(m_memory_pool, m_cte_id_counter->next_id(), pdrgpulColIds);
		CDXLNode *cte_producer_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop, pdxlnCteChild);
		
		m_dxl_cte_producers->Append(cte_producer_dxlnode);
#ifdef GPOS_DEBUG
		BOOL result =
#endif
		m_cteid_at_current_query_level_map->Insert(GPOS_NEW(m_memory_pool) ULONG(dxlop->Id()), GPOS_NEW(m_memory_pool) BOOL(true));
		GPOS_ASSERT(result);
		
		// update CTE producer mappings
		CCTEListEntry *cte_list_entry = m_query_level_to_cte_map->Find(&ulCteQueryLevel);
		if (NULL == cte_list_entry)
		{
			cte_list_entry = GPOS_NEW(m_memory_pool) CCTEListEntry (m_memory_pool, ulCteQueryLevel, pcte, cte_producer_dxlnode);
#ifdef GPOS_DEBUG
		BOOL is_res =
#endif
			m_query_level_to_cte_map->Insert(GPOS_NEW(m_memory_pool) ULONG(ulCteQueryLevel), cte_list_entry);
			GPOS_ASSERT(is_res);
		}
		else
		{
			cte_list_entry->AddCTEProducer(m_memory_pool, pcte, cte_producer_dxlnode);
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
	DXLNodeArray *dxl_nodes,
	CDXLNode **dxl_cte_anchor_top,
	CDXLNode **dxl_cte_anchor_bottom
	)
{
	GPOS_ASSERT(NULL == *dxl_cte_anchor_top);
	GPOS_ASSERT(NULL == *dxl_cte_anchor_bottom);

	if (NULL == dxl_nodes || 0 == dxl_nodes->Size())
	{
		return;
	}
	
	const ULONG ulCTEs = dxl_nodes->Size();
	
	for (ULONG ul = ulCTEs; ul > 0; ul--)
	{
		// construct a new CTE anchor on top of the previous one
		CDXLNode *cte_producer_dxlnode = (*dxl_nodes)[ul-1];
		CDXLLogicalCTEProducer *dxl_cte_prod_op = CDXLLogicalCTEProducer::Cast(cte_producer_dxlnode->GetOperator());
		ULONG ulCTEProducerId = dxl_cte_prod_op->Id();
		
		if (NULL == m_cteid_at_current_query_level_map->Find(&ulCTEProducerId))
		{
			// cte not defined at this level: CTE anchor was already added
			continue;
		}
		
		CDXLNode *pdxlnCTEAnchorNew = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLLogicalCTEAnchor(m_memory_pool, ulCTEProducerId));
		
		if (NULL == *dxl_cte_anchor_bottom)
		{
			*dxl_cte_anchor_bottom = pdxlnCTEAnchorNew;
		}

		if (NULL != *dxl_cte_anchor_top)
		{
			pdxlnCTEAnchorNew->AddChild(*dxl_cte_anchor_top);
		}
		*dxl_cte_anchor_top = pdxlnCTEAnchorNew;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::GenerateColIds
//
//	@doc:
//		Generate an array of new column ids of the given size
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorQueryToDXL::GenerateColIds
	(
	IMemoryPool *memory_pool,
	ULONG size
	)
	const
{
	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	
	for (ULONG ul = 0; ul < size; ul++)
	{
		pdrgpul->Append(GPOS_NEW(memory_pool) ULONG(m_colid_counter->next_id()));
	}
	
	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::ExtractColIds
//
//	@doc:
//		Extract column ids from the given mapping
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorQueryToDXL::ExtractColIds
	(
	IMemoryPool *memory_pool,
	IntUlongHashMap *attno_to_colid_mapping
	)
	const
{
	UlongUlongHashMap *old_new_col_mapping = GPOS_NEW(memory_pool) UlongUlongHashMap(memory_pool);
	
	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	
	IntUlongHashmapIter mi(attno_to_colid_mapping);
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
//		CTranslatorQueryToDXL::RemapColIds
//
//	@doc:
//		Construct a new hashmap which replaces the values in the From array
//		with the corresponding value in the To array
//
//---------------------------------------------------------------------------
IntUlongHashMap *
CTranslatorQueryToDXL::RemapColIds
	(
	IMemoryPool *memory_pool,
	IntUlongHashMap *attno_to_colid_mapping,
	ULongPtrArray *from_list_colids,
	ULongPtrArray *to_list_colids
	)
	const
{
	GPOS_ASSERT(NULL != attno_to_colid_mapping);
	GPOS_ASSERT(NULL != from_list_colids && NULL != to_list_colids);
	GPOS_ASSERT(from_list_colids->Size() == to_list_colids->Size());
	
	// compute a map of the positions in the from array
	UlongUlongHashMap *old_new_col_mapping = GPOS_NEW(memory_pool) UlongUlongHashMap(memory_pool);
	const ULONG size = from_list_colids->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
#ifdef GPOS_DEBUG
		BOOL result = 
#endif // GPOS_DEBUG
		old_new_col_mapping->Insert(GPOS_NEW(memory_pool) ULONG(*((*from_list_colids)[ul])), GPOS_NEW(memory_pool) ULONG(*((*to_list_colids)[ul])));
		GPOS_ASSERT(result);
	}

	IntUlongHashMap *phmiulResult = GPOS_NEW(memory_pool) IntUlongHashMap(memory_pool);
	IntUlongHashmapIter mi(attno_to_colid_mapping);
	while (mi.Advance())
	{
		INT *key = GPOS_NEW(memory_pool) INT(*(mi.Key()));
		const ULONG *value = mi.Value();
		GPOS_ASSERT(NULL != value);
		
		ULONG *pulValueRemapped = GPOS_NEW(memory_pool) ULONG(*(old_new_col_mapping->Find(value)));
		phmiulResult->Insert(key, pulValueRemapped);
	}
		
	old_new_col_mapping->Release();
	
	return phmiulResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorQueryToDXL::RemapColIds
//
//	@doc:
//		True iff this query or one of its ancestors is a DML query
//
//---------------------------------------------------------------------------
BOOL
CTranslatorQueryToDXL::IsDMLQuery()
{
	return (m_is_top_query_dml || m_query->resultRelation != 0);
}

// EOF
