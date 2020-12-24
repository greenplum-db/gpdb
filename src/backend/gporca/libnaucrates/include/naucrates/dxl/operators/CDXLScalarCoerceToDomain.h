//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarCoerceToDomain.h
//
//	@doc:
//		Class for representing DXL CoerceToDomain operation,
//		the operator captures coercing a value to a domain type,
//
//		at runtime, the precise set of constraints to be checked against
//		value are determined,
//		if the value passes, it is returned as the result, otherwise an error
//		is raised.
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarCoerceToDomain_H
#define GPDXL_CDXLScalarCoerceToDomain_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalarCoerceBase.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarCoerceToDomain
//
//	@doc:
//		Class for representing DXL casting operator
//
//---------------------------------------------------------------------------
class CDXLScalarCoerceToDomain : public CDXLScalarCoerceBase
{
private:
public:
	CDXLScalarCoerceToDomain(const CDXLScalarCoerceToDomain &) = delete;

	// ctor/dtor
	CDXLScalarCoerceToDomain(CMemoryPool *mp, IMDId *mdid_type,
							 INT type_modifier,
							 EdxlCoercionForm dxl_coerce_format, INT location);

	~CDXLScalarCoerceToDomain() override = default;

	// ident accessor
	Edxlopid
	GetDXLOperator() const override
	{
		return EdxlopScalarCoerceToDomain;
	}

	// name of the DXL operator name
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLScalarCoerceToDomain *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarCoerceToDomain == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarCoerceToDomain *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarCoerceToDomain_H

// EOF
