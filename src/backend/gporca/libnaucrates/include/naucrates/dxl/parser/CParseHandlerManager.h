//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerManager.h
//
//	@doc:
//		Class for managing the creation and installation of parse handlers.
//		The class serves as a factory for creating parse handler objects, and
//		for switching between parse handlers during parsing.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerManager_H
#define GPDXL_CParseHandlerManager_H

#include "gpos/base.h"
#include "gpos/common/CStack.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

#include <xercesc/sax2/SAX2XMLReader.hpp>

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CParseHandlerPhysicalOp;
class CDXLMemoryManager;

// stack of parse handlers
typedef CStack<CParseHandlerBase> ParseHandlerStack;


//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerManager
//
//	@doc:
//		Manages the creation, activation and deactivation of parse handlers
//
//
//---------------------------------------------------------------------------
class CParseHandlerManager
{
private:
	// the memory manager used for parsing the current document
	CDXLMemoryManager *m_dxl_memory_manager;

	// parser object responsible for parsing the current XML document
	SAX2XMLReader *m_xml_reader;

	// current parse handler
	CParseHandlerBase *m_curr_parse_handler;

	// stack of parse handlers
	ParseHandlerStack *m_parse_handler_stack;

	// steps since last check for aborts
	ULONG m_iteration_since_last_abortcheck;

	// check for aborts at regular intervals
	void CheckForAborts();

	// private copy ctor
	CParseHandlerManager(const CParseHandlerManager &);


public:
	// ctor/dtor
	CParseHandlerManager(CDXLMemoryManager *, SAX2XMLReader *);
	~CParseHandlerManager();


	// returns the current memory manager
	CDXLMemoryManager *
	GetDXLMemoryManager()
	{
		return m_dxl_memory_manager;
	}

	// activation and deactivation of parsers

	// Gives parsing control to the specified handler. When the handler
	// exits (by calling Deactivate), control will be returned to the handler,
	// which was active before the function was invoked.
	void ActivateParseHandler(CParseHandlerBase *);

	// Replaces current parse handler with the specified one. When the new
	// handler exits, control goes to the handler active before the one replaced
	// with this function call.
	void ReplaceHandler(CParseHandlerBase *parse_handler_new,
						CParseHandlerBase *parse_handler_root);

	// Deactivates current handler and returns control to the previously active one.
	void DeactivateHandler();

	// Returns the current parse handler if one exists; used for debugging purposes
	const CParseHandlerBase *GetCurrentParseHandler();
};
}  // namespace gpdxl
#endif	// !GPDXL_CParseHandlerManager_H

// EOF
