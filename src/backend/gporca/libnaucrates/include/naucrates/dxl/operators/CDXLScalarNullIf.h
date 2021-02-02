//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarNullIf.h
//
//	@doc:
//		Class for representing DXL scalar NullIf operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarNullIf_H
#define GPDXL_CDXLScalarNullIf_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarNullIf
//
//	@doc:
//		Class for representing DXL scalar NullIf operator
//
//---------------------------------------------------------------------------
class CDXLScalarNullIf : public CDXLScalar
{
private:
	// operator number
	IMDId *m_mdid_op;

	// return type
	IMDId *m_mdid_type;

public:
	CDXLScalarNullIf(CDXLScalarNullIf &) = delete;

	// ctor
	CDXLScalarNullIf(CMemoryPool *mp, IMDId *mdid_op, IMDId *mdid_type);

	// dtor
	~CDXLScalarNullIf() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const override;

	// operator id
	virtual IMDId *MdIdOp() const;

	// return type
	virtual IMDId *MdidType() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

	// conversion function
	static CDXLScalarNullIf *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarNullIf == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarNullIf *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarNullIf_H


// EOF
