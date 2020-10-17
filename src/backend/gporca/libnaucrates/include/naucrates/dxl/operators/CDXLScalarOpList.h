//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLScalarOpList.h
//
//	@doc:
//		Class for representing DXL list of scalar expressions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarOpList_H
#define GPDXL_CDXLScalarOpList_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarOpList
//
//	@doc:
//		Class for representing DXL list of scalar expressions
//
//---------------------------------------------------------------------------
class CDXLScalarOpList : public CDXLScalar
{
public:
	// type of the operator list
	enum EdxlOpListType
	{
		EdxloplistEqFilterList,
		EdxloplistFilterList,
		EdxloplistGeneral,
		EdxloplistSentinel
	};

private:
	// operator list type
	EdxlOpListType m_dxl_op_list_type;

	// private copy ctor
	CDXLScalarOpList(const CDXLScalarOpList &);

public:
	// ctor
	CDXLScalarOpList(CMemoryPool *mp,
					 EdxlOpListType dxl_op_list_type = EdxloplistGeneral);

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

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
	virtual void AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarOpList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarOpList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarOpList *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarOpList_H

// EOF
