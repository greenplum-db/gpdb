//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBFunc.h
//
//	@doc:
//		SAX parse handler class for GPDB function metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDGPDBFunc_H
#define GPDXL_CParseHandlerMDGPDBFunc_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDFunctionGPDB.h"


namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDGPDBFunc
//
//	@doc:
//		Parse handler for GPDB function metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMDGPDBFunc : public CParseHandlerMetadataObject
{
private:
	// id and version
	IMDId *m_mdid;

	// name
	CMDName *m_mdname;

	// result type
	IMDId *m_mdid_type_result;

	// output argument types
	IMdIdArray *m_mdid_types_array;

	// whether function returns a set of values
	BOOL m_returns_set;

	// function stability
	CMDFunctionGPDB::EFuncStbl m_func_stability;

	// function strictness (i.e. whether func returns NULL on NULL input)
	BOOL m_is_strict;

	BOOL m_is_ndv_preserving;

	BOOL m_is_allowed_for_PS;

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

	// parse function stability property from XML string
	static CMDFunctionGPDB::EFuncStbl ParseFuncStability(const XMLCh *xml_val);

public:
	CParseHandlerMDGPDBFunc(const CParseHandlerMDGPDBFunc &) = delete;

	// ctor
	CParseHandlerMDGPDBFunc(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDGPDBFunc_H

// EOF
