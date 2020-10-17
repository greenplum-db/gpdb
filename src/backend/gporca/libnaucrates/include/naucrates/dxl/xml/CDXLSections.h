//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLSections.h
//
//	@doc:
//		Section headers in DXL documents
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLSections_H
#define GPDXL_CDXLSections_H


#include "gpos/base.h"

using namespace gpos;

namespace gpdxl
{
class CDXLSections
{
public:
	// DXL document header and footer
	static const WCHAR *m_wszDocumentHeader;
	static const WCHAR *m_wszDocumentFooter;

	// Thread header and footer
	static const WCHAR *m_wszThreadHeaderTemplate;
	static const WCHAR *m_wszThreadFooter;

	// Optimizer config header and footer
	static const WCHAR *m_wszOptimizerConfigHeader;
	static const WCHAR *m_wszOptimizerConfigFooter;

	// Metadata section header and footer
	static const WCHAR *m_wszMetadataHeaderPrefix;
	static const WCHAR *m_wszMetadataHeaderSuffix;
	static const WCHAR *m_wszMetadataFooter;

	// trace flags section prefix
	static const WCHAR *m_wszTraceFlagsSectionPrefix;

	// trace flags section suffix
	static const WCHAR *m_wszTraceFlagsSectionSuffix;

	// stack trace section header
	static const WCHAR *m_wszStackTraceHeader;

	// stack trace section footer
	static const WCHAR *m_wszStackTraceFooter;
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLSections_H

// EOF
