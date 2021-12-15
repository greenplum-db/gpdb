//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarAggref.h
//
//	@doc:
//		Class for representing DXL AggRef
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarAggref_H
#define GPDXL_CDXLScalarAggref_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

enum Edxlascalarggref
{
	EdxlscalaraggrefIndexArgs = 0,
	EdxlscalaraggrefIndexDirectArgs,
	EdxlscalaraggrefIndexAggOrder,
	EdxlscalaraggrefIndexAggDistinct,
};

enum EdxlAggrefStage
{
	EdxlaggstageNormal = 0,
	EdxlaggstagePartial,  // First (lower, earlier) stage of 2-stage aggregation.
	EdxlaggstageIntermediate,  // Between AGGSTAGE_PARTIAL and AGGSTAGE_FINAL that handles the higher aggregation
	// level in a (partial) ROLLUP grouping extension query
	EdxlaggstageFinal,	// Second (upper, later) stage of 2-stage aggregation.
	EdxlaggstageSentinel
};

enum EdxlAggrefKind
{
	EdxlaggkindNormal = 0,
	EdxlaggkindOrderedSet,
	EdxlaggkindHypothetical
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarAggref
//
//	@doc:
//		Class for representing DXL AggRef
//
//---------------------------------------------------------------------------
class CDXLScalarAggref : public CDXLScalar
{
private:
	// catalog id of the function
	IMDId *m_agg_func_mdid;

	// resolved return type refers to a non-ambiguous type that was resolved during query
	// parsing if the actual return type of Agg is ambiguous (e.g., AnyElement in GPDB)
	// if resolved return type is NULL, then we can get Agg return type by looking up MD cache
	// using Agg MDId
	IMDId *m_resolved_rettype_mdid;

	// Denotes whether it's agg(DISTINCT ...)
	BOOL m_is_distinct;

	// Denotes the MPP Stage
	EdxlAggrefStage m_agg_stage;

	EdxlAggrefKind m_aggkind;

	ULongPtrArray *m_argtypes;

public:
	CDXLScalarAggref(const CDXLScalarAggref &) = delete;

	// ctor/dtor
	CDXLScalarAggref(CMemoryPool *mp, IMDId *agg_mdid, IMDId *resolved_rettype,
					 BOOL is_distinct, EdxlAggrefStage agg_stage,
					 EdxlAggrefKind aggkind, ULongPtrArray *argtypes);

	~CDXLScalarAggref() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	const CWStringConst *GetOpNameStr() const override;

	IMDId *GetDXLAggFuncMDid() const;

	IMDId *GetDXLResolvedRetTypeMDid() const;

	const CWStringConst *GetDXLStrAggStage() const;

	EdxlAggrefStage GetDXLAggStage() const;

	const CWStringConst *GetDXLStrAggKind() const;

	BOOL IsDistinct() const;

	EdxlAggrefKind
	GetAggKind() const
	{
		return m_aggkind;
	}

	ULongPtrArray *
	GetArgTypes() const
	{
		return m_argtypes;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	void SerializeValuesListChildToDXL(CXMLSerializer *xml_serializer,
									   const CDXLNode *dxlnode, ULONG index,
									   const CHAR *attr_name) const;

	// conversion function
	static CDXLScalarAggref *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarAggref == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarAggref *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarAggref_H

// EOF
