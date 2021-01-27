//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CParseHandlerCtasStorageOptions.h
//
//	@doc:
//		Parse handler for parsing CTAS storage options
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerCTASStorageOptions_H
#define GPDXL_CParseHandlerCTASStorageOptions_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerCtasStorageOptions
//
//	@doc:
//		Parse handler for parsing CTAS storage options
//
//---------------------------------------------------------------------------
class CParseHandlerCtasStorageOptions : public CParseHandlerBase
{
private:
	// tablespace name
	CMDName *m_mdname_tablespace;

	// on commit action
	CDXLCtasStorageOptions::ECtasOnCommitAction m_ctas_on_commit_action;

	// CTAS storage options
	CDXLCtasStorageOptions *m_dxl_ctas_storage_option;

	// parsed array of key-value pairs of options
	CDXLCtasStorageOptions::CDXLCtasOptionArray *m_ctas_storage_option_array;

	// private copy ctor
	CParseHandlerCtasStorageOptions(const CParseHandlerCtasStorageOptions &);

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
	CParseHandlerCtasStorageOptions(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerCtasStorageOptions();

	// parsed storage options
	CDXLCtasStorageOptions *GetDxlCtasStorageOption() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerCTASStorageOptions_H

// EOF
