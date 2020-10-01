//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarSwitch.h
//
//	@doc:
//
//		Class for representing DXL Switch (corresponds to Case (expr) ...)
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSwitch_H
#define GPDXL_CDXLScalarSwitch_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSwitch
//
//	@doc:
//		Class for representing DXL Switch
//
//---------------------------------------------------------------------------
class CDXLScalarSwitch : public CDXLScalar
{
private:
	// return type
	IMDId *m_mdid_type;

	// private copy ctor
	CDXLScalarSwitch(const CDXLScalarSwitch &);

public:
	// ctor
	CDXLScalarSwitch(CMemoryPool *mp, IMDId *mdid_type);

	//dtor
	virtual ~CDXLScalarSwitch();

	// name of the operator
	virtual const CWStringConst *GetOpNameStr() const;

	// return type
	virtual IMDId *MdidType() const;

	// DXL Operator ID
	virtual Edxlopid GetDXLOperator() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarSwitch *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarSwitch == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSwitch *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL HasBoolResult(CMDAccessor *md_accessor) const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarSwitch_H

// EOF
