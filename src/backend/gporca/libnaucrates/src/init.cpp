//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		init.cpp
//
//	@doc:
//		Implementation of initialization and termination functions for
//		libgpdxl.
//---------------------------------------------------------------------------

#include "naucrates/init.h"

#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/util/XMLString.hpp>

#include "gpos/memory/CAutoMemoryPool.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/exception.h"

using namespace gpos;
using namespace gpdxl;

static CDXLMemoryManager *dxl_memory_manager = NULL;

static CMemoryPool *pmpXerces = NULL;

static CMemoryPool *pmpDXL = NULL;

// safe-guard to prevent initializing DXL support more than once
static ULONG_PTR m_ulpInitDXL = 0;

// safe-guard to prevent shutting DXL support down more than once
static ULONG_PTR m_ulpShutdownDXL = 0;


//---------------------------------------------------------------------------
//      @function:
//              InitDXL
//
//      @doc:
//				Initialize DXL support; must be called before any library
//				function is called
//
//
//---------------------------------------------------------------------------
void
InitDXL()
{
	if (0 < m_ulpInitDXL)
	{
		// DXL support is already initialized by a previous call
		return;
	}

	GPOS_ASSERT(NULL != pmpXerces);
	GPOS_ASSERT(NULL != pmpDXL);

	m_ulpInitDXL++;

	// setup own memory manager
	dxl_memory_manager = GPOS_NEW(pmpXerces) CDXLMemoryManager(pmpXerces);

	// initialize Xerces, if this fails library initialization should crash here
	XMLPlatformUtils::Initialize(XMLUni::fgXercescDefaultLocale,  // locale
								 NULL,	// nlsHome: location for message files
								 NULL,	// panicHandler
								 dxl_memory_manager	 // memoryManager
	);

	// initialize DXL tokens
	CDXLTokens::Init(pmpDXL);

	// initialize parse handler mappings
	CParseHandlerFactory::Init(pmpDXL);
}


//---------------------------------------------------------------------------
//      @function:
//              ShutdownDXL
//
//      @doc:
//				Shutdown DXL support; called only at library termination
//
//---------------------------------------------------------------------------
void
ShutdownDXL()
{
	if (0 < m_ulpShutdownDXL)
	{
		// DXL support is already shut-down by a previous call
		return;
	}

	GPOS_ASSERT(NULL != pmpXerces);

	m_ulpShutdownDXL++;

	XMLPlatformUtils::Terminate();

	CDXLTokens::Terminate();

	GPOS_DELETE(dxl_memory_manager);
	dxl_memory_manager = NULL;
}


//---------------------------------------------------------------------------
//      @function:
//              gpdxl_init
//
//      @doc:
//              Initialize Xerces parser utils
//
//---------------------------------------------------------------------------
void
gpdxl_init()
{
	// create memory pool for Xerces global allocations
	{
		CAutoMemoryPool amp;

		// detach safety
		pmpXerces = amp.Detach();
	}

	// create memory pool for DXL global allocations
	{
		CAutoMemoryPool amp;

		// detach safety
		pmpDXL = amp.Detach();
	}

	// add standard exception messages
	(void) EresExceptionInit(pmpDXL);
}


//---------------------------------------------------------------------------
//      @function:
//              gpdxl_terminate
//
//      @doc:
//              Terminate Xerces parser utils and destroy memory pool
//
//---------------------------------------------------------------------------
void
gpdxl_terminate()
{
#ifdef GPOS_DEBUG
	ShutdownDXL();

	if (NULL != pmpDXL)
	{
		(CMemoryPoolManager::GetMemoryPoolMgr())->Destroy(pmpDXL);
		pmpDXL = NULL;
	}

	if (NULL != pmpXerces)
	{
		(CMemoryPoolManager::GetMemoryPoolMgr())->Destroy(pmpXerces);
		pmpXerces = NULL;
	}
#endif	// GPOS_DEBUG
}

// EOF
