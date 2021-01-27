//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBCheckConstraint.h
//
//	@doc:
//		SAX parse handler class for parsing an MD check constraint
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDGPDBCheckConstraint_H
#define GPDXL_CParseHandlerMDGPDBCheckConstraint_H

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
//		CParseHandlerMDGPDBCheckConstraint
//
//	@doc:
//		Parse handler class for parsing an MD check constraint
//
//---------------------------------------------------------------------------
class CParseHandlerMDGPDBCheckConstraint : public CParseHandlerMetadataObject
{
private:
	// mdid of the check constraint
	IMDId *m_mdid;

	// name of the check constraint
	CMDName *m_mdname;

	// mdid of the relation
	IMDId *m_rel_mdid;

	// private copy ctor
	CParseHandlerMDGPDBCheckConstraint(
		const CParseHandlerMDGPDBCheckConstraint &);

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
	CParseHandlerMDGPDBCheckConstraint(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDGPDBCheckConstraint_H

// EOF
