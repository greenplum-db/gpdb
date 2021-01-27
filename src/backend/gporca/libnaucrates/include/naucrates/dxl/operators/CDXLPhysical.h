//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysical.h
//
//	@doc:
//		Base class for DXL physical operators.
//---------------------------------------------------------------------------


#ifndef GPDXL_CDXLPhysical_H
#define GPDXL_CDXLPhysical_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLOperator.h"
#include "naucrates/dxl/operators/CDXLPhysicalProperties.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CXMLSerializer;
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysical
//
//	@doc:
//		Base class the DXL physical operators
//
//---------------------------------------------------------------------------
class CDXLPhysical : public CDXLOperator
{
private:
	// private copy ctor
	CDXLPhysical(const CDXLPhysical &);

public:
	// ctor/dtor
	explicit CDXLPhysical(CMemoryPool *mp);

	virtual ~CDXLPhysical();

	// Get operator type
	Edxloptype GetDXLOperatorType() const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	virtual void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysical_H

// EOF
