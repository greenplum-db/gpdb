//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerManager.cpp
//
//	@doc:
//		Implementation of the controller for parse handlers.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"

using namespace gpdxl;

#define GPDXL_PARSE_CFA_FREQUENCY 50

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::CParseHandlerManager
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerManager::CParseHandlerManager(
	CDXLMemoryManager *dxl_memory_manager, SAX2XMLReader *sax_2_xml_reader)
	: m_dxl_memory_manager(dxl_memory_manager),
	  m_xml_reader(sax_2_xml_reader),
	  m_curr_parse_handler(NULL),
	  m_iteration_since_last_abortcheck(0)
{
	m_parse_handler_stack = GPOS_NEW(dxl_memory_manager->Pmp())
		ParseHandlerStack(dxl_memory_manager->Pmp());
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::~CParseHandlerManager
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerManager::~CParseHandlerManager()
{
	GPOS_DELETE(m_parse_handler_stack);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::ActivateParseHandler
//
//	@doc:
//		Activates the given parse handler and saves the current one on the stack
//
//---------------------------------------------------------------------------
void
CParseHandlerManager::ActivateParseHandler(
	CParseHandlerBase *parse_handler_base)
{
	CheckForAborts();

	if (m_curr_parse_handler)
	{
		// push current handler on stack
		m_parse_handler_stack->Push(m_curr_parse_handler);
	}

	GPOS_ASSERT(NULL != parse_handler_base);

	m_curr_parse_handler = parse_handler_base;
	m_xml_reader->setContentHandler(parse_handler_base);
	m_xml_reader->setErrorHandler(parse_handler_base);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::ReplaceHandler
//
//	@doc:
//		Activates the given parse handler and throws out the current one
//
//---------------------------------------------------------------------------
void
CParseHandlerManager::ReplaceHandler(CParseHandlerBase *parse_handler_base,
									 CParseHandlerBase *parse_handler_root)
{
	CheckForAborts();

	GPOS_ASSERT(NULL != m_curr_parse_handler);
	GPOS_ASSERT(NULL != parse_handler_base);

	if (NULL != parse_handler_root)
	{
		parse_handler_root->ReplaceParseHandler(m_curr_parse_handler,
												parse_handler_base);
	}

	m_curr_parse_handler = parse_handler_base;
	m_xml_reader->setContentHandler(parse_handler_base);
	m_xml_reader->setErrorHandler(parse_handler_base);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::DeactivateHandler
//
//	@doc:
//		Deactivates the current handler and activates the one on top of the
//		parser stack if one exists
//
//---------------------------------------------------------------------------
void
CParseHandlerManager::DeactivateHandler()
{
	CheckForAborts();

	GPOS_ASSERT(NULL != m_curr_parse_handler);

	if (!m_parse_handler_stack->IsEmpty())
	{
		m_curr_parse_handler = m_parse_handler_stack->Pop();
	}
	else
	{
		m_curr_parse_handler = NULL;
	}

	m_xml_reader->setContentHandler(m_curr_parse_handler);
	m_xml_reader->setErrorHandler(m_curr_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::GetCurrentParseHandler
//
//	@doc:
//		Returns the current handler
//
//---------------------------------------------------------------------------
const CParseHandlerBase *
CParseHandlerManager::GetCurrentParseHandler()
{
	return m_curr_parse_handler;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManager::CheckForAborts
//
//	@doc:
//		Increment CFA counter and check for aborts if necessary according to the
//		specified CFA frequency
//
//---------------------------------------------------------------------------
void
CParseHandlerManager::CheckForAborts()
{
	m_iteration_since_last_abortcheck++;

	if (GPDXL_PARSE_CFA_FREQUENCY < m_iteration_since_last_abortcheck)
	{
		GPOS_CHECK_ABORT;
		m_iteration_since_last_abortcheck = 0;
	}
}

// EOF
