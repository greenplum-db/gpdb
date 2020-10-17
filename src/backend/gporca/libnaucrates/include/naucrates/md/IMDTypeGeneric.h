//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IMDTypeGeneric.h
//
//	@doc:
//		Interface for types in the metadata cache
//---------------------------------------------------------------------------



#ifndef GPMD_IMDTypeGeneric_H
#define GPMD_IMDTypeGeneric_H

#include "gpos/base.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/md/IMDType.h"

namespace gpmd
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		IMDTypeGeneric
//
//	@doc:
//		Interface for generic types in the metadata cache
//
//---------------------------------------------------------------------------
class IMDTypeGeneric : public IMDType
{
public:
	// type id
	static ETypeInfo
	GetTypeInfo()
	{
		return EtiGeneric;
	}

	// type id
	virtual ETypeInfo
	GetDatumType() const
	{
		return IMDTypeGeneric::GetTypeInfo();
	}
};
}  // namespace gpmd

#endif	// !GPMD_IMDTypeGeneric_H

// EOF
