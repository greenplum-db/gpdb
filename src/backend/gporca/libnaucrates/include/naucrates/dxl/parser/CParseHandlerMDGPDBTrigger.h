//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBTrigger.h
//
//	@doc:
//		SAX parse handler class for GPDB trigger metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDGPDBTrigger_H
#define GPDXL_CParseHandlerMDGPDBTrigger_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"


#include "naucrates/md/CMDTriggerGPDB.h"


namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDGPDBTrigger
//
//	@doc:
//		Parse handler for GPDB trigger metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMDGPDBTrigger : public CParseHandlerMetadataObject
{
private:
	// trigger id
	IMDId *m_mdid;

	// trigger name
	CMDName *m_mdname;

	// relation id
	IMDId *m_rel_mdid;

	// function id
	IMDId *m_func_mdid;

	// trigger type
	INT m_type;

	// is trigger enabled
	BOOL m_is_enabled;

	// private copy ctor
	CParseHandlerMDGPDBTrigger(const CParseHandlerMDGPDBTrigger &);

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
	CParseHandlerMDGPDBTrigger(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDGPDBTrigger_H

// EOF
