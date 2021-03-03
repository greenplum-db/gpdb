//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumper.h
//
//	@doc:
//		Interface for minidump handler;
//---------------------------------------------------------------------------
#ifndef GPOS_CMiniDumper_H
#define GPOS_CMiniDumper_H

#include "gpos/base.h"
#include "gpos/common/CStackObject.h"
#include "gpos/io/COstream.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CMiniDumper
//
//	@doc:
//		Interface for minidump handler;
//
//---------------------------------------------------------------------------
class CMiniDumper : CStackObject
{
private:
	// flag indicating if handler is initialized
	BOOL m_initialized{false};

	// flag indicating if handler is finalized
	BOOL m_finalized{false};

protected:
	// stream to serialize objects to
	COstream *m_oos{nullptr};

public:
	CMiniDumper(const CMiniDumper &) = delete;

	// ctor
	CMiniDumper();

	// dtor
	virtual ~CMiniDumper();

	// initialize
	void Init(COstream *oos);

	// finalize
	void Finalize();

	// get stream to serialize to
	COstream &GetOStream();

	// serialize minidump header
	virtual void SerializeHeader() = 0;

	// serialize minidump footer
	virtual void SerializeFooter() = 0;

	// serialize entry header
	virtual void SerializeEntryHeader() = 0;

	// serialize entry footer
	virtual void SerializeEntryFooter() = 0;

};	// class CMiniDumper
}  // namespace gpos

#endif	// !GPOS_CMiniDumper_H

// EOF
