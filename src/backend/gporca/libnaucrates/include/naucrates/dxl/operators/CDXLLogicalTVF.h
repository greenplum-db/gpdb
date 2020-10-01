//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalTVF.h
//
//	@doc:
//		Class for representing table-valued functions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalTVF_H
#define GPDXL_CDXLLogicalTVF_H

#include "gpos/base.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLColDescr.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalTVF
//
//	@doc:
//		Class for representing table-valued functions
//
//---------------------------------------------------------------------------
class CDXLLogicalTVF : public CDXLLogical
{
private:
	// catalog id of the function
	IMDId *m_func_mdid;

	// return type
	IMDId *m_return_type_mdid;

	// function name
	CMDName *m_mdname;

	// list of column descriptors
	CDXLColDescrArray *m_dxl_col_descr_array;

	// private copy ctor
	CDXLLogicalTVF(const CDXLLogicalTVF &);

public:
	// ctor/dtor
	CDXLLogicalTVF(CMemoryPool *mp, IMDId *mdid_func, IMDId *mdid_return_type,
				   CMDName *mdname, CDXLColDescrArray *pdrgdxlcd);

	virtual ~CDXLLogicalTVF();

	// get operator type
	Edxlopid GetDXLOperator() const;

	// get operator name
	const CWStringConst *GetOpNameStr() const;

	// get function name
	CMDName *
	MdName() const
	{
		return m_mdname;
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

	// get number of output columns
	ULONG Arity() const;

	// return the array of column descriptors
	const CDXLColDescrArray *
	GetDXLColumnDescrArray() const
	{
		return m_dxl_col_descr_array;
	}

	// get the column descriptor at the given position
	const CDXLColDescr *GetColumnDescrAt(ULONG ul) const;

	// check if given column is defined by operator
	virtual BOOL IsColDefined(ULONG colid) const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLLogicalTVF *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalTVF == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalTVF *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLLogicalTVF_H

// EOF
