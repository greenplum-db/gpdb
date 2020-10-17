//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalTVF.h
//
//	@doc:
//		Class for representing DXL physical table-valued functions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalTVF_H
#define GPDXL_CDXLPhysicalTVF_H

#include "gpos/base.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLColDescr.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalTVF
//
//	@doc:
//		Class for representing DXL physical table-valued functions
//
//---------------------------------------------------------------------------
class CDXLPhysicalTVF : public CDXLPhysical
{
private:
	// function mdid
	IMDId *m_func_mdid;

	// return type
	IMDId *m_return_type_mdid;

	// function name
	CWStringConst *func_name;

	// private copy ctor
	CDXLPhysicalTVF(const CDXLPhysicalTVF &);

public:
	// ctor
	CDXLPhysicalTVF(CMemoryPool *mp, IMDId *mdid_func, IMDId *mdid_return_type,
					CWStringConst *str);

	// dtor
	virtual ~CDXLPhysicalTVF();

	// get operator type
	Edxlopid GetDXLOperator() const;

	// get operator name
	const CWStringConst *GetOpNameStr() const;

	// get function name
	CWStringConst *
	Pstr() const
	{
		return func_name;
	}

	// get function id
	IMDId *
	FuncMdId() const
	{
		return m_func_mdid;
	}

	// get return type
	IMDId *
	ReturnTypeMdId() const
	{
		return m_return_type_mdid;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalTVF *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalTVF == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalTVF *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalTVF_H

// EOF
