//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperDXL.cpp
//
//	@doc:
//		Implementation of DXL-specific minidump handler
//---------------------------------------------------------------------------

#include "gpopt/minidump/CMiniDumperDXL.h"

#include "gpos/base.h"
#include "gpos/string/CWStringBase.h"
#include "gpos/task/CTask.h"
#include "gpos/task/CWorker.h"

#include "naucrates/dxl/xml/CDXLSections.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpopt;

// size of temporary buffer for expanding XML entry header
#define GPOPT_THREAD_HEADER_SIZE 128

//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXL::CMiniDumperDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMiniDumperDXL::CMiniDumperDXL(CMemoryPool *mp) : CMiniDumper(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXL::~CMiniDumperDXL
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMiniDumperDXL::~CMiniDumperDXL()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXL::SerializeHeader
//
//	@doc:
//		Serialize minidump header
//
//---------------------------------------------------------------------------
void
CMiniDumperDXL::SerializeHeader()
{
	*m_oos << CDXLSections::m_wszDocumentHeader;
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXL::SerializeFooter
//
//	@doc:
//		Serialize minidump footer
//
//---------------------------------------------------------------------------
void
CMiniDumperDXL::SerializeFooter()
{
	*m_oos << CDXLSections::m_wszDocumentFooter;
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXL::SerializeEntryHeader
//
//	@doc:
//		Serialize entry header
//
//---------------------------------------------------------------------------
void
CMiniDumperDXL::SerializeEntryHeader()
{
	WCHAR wszBuffer[GPOPT_THREAD_HEADER_SIZE];

	CWStringStatic str(wszBuffer, GPOS_ARRAY_SIZE(wszBuffer));
	str.AppendFormat(CDXLSections::m_wszThreadHeaderTemplate,
					 0	// thread id
	);

	*m_oos << str.GetBuffer();
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXL::SerializeEntryFooter
//
//	@doc:
//		Serialize entry footer
//
//---------------------------------------------------------------------------
void
CMiniDumperDXL::SerializeEntryFooter()
{
	*m_oos << CDXLSections::m_wszThreadFooter;
}

// EOF
