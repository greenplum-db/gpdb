//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColumnDescriptorTest.cpp
//
//	@doc:
//		Test for CColumnDescriptor
//---------------------------------------------------------------------------


#include "unittest/gpopt/metadata/CColumnDescriptorTest.h"

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/IMDTypeInt4.h"

#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CColumnDescriptorTest::EresUnittest
//
//	@doc:
//		Unittest column descriptors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColumnDescriptorTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CColumnDescriptorTest::EresUnittest_Basic)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CColumnDescriptorTest::EresUnittest_Basic
//
//	@doc:
//		basic naming
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColumnDescriptorTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();

	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	const IMDTypeInt4 *pmdtypeint4 =
		mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);

	CWStringConst strName(GPOS_WSZ_LIT("column desc test"));
	CName name(&strName);
	CColumnDescriptor *pcdesc = GPOS_NEW(mp) CColumnDescriptor(
		mp, pmdtypeint4, default_type_modifier, name, 1, false /*IsNullable*/);

	GPOS_ASSERT(name.Equals(pcdesc->Name()));

	GPOS_ASSERT(1 == pcdesc->AttrNum());

	pcdesc->Release();

	return GPOS_OK;
}


// EOF
