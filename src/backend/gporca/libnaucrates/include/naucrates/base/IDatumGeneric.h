//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IDatumGeneric.h
//
//	@doc:
//		Base abstract class for generic datum representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_IDatumGeneric_H
#define GPNAUCRATES_IDatumGeneric_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/base/IDatum.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		IDatumGeneric
//
//	@doc:
//		Base abstract class for generic datum representation
//
//---------------------------------------------------------------------------
class IDatumGeneric : public IDatum
{
private:
public:
	IDatumGeneric(const IDatumGeneric &) = delete;

	// ctor
	IDatumGeneric() = default;

	// dtor
	~IDatumGeneric() override = default;

	// accessor for datum type
	IMDType::ETypeInfo
	GetDatumType() override
	{
		return IMDType::EtiGeneric;
	}

};	// class IDatumGeneric

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_IDatumGeneric_H

// EOF
