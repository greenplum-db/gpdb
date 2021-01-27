//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarArrayComp.h
//
//	@doc:
//		Class for representing DXL scalar array comparison such as in, not in, any, all
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarArrayComp_H
#define GPDXL_CDXLScalarArrayComp_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarComp.h"


namespace gpdxl
{
using namespace gpos;

enum EdxlArrayCompType
{
	Edxlarraycomptypeany = 0,
	Edxlarraycomptypeall
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarArrayComp
//
//	@doc:
//		Class for representing DXL scalar Array OpExpr
//
//---------------------------------------------------------------------------
class CDXLScalarArrayComp : public CDXLScalarComp
{
private:
	EdxlArrayCompType m_comparison_type;

	// private copy ctor
	CDXLScalarArrayComp(const CDXLScalarArrayComp &);

	const CWStringConst *GetDXLStrArrayCmpType() const;

public:
	// ctor/dtor
	CDXLScalarArrayComp(CMemoryPool *mp, IMDId *mdid_op,
						const CWStringConst *str_opname,
						EdxlArrayCompType comparison_type);

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const;

	//accessors
	BOOL HasBoolResult() const;
	EdxlArrayCompType GetDXLArrayCmpType() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarArrayComp *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarArrayComp == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarArrayComp *>(dxl_op);
	}

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
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarArrayComp_H

// EOF
