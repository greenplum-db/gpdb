//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CDXLPhysicalDynamicForeignScan.h
//
//	@doc:
//		Class for representing DXL multi foreign scan operators
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalDynamicForeignScan_H
#define GPDXL_CDXLPhysicalDynamicForeignScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"


namespace gpdxl
{
// indices of dynamic foreign scan elements in the children array
enum Edxldfs
{
	EdxldfsIndexProjList = 0,
	EdxldfsIndexFilter,
	EdxldfsSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalDynamicForeignScan
//
//	@doc:
//		Class for representing DXL multi foreign scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalDynamicForeignScan : public CDXLPhysical
{
private:
	// table descriptor for the scanned table
	CDXLTableDescr *m_dxl_table_descr;

	IMdIdArray *m_part_mdids;

	ULongPtrArray *m_selector_ids = nullptr;

	OID m_foreign_server_oid;

public:
	CDXLPhysicalDynamicForeignScan(CDXLPhysicalDynamicForeignScan &) = delete;

	// ctor
	CDXLPhysicalDynamicForeignScan(CMemoryPool *mp, CDXLTableDescr *table_descr,
								   IMdIdArray *part_mdids,
								   ULongPtrArray *selector_ids,
								   OID foreign_server_oid);

	// dtor
	~CDXLPhysicalDynamicForeignScan() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// table descriptor
	const CDXLTableDescr *GetDXLTableDescr() const;

	IMdIdArray *GetParts() const;

	const ULongPtrArray *
	GetSelectorIds() const
	{
		return m_selector_ids;
	}

	OID
	GetForeignServerOid() const
	{
		return m_foreign_server_oid;
	}
	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLPhysicalDynamicForeignScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalDynamicForeignScan ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalDynamicForeignScan *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalDynamicForeignScan_H

// EOF
