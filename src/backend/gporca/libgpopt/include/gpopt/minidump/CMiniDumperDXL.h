//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperDXL.h
//
//	@doc:
//		DXL-based minidump handler;
//---------------------------------------------------------------------------
#ifndef GPOPT_CMiniDumperDXL_H
#define GPOPT_CMiniDumperDXL_H

#include "gpos/base.h"
#include "gpos/error/CMiniDumper.h"

using namespace gpos;

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CMiniDumperDXL
//
//	@doc:
//		DXL-specific minidump handler
//
//---------------------------------------------------------------------------
class CMiniDumperDXL : public CMiniDumper
{
private:
	// private copy ctor
	CMiniDumperDXL(const CMiniDumperDXL &);

public:
	// ctor
	explicit CMiniDumperDXL(CMemoryPool *mp);

	// dtor
	virtual ~CMiniDumperDXL();

	// serialize minidump header
	virtual void SerializeHeader();

	// serialize minidump footer
	virtual void SerializeFooter();

	// serialize entry header
	virtual void SerializeEntryHeader();

	// serialize entry footer
	virtual void SerializeEntryFooter();

};	// class CMiniDumperDXL
}  // namespace gpopt

#endif	// !GPOPT_CMiniDumperDXL_H

// EOF
