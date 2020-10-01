//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CDXLScalarMinMax.h
//
//	@doc:
//		Class for representing DXL MinMax operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarMinMax_H
#define GPDXL_CDXLScalarMinMax_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarMinMax
//
//	@doc:
//		Class for representing DXL MinMax operator
//
//---------------------------------------------------------------------------
class CDXLScalarMinMax : public CDXLScalar
{
public:
	// types of operations: either min or max
	enum EdxlMinMaxType
	{
		EmmtMin,
		EmmtMax,
		EmmtSentinel
	};

private:
	// return type
	IMDId *m_mdid_type;

	// min/max type
	EdxlMinMaxType m_min_max_type;

	// private copy ctor
	CDXLScalarMinMax(const CDXLScalarMinMax &);

public:
	// ctor
	CDXLScalarMinMax(CMemoryPool *mp, IMDId *mdid_type,
					 EdxlMinMaxType min_max_type);

	//dtor
	virtual ~CDXLScalarMinMax();

	// name of the operator
	virtual const CWStringConst *GetOpNameStr() const;

	// return type
	virtual IMDId *
	MdidType() const
	{
		return m_mdid_type;
	}

	// DXL Operator ID
	virtual Edxlopid GetDXLOperator() const;

	// min/max type
	EdxlMinMaxType
	GetMinMaxType() const
	{
		return m_min_max_type;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// does the operator return a boolean result
	virtual BOOL HasBoolResult(CMDAccessor *md_accessor) const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarMinMax *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarMinMax == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarMinMax *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarMinMax_H

// EOF
