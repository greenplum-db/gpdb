//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CLoggerSyslog.h
//
//	@doc:
//		Implementation of logging interface over syslog
//---------------------------------------------------------------------------
#ifndef GPOS_CLoggerSyslog_H
#define GPOS_CLoggerSyslog_H

#include "gpos/error/CLogger.h"

#define GPOS_SYSLOG_ALERT(szMsg) CLoggerSyslog::Alert(GPOS_WSZ_LIT(szMsg))

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CLoggerSyslog
//
//	@doc:
//		Syslog logging.
//
//---------------------------------------------------------------------------

class CLoggerSyslog : public CLogger
{
private:
	// executable name
	const CHAR *m_proc_name;

	// initialization flags
	ULONG m_init_mask;

	// message priotity
	ULONG m_message_priority;

	// write string to syslog
	void Write(const WCHAR *log_entry, ULONG severity) override;

	static CLoggerSyslog m_alert_logger;

public:
	CLoggerSyslog(const CLoggerSyslog &) = delete;

	// ctor
	CLoggerSyslog(const CHAR *proc_name, ULONG init_mask,
				  ULONG message_priority);

	// dtor
	~CLoggerSyslog() override;

	// write alert message to syslog - use ASCII characters only
	static void Alert(const WCHAR *msg);

};	// class CLoggerSyslog
}  // namespace gpos

#endif	// !GPOS_CLoggerSyslog_H

// EOF
