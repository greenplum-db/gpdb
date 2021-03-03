//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarHashExpr.h
//
//	@doc:
//		Class for representing hash expressions list in DXL Redistribute motion nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarHashExpr_H
#define GPDXL_CDXLScalarHashExpr_H

#include "gpos/base.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarHashExpr
//
//	@doc:
//		Hash expressions list in Redistribute motion nodes
//
//---------------------------------------------------------------------------
class CDXLScalarHashExpr : public CDXLScalar
{
private:
	// catalog Oid of the distribution opfamily
	IMDId *m_mdid_opfamily;

public:
	CDXLScalarHashExpr(CDXLScalarHashExpr &) = delete;

	// ctor/dtor
	CDXLScalarHashExpr(CMemoryPool *mp, IMDId *mdid_type);

	~CDXLScalarHashExpr() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	IMDId *MdidOpfamily() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarHashExpr *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarHashExpr == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarHashExpr *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarHashExpr_H

// EOF
