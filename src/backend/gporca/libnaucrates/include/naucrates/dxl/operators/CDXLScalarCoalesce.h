//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarCoalesce.h
//
//	@doc:
//		Class for representing DXL Coalesce operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarCoalesce_H
#define GPDXL_CDXLScalarCoalesce_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarCoalesce
//
//	@doc:
//		Class for representing DXL Coalesce operator
//
//---------------------------------------------------------------------------
class CDXLScalarCoalesce : public CDXLScalar
{
private:
	// return type
	IMDId *m_mdid_type;

	// private copy ctor
	CDXLScalarCoalesce(const CDXLScalarCoalesce &);

public:
	// ctor
	CDXLScalarCoalesce(CMemoryPool *mp, IMDId *mdid_type);

	//dtor
	virtual ~CDXLScalarCoalesce();

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
	static CDXLScalarCoalesce *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarCoalesce == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarCoalesce *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarCoalesce_H

// EOF
