//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalGet.h
//
//	@doc:
//		Class for representing DXL logical get operators
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalGet_H
#define GPDXL_CDXLLogicalGet_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalGet
//
//	@doc:
//		Class for representing DXL logical get operators
//
//---------------------------------------------------------------------------
class CDXLLogicalGet : public CDXLLogical
{
private:
	// table descriptor for the scanned table
	CDXLTableDescr *m_dxl_table_descr;

public:
	CDXLLogicalGet(CDXLLogicalGet &) = delete;

	// ctor
	CDXLLogicalGet(CMemoryPool *mp, CDXLTableDescr *table_descr);

	// dtor
	~CDXLLogicalGet() override;

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;
	CDXLTableDescr *GetDXLTableDescr() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// check if given column is defined by operator
	BOOL IsColDefined(ULONG colid) const override;

	// conversion function
	static CDXLLogicalGet *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopLogicalGet == dxl_op->GetDXLOperator() ||
					EdxlopLogicalForeignGet == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalGet *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalGet_H

// EOF
