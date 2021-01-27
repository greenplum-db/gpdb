//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerEnumeratorConfig.h
//
//	@doc:
//		SAX parse handler class for parsing enumerator configuration options
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerEnumeratorConfig_H
#define GPDXL_CParseHandlerEnumeratorConfig_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerEnumeratorConfig
//
//	@doc:
//		SAX parse handler class for parsing enumerator configuration options
//
//---------------------------------------------------------------------------
class CParseHandlerEnumeratorConfig : public CParseHandlerBase
{
private:
	// enumerator configuration
	CEnumeratorConfig *m_enumerator_cfg;

	// private copy ctor
	CParseHandlerEnumeratorConfig(const CParseHandlerEnumeratorConfig &);

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
	CParseHandlerEnumeratorConfig(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerEnumeratorConfig();

	// type of the parse handler
	EDxlParseHandlerType GetParseHandlerType() const;

	// enumerator configuration
	CEnumeratorConfig *GetEnumeratorCfg() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerEnumeratorConfig_H

// EOF
