//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CTaskLocalStorageObjectObject.h
//
//	@doc:
//		Base class for objects stored in TLS
//---------------------------------------------------------------------------
#ifndef GPOS_CTaskLocalStorageObject_H
#define GPOS_CTaskLocalStorageObject_H

#include "gpos/base.h"
#include "gpos/task/CTaskLocalStorage.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CTaskLocalStorageObject
//
//	@doc:
//		Abstract TLS object base class; provides hooks for hash table, key, and
//		hash function;
//
//---------------------------------------------------------------------------
class CTaskLocalStorageObject
{
private:
public:
	CTaskLocalStorageObject(const CTaskLocalStorageObject &) = delete;

	// ctor
	CTaskLocalStorageObject(CTaskLocalStorage::Etlsidx etlsidx)
		: m_etlsidx(etlsidx)
	{
		GPOS_ASSERT(CTaskLocalStorage::EtlsidxSentinel > etlsidx &&
					"TLS index out of range");
	}

	// dtor
	virtual ~CTaskLocalStorageObject() = default;

	// accessor
	const CTaskLocalStorage::Etlsidx &
	idx() const
	{
		return m_etlsidx;
	}

	// link
	SLink m_link;

	// key
	const CTaskLocalStorage::Etlsidx m_etlsidx;

};	// class CTaskLocalStorageObject
}  // namespace gpos

#endif	// !GPOS_CTaskLocalStorageObject_H

// EOF
