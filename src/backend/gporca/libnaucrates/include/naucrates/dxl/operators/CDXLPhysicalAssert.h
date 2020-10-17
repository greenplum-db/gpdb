//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalAssert.h
//
//	@doc:
//		Class for representing DXL physical assert operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalAssert_H
#define GPDXL_CDXLPhysicalAssert_H

#include "gpos/base.h"
#include "naucrates/dxl/errorcodes.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalAssert
//
//	@doc:
//		Class for representing DXL assert operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalAssert : public CDXLPhysical
{
public:
	// indices of assert elements in the children array
	enum Edxlassert
	{
		EdxlassertIndexProjList = 0,
		EdxlassertIndexFilter,
		EdxlassertIndexChild,
		EdxlassertIndexSentinel
	};

private:
	// error code
	CHAR m_sql_state[GPOS_SQLSTATE_LENGTH + 1];

	// private copy ctor
	CDXLPhysicalAssert(CDXLPhysicalAssert &);

public:
	// ctor
	CDXLPhysicalAssert(CMemoryPool *mp, const CHAR *sql_state);

	// dtor
	virtual ~CDXLPhysicalAssert();

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// error code
	const CHAR *
	GetSQLState() const
	{
		return m_sql_state;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLPhysicalAssert *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalAssert == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalAssert *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalAssert_H

// EOF
