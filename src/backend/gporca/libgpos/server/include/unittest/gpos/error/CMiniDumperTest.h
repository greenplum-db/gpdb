//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperTest.h
//
//	@doc:
//		Test for minidump handler
//---------------------------------------------------------------------------
#ifndef GPOS_CMiniDumperTest_H
#define GPOS_CMiniDumperTest_H

#include "gpos/base.h"
#include "gpos/error/CMiniDumper.h"
#include "gpos/error/CSerializable.h"

#define GPOS_MINIDUMP_BUF_SIZE (1024 * 8)

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CMiniDumperTest
//
//	@doc:
//		Static unit tests for minidump handler
//
//---------------------------------------------------------------------------
class CMiniDumperTest
{
private:
	//---------------------------------------------------------------------------
	//	@class:
	//		CMiniDumperStream
	//
	//	@doc:
	//		Local minidump handler
	//
	//---------------------------------------------------------------------------
	class CMiniDumperStream : public CMiniDumper
	{
	public:
		// ctor
		CMiniDumperStream(CMemoryPool *mp);

		// dtor
		virtual ~CMiniDumperStream();

		// serialize minidump header
		virtual void SerializeHeader();

		// serialize minidump footer
		virtual void SerializeFooter();

		// serialize entry header
		virtual void SerializeEntryHeader();

		// serialize entry footer
		virtual void SerializeEntryFooter();

	};	// class CMiniDumperStream

	//---------------------------------------------------------------------------
	//	@class:
	//		CSerializableStack
	//
	//	@doc:
	//		Stack serializer
	//
	//---------------------------------------------------------------------------
	class CSerializableStack : public CSerializable
	{
	public:
		// ctor
		CSerializableStack();

		// dtor
		virtual ~CSerializableStack();

		// serialize object to passed stream
		virtual void Serialize(COstream &oos);

	};	// class CSerializableStack

	// helper functions
	static void *PvRaise(void *);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CMiniDumperTest
}  // namespace gpos

#endif	// !GPOS_CMiniDumperTest_H

// EOF
