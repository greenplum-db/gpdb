//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CPartPruneStepsBuilder.h
//
//	@doc:
//		Utility class to construct PartPruneInfos with appropriate
// 		PartPruningSteps from partitioning filter expressions
//---------------------------------------------------------------------------

#ifndef GPDXL_CPartPruneStepsBuilder_H
#define GPDXL_CPartPruneStepsBuilder_H

#include "gpos/base.h"

#include "gpopt/translate/CMappingColIdVarPlStmt.h"
#include "gpopt/translate/CTranslatorDXLToScalar.h"
#include "naucrates/dxl/operators/CDXLNode.h"

using namespace gpos;

namespace gpdxl
{
class CPartPruneStepsBuilder
{
private:
	// root partitioned tabled
	Relation m_relation;

	// index in the rtable
	Index m_rtindex;

	// list of pruned scan nodes denoted as an index of the relation's partition_mdids
	ULongPtrArray *m_part_indexes;

	// colid -> var mapping from the subtree
	CMappingColIdVarPlStmt *m_colid_var_mapping;

	// dxl -> scalar translator
	CTranslatorDXLToScalar *m_translator_dxl_to_scalar;

	// ctor
	CPartPruneStepsBuilder(Relation relation, Index rtindex,
						   ULongPtrArray *part_indexes,
						   CMappingColIdVarPlStmt *colid_var_mapping,
						   CTranslatorDXLToScalar *translator_dxl_to_scalar);

	CPartPruneStepsBuilder(const CPartPruneStepsBuilder &) = default;

public:
	// dtor
	~CPartPruneStepsBuilder() = default;

	static List *CreatePartPruneInfos(
		CDXLNode *filterNode, Relation relation, Index rtindex,
		ULongPtrArray *part_indexes, CMappingColIdVarPlStmt *colid_var_mapping,
		CTranslatorDXLToScalar *translator_dxl_to_scalar);

	PartitionedRelPruneInfo *CreatePartPruneInfoForOneLevel(
		CDXLNode *filterNode);

	List *PartPruneStepsFromFilter(CDXLNode *filterNode, INT *step_id,
								   List *steps_list);

	List *PartPruneStepFromScalarCmp(CDXLNode *node, INT *step_id,
									 List *steps_list);

	List *PartPruneStepFromScalarBoolExpr(CDXLNode *node, INT *step_id,
										  List *steps_list);
};
}  // namespace gpdxl

#endif	// !GPDXL_CPartPruneStepsBuilder_H

//EOF
