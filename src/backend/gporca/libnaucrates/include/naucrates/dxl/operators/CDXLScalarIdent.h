//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarIdent.h
//
//	@doc:
//		Class for representing DXL scalar identifier operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarIdent_H
#define GPDXL_CDXLScalarIdent_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLColRef.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarIdent
//
//	@doc:
//		Class for representing DXL scalar identifier operators.
//
//---------------------------------------------------------------------------
class CDXLScalarIdent : public CDXLScalar
{
private:
	// column reference
	CDXLColRef *m_dxl_colref;

	// private copy ctor
	CDXLScalarIdent(CDXLScalarIdent &);

public:
	// ctor/dtor
	CDXLScalarIdent(CMemoryPool *, CDXLColRef *);

	virtual ~CDXLScalarIdent();

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the operator
	const CWStringConst *GetOpNameStr() const;

	// accessors
	const CDXLColRef *GetDXLColRef() const;

	IMDId *MdidType() const;

	INT TypeModifier() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLScalarIdent *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarIdent == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarIdent *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL HasBoolResult(CMDAccessor *md_accessor) const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl



#endif	// !GPDXL_CDXLScalarIdent_H

// EOF
