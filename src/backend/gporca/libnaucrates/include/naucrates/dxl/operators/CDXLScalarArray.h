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

	// private copy ctor
	CDXLScalarArray(const CDXLScalarArray &);

public:
	// ctor
	CDXLScalarArray(CMemoryPool *mp, IMDId *elem_type_mdid,
					IMDId *array_type_mdid, BOOL multi_dimensional_array);

	// dtor
	virtual ~CDXLScalarArray();

	// ident accessors
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// element type id
	IMDId *ElementTypeMDid() const;

	// array type id
	IMDId *ArrayTypeMDid() const;

	// is array multi-dimensional
	BOOL IsMultiDimensional() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarArray *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarArray == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarArray *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarArray_H

// EOF
