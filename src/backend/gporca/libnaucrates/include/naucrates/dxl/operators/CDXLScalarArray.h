//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarArray.h
//
//	@doc:
//		Class for representing DXL scalar arrays
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarArray_H
#define GPDXL_CDXLScalarArray_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarArray
//
//	@doc:
//		Class for representing DXL arrays
//
//---------------------------------------------------------------------------
class CDXLScalarArray : public CDXLScalar
{
private:
	// base element type id
	IMDId *m_elem_type_mdid;

	// array type id
	IMDId *m_array_type_mdid;

	// is it a multidimensional array
	BOOL m_multi_dimensional_array;

public:
	CDXLScalarArray(const CDXLScalarArray &) = delete;

	// ctor
	CDXLScalarArray(CMemoryPool *mp, IMDId *elem_type_mdid,
					IMDId *array_type_mdid, BOOL multi_dimensional_array);

	// dtor
	~CDXLScalarArray() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// element type id
	IMDId *ElementTypeMDid() const;

	// array type id
	IMDId *ArrayTypeMDid() const;

	// is array multi-dimensional
	BOOL IsMultiDimensional() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLScalarArray *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarArray == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarArray *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarArray_H

// EOF
