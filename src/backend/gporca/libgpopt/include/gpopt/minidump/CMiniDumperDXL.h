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
public:
	CMiniDumperDXL(const CMiniDumperDXL &) = delete;

	// ctor
	CMiniDumperDXL();

	// dtor
	~CMiniDumperDXL() override;

	// serialize minidump header
	void SerializeHeader() override;

	// serialize minidump footer
	void SerializeFooter() override;

	// serialize entry header
	void SerializeEntryHeader() override;

	// serialize entry footer
	void SerializeEntryFooter() override;

};	// class CMiniDumperDXL
}  // namespace gpopt

#endif	// !GPOPT_CMiniDumperDXL_H

// EOF
