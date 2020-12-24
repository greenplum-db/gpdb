//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarPartBound.h
//
//	@doc:
//		Class for representing DXL Part boundary expressions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarPartBound_H
#define GPDXL_CDXLScalarPartBound_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarPartBound
//
//	@doc:
//		Class for representing DXL Part boundary expressions
//		These expressions are created and consumed by the PartitionSelector operator
//
//---------------------------------------------------------------------------
class CDXLScalarPartBound : public CDXLScalar
{
private:
	// partitioning level
	ULONG m_partitioning_level;

	// boundary type
	IMDId *m_mdid_type;

	// whether this represents a lower or upper bound
	BOOL m_is_lower_bound;

public:
	CDXLScalarPartBound(const CDXLScalarPartBound &) = delete;

	// ctor
	CDXLScalarPartBound(CMemoryPool *mp, ULONG partitioning_level,
						IMDId *mdid_type, BOOL is_lower_bound);

	// dtor
	~CDXLScalarPartBound() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// partitioning level
	ULONG
	GetPartitioningLevel() const
	{
		return m_partitioning_level;
	}

	// boundary type
	IMDId *
	MdidType() const
	{
		return m_mdid_type;
	}

	// is this a lower (or upper) bound
	BOOL
	IsLowerBound() const
	{
		return m_is_lower_bound;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarPartBound *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarPartBound == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarPartBound *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarPartBound_H

// EOF
