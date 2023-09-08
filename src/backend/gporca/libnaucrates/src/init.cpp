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

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/exception.h"

using namespace gpos;
using namespace gpdxl;

static CDXLMemoryManager *dxl_memory_manager = nullptr;

static CMemoryPool *pmpXerces = nullptr;

static CMemoryPool *pmpDXL = nullptr;

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

	GPOS_ASSERT(nullptr != pmpXerces);
	GPOS_ASSERT(nullptr != pmpDXL);

	// setup own memory manager
	dxl_memory_manager = GPOS_NEW(pmpXerces) CDXLMemoryManager(pmpXerces);

	// initialize Xerces, if this fails library initialization should crash here
	XMLPlatformUtils::Initialize(
		XMLUni::fgXercescDefaultLocale,	 // locale
		nullptr,						 // nlsHome: location for message files
		nullptr,						 // panicHandler
		dxl_memory_manager				 // memoryManager
	);

	// initialize DXL tokens
	CDXLTokens::Init(pmpDXL);

	// initialize parse handler mappings
	CParseHandlerFactory::Init(pmpDXL);

	m_ulpInitDXL++;
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

	GPOS_ASSERT(nullptr != pmpXerces);

	m_ulpShutdownDXL++;

	XMLPlatformUtils::Terminate();

	CDXLTokens::Terminate();

	GPOS_DELETE(dxl_memory_manager);
	dxl_memory_manager = nullptr;
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
	pmpXerces = CMemoryPoolManager::CreateMemoryPool();

	// create memory pool for DXL global allocations
	pmpDXL = CMemoryPoolManager::CreateMemoryPool();

	// add standard exception messages
	EresExceptionInit(pmpDXL);
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

	if (nullptr != pmpDXL)
	{
		CMemoryPoolManager::Destroy(pmpDXL);
		pmpDXL = nullptr;
	}

	if (nullptr != pmpXerces)
	{
		CMemoryPoolManager::Destroy(pmpXerces);
		pmpXerces = nullptr;
	}
#endif	// GPOS_DEBUG
}

// EOF
