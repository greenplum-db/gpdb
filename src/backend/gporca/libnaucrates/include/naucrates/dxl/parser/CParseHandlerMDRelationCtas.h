//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerMDRelationCtas.h
//
//	@doc:
//		SAX parse handler class for CTAS relation metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDRelationCTAS_H
#define GPDXL_CParseHandlerMDRelationCTAS_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMDRelation.h"

#include "naucrates/md/CMDRelationExternalGPDB.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDRelationCtas
//
//	@doc:
//		Parse handler for CTAS relation metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMDRelationCtas : public CParseHandlerMDRelation
{
private:
	// vartypemod list
	IntPtrArray *m_vartypemod_array;

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
	CParseHandlerMDRelationCtas(const CParseHandlerMDRelationCtas &) = delete;

	// ctor
	CParseHandlerMDRelationCtas(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDRelationCTAS_H

// EOF
