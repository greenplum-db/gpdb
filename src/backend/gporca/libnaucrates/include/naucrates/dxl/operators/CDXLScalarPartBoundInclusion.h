//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CDXLScalarPartBoundInclusion.h
//
//	@doc:
//		Class for representing DXL Part boundary inclusion expressions
//		These expressions indicate whether a particular boundary of a part
//		constraint is closed (inclusive) or open (exclusive)
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarPartBoundInclusion_H
#define GPDXL_CDXLScalarPartBoundInclusion_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarPartBoundInclusion
//
//	@doc:
//		Class for representing DXL Part boundary inclusion expressions
//		These expressions are created and consumed by the PartitionSelector operator
//
//---------------------------------------------------------------------------
class CDXLScalarPartBoundInclusion : public CDXLScalar
{
private:
	// partitioning level
	ULONG m_partitioning_level;

	// whether this represents a lower or upper bound
	BOOL m_is_lower_bound;

	// private copy ctor
	CDXLScalarPartBoundInclusion(const CDXLScalarPartBoundInclusion &);

public:
	// ctor
	CDXLScalarPartBoundInclusion(CMemoryPool *mp, ULONG partitioning_level,
								 BOOL is_lower_bound);

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

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
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	virtual void AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarPartBoundInclusion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarPartBoundInclusion == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarPartBoundInclusion *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarPartBoundInclusion_H

// EOF
