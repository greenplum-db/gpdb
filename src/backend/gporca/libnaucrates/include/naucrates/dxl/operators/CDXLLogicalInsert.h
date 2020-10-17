//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalInsert.h
//
//	@doc:
//		Class for representing logical insert operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalInsert_H
#define GPDXL_CDXLLogicalInsert_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
using namespace gpmd;

// fwd decl
class CDXLTableDescr;

//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalInsert
//
//	@doc:
//		Class for representing logical insert operators
//
//---------------------------------------------------------------------------
class CDXLLogicalInsert : public CDXLLogical
{
private:
	// target table descriptor
	CDXLTableDescr *m_dxl_table_descr;

	// list of source column ids
	ULongPtrArray *m_src_colids_array;

	// private copy ctor
	CDXLLogicalInsert(const CDXLLogicalInsert &);

public:
	// ctor/dtor
	CDXLLogicalInsert(CMemoryPool *mp, CDXLTableDescr *table_descr,
					  ULongPtrArray *src_colids_array);

	virtual ~CDXLLogicalInsert();

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

	// target table descriptor
	CDXLTableDescr *
	GetDXLTableDescr() const
	{
		return m_dxl_table_descr;
	}

	// source column ids
	ULongPtrArray *
	GetSrcColIdsArray() const
	{
		return m_src_colids_array;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLLogicalInsert *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalInsert == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalInsert *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLLogicalInsert_H

// EOF
