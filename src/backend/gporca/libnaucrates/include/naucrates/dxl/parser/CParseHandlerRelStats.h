//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerRelStats.h
//
//	@doc:
//		SAX parse handler class for parsing base relation stats objects
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerRelStats_H
#define GPDXL_CParseHandlerRelStats_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerRelStats
//
//	@doc:
//		Base parse handler class for base relation stats
//
//---------------------------------------------------------------------------
class CParseHandlerRelStats : public CParseHandlerMetadataObject
{
private:
	// private copy ctor
	CParseHandlerRelStats(const CParseHandlerRelStats &);

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
	CParseHandlerRelStats(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerRelStats_H

// EOF
