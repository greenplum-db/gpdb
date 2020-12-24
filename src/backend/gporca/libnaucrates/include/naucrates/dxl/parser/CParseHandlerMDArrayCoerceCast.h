//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerMDArrayCoerceCast.h
//
//	@doc:
//		SAX parse handler class for GPDB array coerce cast metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDArrayCoerceCast_H
#define GPDXL_CParseHandlerMDArrayCoerceCast_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"


#include "naucrates/md/CMDFunctionGPDB.h"


namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

// parse handler for GPDB array coerce cast function metadata
class CParseHandlerMDArrayCoerceCast : public CParseHandlerMetadataObject
{
private:
	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerMDArrayCoerceCast(const CParseHandlerMDArrayCoerceCast &) =
		delete;

	// ctor
	CParseHandlerMDArrayCoerceCast(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr,
								   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDArrayCoerceCast_H

// EOF
