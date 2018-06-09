//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CTranslatorUtils.h
//
//	@doc:
//		Class providing utility methods for translating GPDB's PlannedStmt/Query
//		into DXL Tree
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorUtils_H
#define GPDXL_CTranslatorUtils_H
#define GPDXL_SYSTEM_COLUMNS 8

#include "gpopt/translate/CTranslatorScalarToDXL.h"

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"

#include "naucrates/dxl/operators/dxlops.h"
#include "naucrates/dxl/CIdGenerator.h"

#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/IMDType.h"

#include "naucrates/statistics/IStatistics.h"

#include "nodes/parsenodes.h"
#include "access/sdir.h"
#include "access/skey.h"

// fwd declarations
namespace gpopt
{
	class CMDAccessor;

	// dynamic array of bitsets
	typedef CDynamicPtrArray<CBitSet, CleanupRelease> CBitSetArray;
}

namespace gpdxl
{
	class CDXLTranslateContext;
}

namespace gpdxl
{
	using namespace gpopt;

	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorUtils
	//
	//	@doc:
	//		Class providing methods for translating GPDB's PlannedStmt/Query
	//		into DXL Tree
	//
	//---------------------------------------------------------------------------
	class CTranslatorUtils
	{
		private:

			// construct a set of column attnos corresponding to a single grouping set
			static
			CBitSet *CreateAttnoSetForGroupingSet(IMemoryPool *mp, List *group_elems, ULONG num_cols, UlongToUlongMap *group_col_pos, CBitSet *group_cols);

			// create a set of grouping sets for a rollup
			static
			CBitSetArray *CreateGroupingSetsForRollup(IMemoryPool *mp, GroupingClause *grouping_clause, ULONG num_cols, UlongToUlongMap *grouping_col_to_pos_map, CBitSet *group_cols);

			// check if the given mdid array contains any of the polymorphic
			// types (ANYELEMENT, ANYARRAY)
			static
			BOOL ContainsPolymorphicTypes(IMdIdArray *mdid_array);

			// resolve polymorphic types in the given array of type ids, replacing
			// them with the actual types obtained from the query
			static
			IMdIdArray *ResolvePolymorphicTypes
						(
						IMemoryPool *mp,
						IMdIdArray *mdid_array,
						List *arg_types,
						FuncExpr *func_expr
						);
			
			// update grouping col position mappings
			static
			void UpdateGrpColMapping(IMemoryPool *mp, UlongToUlongMap *grouping_col_to_pos_map, CBitSet *group_cols, ULONG sort_group_ref);

		public:

			struct SCmptypeStrategy
			{
				IMDType::ECmpType comptype;
				StrategyNumber strategy_no;

			};

			// get the GPDB scan direction from its corresponding DXL representation
			static
			ScanDirection GetScanDirection(EdxlIndexScanDirection idx_scan_direction);

			// get the oid of comparison operator
			static
			OID OidCmpOperator(Expr* expr);

			// get the opfamily for index key
			static
			OID GetOpFamilyForIndexQual(INT attno, OID oid_index);
			
			// return the type for the system column with the given number
			static
			CMDIdGPDB *GetSystemColType(IMemoryPool *mp, AttrNumber attno);

			// find the n-th column descriptor in the table descriptor
			static
			const CDXLColDescr *GetColumnDescrAt(const CDXLTableDescr *table_descr, ULONG pos);

			// return the name for the system column with given number
			static
			const CWStringConst *GetSystemColName(AttrNumber attno);

			// returns the length for the system column with given attno number
			static
			const ULONG GetSystemColLength(AttrNumber attno);

			// translate the join type from its GPDB representation into the DXL one
			static
			EdxlJoinType ConvertToDXLJoinType(JoinType jt);

			// translate the index scan direction from its GPDB representation into the DXL one
			static
			EdxlIndexScanDirection ConvertToDXLIndexScanDirection(ScanDirection sd);

			// create a DXL index descriptor from an index MD id
			static
			CDXLIndexDescr *GetIndexDescr(IMemoryPool *mp, CMDAccessor *md_accessor, IMDId *mdid);

			// translate a RangeTableEntry into a CDXLTableDescr
			static
			CDXLTableDescr *GetTableDescr
								(
								IMemoryPool *mp,
								CMDAccessor *md_accessor,
								CIdGenerator *id_generator,
								const RangeTblEntry *rte,
								BOOL *is_distributed_table = NULL
								);

			// translate a RangeTableEntry into a CDXLLogicalTVF
			static
			CDXLLogicalTVF *ConvertToCDXLLogicalTVF
								(
								IMemoryPool *mp,
								CMDAccessor *md_accessor,
								CIdGenerator *id_generator,
								const RangeTblEntry *rte
								);

			// get column descriptors from a record type
			static
			CDXLColDescrArray *GetColumnDescriptorsFromRecord
						(
						IMemoryPool *mp,
						CIdGenerator *id_generator,
						List *col_names,
						List *col_types,
						List *col_type_modifiers
						);

			// get column descriptors from a record type
			static
			CDXLColDescrArray *GetColumnDescriptorsFromRecord
						(
						IMemoryPool *mp,
						CIdGenerator *id_generator,
						List *col_names,
						IMdIdArray *out_arg_types
						);

			// get column descriptor from a base type
			static
			CDXLColDescrArray *GetColumnDescriptorsFromBase
						(
						IMemoryPool *mp,
						CIdGenerator *id_generator,
						IMDId *mdid_return_type,
						INT type_modifier,
						CMDName *md_name
						);

			// get column descriptors from a composite type
			static
			CDXLColDescrArray *GetColumnDescriptorsFromComposite
						(
						IMemoryPool *mp,
						CMDAccessor *md_accessor,
						CIdGenerator *id_generator,
						const IMDType *md_type
						);

			// expand a composite type into an array of IMDColumns
			static
			CMDColumnArray *ExpandCompositeType
						(
						IMemoryPool *mp,
						CMDAccessor *md_accessor,
						const IMDType *md_type
						);

			// return the dxl representation of the set operation
			static
			EdxlSetOpType GetSetOpType(SetOperation setop, BOOL is_all);

			// return the GPDB frame exclusion strategy from its corresponding DXL representation
			static
			WindowExclusion Windowexclusion(EdxlFrameExclusionStrategy edxlfes);

			// return the GPDB frame boundary kind from its corresponding DXL representation
			static
			WindowBoundingKind Windowboundkind(EdxlFrameBoundary edxlfb);

			// construct a dynamic array of sets of column attnos corresponding
			// to the group by clause
			static
			CBitSetArray *GetColumnAttnosForGroupBy(IMemoryPool *mp, List *group_clause, ULONG num_cols, UlongToUlongMap *group_col_pos, CBitSet *group_cold);

			// return a copy of the query with constant of unknown type being coerced
			// to the common data type of the output target list
			static
			Query *FixUnknownTypeConstant(Query *query, List *target_list);

			// return the type of the nth non-resjunked target list entry
			static OID GetTargetListReturnTypeOid(List *target_list, ULONG col_pos);

			// construct an array of DXL column identifiers for a target list
			static
			ULongPtrArray *GenerateColIds
					(
					IMemoryPool *mp,
					List *target_list,
					IMdIdArray *input_mdids,
					ULongPtrArray *input_nums,
					BOOL *is_outer_ref,
					CIdGenerator *colid_generator
					);

			// construct an array of DXL column descriptors for a target list
			// using the column ids in the given array
			static
			CDXLColDescrArray *GetDXLColumnDescrArray(IMemoryPool *mp, List *target_list, ULongPtrArray *colids, BOOL keep_res_junked);

			// return the positions of the target list entries included in the output
			static
			ULongPtrArray *GetPosInTargetList(IMemoryPool *mp, List *target_list, BOOL keep_res_junked);

			// construct a column descriptor from the given target entry, column identifier and position in the output
			static
			CDXLColDescr *GetColumnDescrAt(IMemoryPool *mp, TargetEntry *target_entry, ULONG colid, ULONG pos);

			// create a dummy project element to rename the input column identifier
			static
			CDXLNode *CreateDummyProjectElem(IMemoryPool *mp, ULONG colid_input, ULONG colid_output, CDXLColDescr *dxl_col_descr);

			// construct a list of colids corresponding to the given target list
			// using the given attno->colid map
			static
			ULongPtrArray *GetOutputColIdsArray(IMemoryPool *mp, List *target_list, IntToUlongMap *attno_to_colid_map);

			// construct an array of column ids for the given group by set
			static
			ULongPtrArray *GetGroupingColidArray(IMemoryPool *mp, CBitSet *group_by_cols, IntToUlongMap *sort_group_cols_to_colid_map);

			// return the Colid of column with given index
			static
			ULONG GetColId(INT index, IntToUlongMap *index_to_colid_map);

			// return the corresponding ColId for the given varno, varattno and querylevel
			static
			ULONG GetColId(ULONG query_level, INT varno, INT var_attno, IMDId *mdid, CMappingVarColId *var_colid_mapping);

			// check to see if the target list entry is a sorting column
			static
			BOOL IsSortingColumn(const TargetEntry *target_entry, List *sort_clause_list);
			// check to see if the target list entry is used in the window reference
			static
			BOOL IsWindowSpec(const TargetEntry *target_entry, List *window_clause_list);

			// extract a matching target entry that is a window spec
			static
			TargetEntry *GetWindowSpecTargetEntry(Node *node, List *window_clause_list, List *target_list);

			// check if the expression has a matching target entry that is a window spec
			static
			BOOL IsWindowSpec(Node *node, List *window_clause_list, List *target_list);

			// create a scalar const value expression for the given int8 value
			static
			CDXLNode *CreateDXLProjElemFromInt8Const(IMemoryPool *mp, CMDAccessor *md_accessor, INT val);

			// check to see if the target list entry is a grouping column
			static
			BOOL IsGroupingColumn(const TargetEntry *target_entry, List *group_clause_list);

			// check to see if the target list entry is a grouping column
			static
			BOOL IsGroupingColumn(const TargetEntry *target_entry, const GroupClause *sort_group_clause);

			// check to see if the sorting column entry is a grouping column
			static
			BOOL IsGroupingColumn(const SortClause *sort_group_clause, List *group_clause_list);

			// check if the expression has a matching target entry that is a grouping column
			static
			BOOL IsGroupingColumn(Node *node, List *group_clause_list, List *target_list);

			// extract a matching target entry that is a grouping column
			static
			TargetEntry *GetGroupingColumnTargetEntry(Node *node, List *group_clause_list, List *target_list);

			// convert a list of column ids to a list of attribute numbers using
			// the provided context with mappings
			static
			List *ConvertColidToAttnos(ULongPtrArray *pdrgpul, CDXLTranslateContext *dxl_translate_ctxt);
			
			// parse string value into a Long Integer
			static
			LINT GetLongFromStr(const CWStringBase *wcstr);

			// parse string value into an Integer
			static
			INT GetIntFromStr(const CWStringBase *wcstr);

			// check whether the given project list has a project element of the given
			// operator type
			static
			BOOL HasProjElem(CDXLNode *project_list_dxlnode, Edxlopid dxl_op_id);

			// create a multi-byte character string from a wide character string
			static
			CHAR *CreateMultiByteCharStringFromWCString(const WCHAR *wcstr);
			
			static 
			UlongToUlongMap *MakeNewToOldColMapping(IMemoryPool *mp, ULongPtrArray *old_colids, ULongPtrArray *new_colids);

			// check if the given tree contains a subquery
			static
			BOOL HasSubquery(Node *node);

			// check if the given function is a SIRV (single row volatile) that reads
			// or modifies SQL data
			static
			BOOL IsSirvFunc(IMemoryPool *mp, CMDAccessor *md_accessor, OID func_oid);
			
			// is this a motion sensitive to duplicates
			static
			BOOL IsDuplicateSensitiveMotion(CDXLPhysicalMotion *dxl_motion);

			// construct a project element with a const NULL expression
			static
			CDXLNode *CreateDXLProjElemConstNULL(IMemoryPool *mp, CMDAccessor *md_accessor, IMDId *mdid, ULONG colid, const WCHAR *col_name);

			// construct a project element with a const NULL expression
			static
			CDXLNode *CreateDXLProjElemConstNULL(IMemoryPool *mp, CMDAccessor *md_accessor, IMDId *mdid, ULONG colid, CHAR *alias_name);

			// create a DXL project element node with a Const NULL of type provided
			// by the column descriptor
			static
			CDXLNode *CreateDXLProjElemConstNULL(IMemoryPool *mp, CMDAccessor *md_accessor, CIdGenerator *colid_generator, const IMDColumn *col);

			// check required permissions for the range table
			static 
			void CheckRTEPermissions(List *range_table_list);

			// check if an aggregate window function has either prelim or inverse prelim func
			static
			void CheckAggregateWindowFn(Node *node);

			// check if given column ids are outer references in the tree rooted by given node
                        static
			void MarkOuterRefs(ULONG *colid, BOOL *is_outer_ref, ULONG num_columns, CDXLNode *node);

			// map DXL Subplan type to GPDB SubLinkType
			static
			SubLinkType MapDXLSubplanToSublinkType(EdxlSubPlanType dxl_subplan_type);

			// map GPDB SubLinkType to DXL Subplan type
			static
			EdxlSubPlanType MapSublinkTypeToDXLSubplan(SubLinkType slink);

			// check whether there are triggers for the given operation on
			// the given relation
			static
			BOOL RelHasTriggers(IMemoryPool *mp, CMDAccessor *md_accessor, const IMDRelation *mdrel, const EdxlDmlType dml_type_dxl);

			// check whether the given trigger is applicable to the given DML operation
			static
			BOOL IsApplicableTrigger(CMDAccessor *md_accessor, IMDId *trigger_mdid, const EdxlDmlType dml_type_dxl);
						
			// check whether there are NOT NULL or CHECK constraints for the given relation
			static
			BOOL RelHasConstraints(const IMDRelation *rel);

			// translate the list of error messages from an assert constraint list
			static 
			List *GetAssertErrorMsgs(CDXLNode *assert_constraint_list);

			// return the count of non-system columns in the relation
			static
			ULONG GetNumNonSystemColumns(const IMDRelation *mdrel);

			// check if we need to create stats buckets in DXL for the column attribute
			static
			BOOL ShouldCreateStatsBucket(OID att_type_oid);
	};
}

#endif // !GPDXL_CTranslatorUtils_H

// EOF
