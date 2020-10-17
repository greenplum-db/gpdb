//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarComp.h
//
//	@doc:
//		Class for representing DXL scalar comparison operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarComp_H
#define GPDXL_CDXLScalarComp_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

// indices of scalar comparison elements in the children array
enum Edxlsccmp
{
	EdxlsccmpIndexLeft = 0,
	EdxlsccmpIndexRight,
	EdxlsccmpSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarComp
//
//	@doc:
//		Class for representing DXL scalar comparison operators
//
//---------------------------------------------------------------------------
class CDXLScalarComp : public CDXLScalar
{
protected:
	// operator number in the catalog
	IMDId *m_mdid;

	// comparison operator name
	const CWStringConst *m_comparison_operator_name;

private:
	// private copy ctor
	CDXLScalarComp(CDXLScalarComp &);

public:
	// ctor/dtor
	CDXLScalarComp(CMemoryPool *mp, IMDId *operator_mdid,
				   const CWStringConst *comparison_operator_name);

	virtual ~CDXLScalarComp();

	// accessor

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const;

	// name of the comparison operator
	const CWStringConst *GetComparisonOpName() const;

	// operator id
	IMDId *MDId() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLScalarComp *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarCmp == dxl_op->GetDXLOperator() ||
					EdxlopScalarDistinct == dxl_op->GetDXLOperator() ||
					EdxlopScalarArrayComp == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarComp *>(dxl_op);
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
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarComp_H


// EOF
