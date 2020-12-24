//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarBitmapIndexProbe.h
//
//	@doc:
//		Class for representing DXL bitmap index probe operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarBitmapIndexProbe_H
#define GPDXL_CDXLScalarBitmapIndexProbe_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
using namespace gpos;

// fwd declarations
class CDXLIndexDescr;
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarBitmapIndexProbe
//
//	@doc:
//		Class for representing DXL bitmap index probe operators
//
//---------------------------------------------------------------------------
class CDXLScalarBitmapIndexProbe : public CDXLScalar
{
private:
	// index descriptor associated with the scanned table
	CDXLIndexDescr *m_dxl_index_descr;

public:
	CDXLScalarBitmapIndexProbe(CDXLScalarBitmapIndexProbe &) = delete;

	// ctor
	CDXLScalarBitmapIndexProbe(CMemoryPool *mp,
							   CDXLIndexDescr *dxl_index_descr);

	//dtor
	~CDXLScalarBitmapIndexProbe() override;

	// operator type
	Edxlopid
	GetDXLOperator() const override
	{
		return EdxlopScalarBitmapIndexProbe;
	}

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// index descriptor
	virtual const CDXLIndexDescr *
	GetDXLIndexDescr() const
	{
		return m_dxl_index_descr;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

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

	// conversion function
	static CDXLScalarBitmapIndexProbe *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarBitmapIndexProbe == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarBitmapIndexProbe *>(dxl_op);
	}

};	// class CDXLScalarBitmapIndexProbe
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarBitmapIndexProbe_H

// EOF
