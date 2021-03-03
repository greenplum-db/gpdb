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
		CMiniDumperStream();

		// dtor
		~CMiniDumperStream() override;

		// serialize minidump header
		void SerializeHeader() override;

		// serialize minidump footer
		void SerializeFooter() override;

		// serialize entry header
		void SerializeEntryHeader() override;

		// serialize entry footer
		void SerializeEntryFooter() override;

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
		~CSerializableStack() override;

		// serialize object to passed stream
		void Serialize(COstream &oos) override;

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
