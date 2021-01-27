//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLScalarArrayRefIndexList.h
//
//	@doc:
//		Class for representing DXL scalar arrayref index list
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarArrayRefIndexList_H
#define GPDXL_CDXLScalarArrayRefIndexList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarArrayRefIndexList
//
//	@doc:
//		Class for representing DXL scalar arrayref index list
//
//---------------------------------------------------------------------------
class CDXLScalarArrayRefIndexList : public CDXLScalar
{
public:
	enum EIndexListBound
	{
		EilbLower,	// lower index
		EilbUpper,	// upper index
		EilbSentinel
	};

private:
	// index list bound
	EIndexListBound m_index_list_bound;

	// private copy ctor
	CDXLScalarArrayRefIndexList(const CDXLScalarArrayRefIndexList &);

	// string representation of index list bound
	static const CWStringConst *GetDXLIndexListBoundStr(
		EIndexListBound index_list_bound);

public:
	// ctor
	CDXLScalarArrayRefIndexList(CMemoryPool *mp,
								EIndexListBound index_list_bound);

	// ident accessors
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// index list bound
	EIndexListBound
	GetDXLIndexListBound() const
	{
		return m_index_list_bound;
	}

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
	static CDXLScalarArrayRefIndexList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarArrayRefIndexList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarArrayRefIndexList *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarArrayRefIndexList_H

// EOF
