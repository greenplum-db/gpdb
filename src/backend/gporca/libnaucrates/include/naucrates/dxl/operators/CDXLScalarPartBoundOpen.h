//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarPartBoundOpen.h
//
//	@doc:
//		Class for representing DXL Part boundary openness expressions
//		These expressions indicate whether a particular boundary of a part
//		constraint is open (unbounded)
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarPartBoundOpen_H
#define GPDXL_CDXLScalarPartBoundOpen_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarPartBoundOpen
//
//	@doc:
//		Class for representing DXL Part boundary openness expressions
//		These expressions are created and consumed by the PartitionSelector operator
//
//---------------------------------------------------------------------------
class CDXLScalarPartBoundOpen : public CDXLScalar
{
private:
	// partitioning level
	ULONG m_partitioning_level;

	// whether this represents a lower or upper bound
	BOOL m_is_lower_bound;

public:
	CDXLScalarPartBoundOpen(const CDXLScalarPartBoundOpen &) = delete;

	// ctor
	CDXLScalarPartBoundOpen(CMemoryPool *mp, ULONG partitioning_level,
							BOOL is_lower_bound);

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
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarPartBoundOpen *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarPartBoundOpen == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarPartBoundOpen *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarPartBoundOpen_H

// EOF
