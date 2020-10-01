//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadataIdList.h
//
//	@doc:
//		SAX parse handler class for parsing a list of metadata identifiers
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMetadataIdList_H
#define GPDXL_CParseHandlerMetadataIdList_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMetadataIdList
//
//	@doc:
//		SAX parse handler class for parsing a list of metadata identifiers
//
//---------------------------------------------------------------------------
class CParseHandlerMetadataIdList : public CParseHandlerBase
{
private:
	// list of metadata identifiers
	IMdIdArray *m_mdid_array;


	// private copy ctor
	CParseHandlerMetadataIdList(const CParseHandlerMetadataIdList &);

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

	// is this a supported element of a metadata list
	BOOL FSupportedElem(const XMLCh *const xml_str);

	// is this a supported metadata list type
	BOOL FSupportedListType(const XMLCh *const xml_str);

public:
	// ctor/dtor
	CParseHandlerMetadataIdList(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerMetadataIdList();

	// return the constructed list of metadata identifiers
	IMdIdArray *GetMdIdArray();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMetadataIdList_H

// EOF
