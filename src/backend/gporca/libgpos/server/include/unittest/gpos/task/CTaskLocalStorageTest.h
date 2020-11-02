//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CTaskLocalStorageTest.h
//
//	@doc:
//		Task-local storage facility; implements TLS to store an instance
//---------------------------------------------------------------------------
#ifndef GPOS_CTaskLocalStorageTest_H
#define GPOS_CTaskLocalStorageTest_H

#include "gpos/base.h"
#include "gpos/common/CSyncHashtable.h"
#include "gpos/task/CTaskLocalStorageObject.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CTaskLocalStorageTest
//
//	@doc:
//		Unittest for TLS implementation
//
//---------------------------------------------------------------------------
class CTaskLocalStorageTest
{
private:
	//---------------------------------------------------------------------------
	//	@class:
	//		CTestObject
	//
	//	@doc:
	//		Simple subclass of CTaskLocalStorageObject
	//
	//---------------------------------------------------------------------------
	class CTestObject : public CTaskLocalStorageObject
	{
	public:
		// ctor
		CTestObject() : CTaskLocalStorageObject(CTaskLocalStorage::EtlsidxTest)
		{
		}
	};

public:
	// actual unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();
	static GPOS_RESULT EresUnittest_TraceFlags();
};

}  // namespace gpos

#endif	// !GPOS_CTaskLocalStorageTest_H

// EOF
