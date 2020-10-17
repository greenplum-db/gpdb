//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerColStats.h
//
//	@doc:
//		SAX parse handler class for parsing base relation column stats objects
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerColStats_H
#define GPDXL_CParseHandlerColStats_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"

// fwd decl
namespace gpmd
{
class CMDIdColStats;
}

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerColStats
//
//	@doc:
//		Parse handler class for base relation column stats
//
//---------------------------------------------------------------------------
class CParseHandlerColStats : public CParseHandlerMetadataObject
{
private:
	// mdid of the col stats object
	CMDIdColStats *m_mdid;

	// name of the column
	CMDName *m_md_name;

	// column width
	CDouble m_width;

	// null fraction
	CDouble m_null_freq;

	// ndistinct of remaining tuples
	CDouble m_distinct_remaining;

	// frequency of remaining tuples
	CDouble m_freq_remaining;

	// is the column statistics missing in the database
	BOOL m_is_column_stats_missing;

	// private copy ctor
	CParseHandlerColStats(const CParseHandlerColStats &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerColStats(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_base);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerColStats_H

// EOF
