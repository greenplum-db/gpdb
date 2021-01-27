//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerOptimizerConfig.h
//
//	@doc:
//		SAX parse handler class for parsing optimizer config options
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerOptimizerConfig_H
#define GPDXL_CParseHandlerOptimizerConfig_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

// fwd decl
namespace gpos
{
class CBitSet;
}

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerOptimizerConfig
//
//	@doc:
//		SAX parse handler class for parsing optimizer config options
//
//---------------------------------------------------------------------------
class CParseHandlerOptimizerConfig : public CParseHandlerBase
{
private:
	// trace flag bitset
	CBitSet *m_pbs;

	// optimizer configuration
	COptimizerConfig *m_optimizer_config;

	// private copy ctor
	CParseHandlerOptimizerConfig(const CParseHandlerOptimizerConfig &);

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
	// ctor/dtor
	CParseHandlerOptimizerConfig(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerOptimizerConfig();

	// type of the parse handler
	EDxlParseHandlerType GetParseHandlerType() const;

	// trace flags
	CBitSet *Pbs() const;

	// optimizer config
	COptimizerConfig *GetOptimizerConfig() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerOptimizerConfig_H

// EOF
