//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CXMLSerializerTest.cpp
//
//	@doc:
//		Tests serializing XML.
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/io/COstreamString.h"
#include "gpos/common/CRandom.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "unittest/dxl/CXMLSerializerTest.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializerTest::EresUnittest
//
//	@doc:
//		Unittest for serializing XML
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXMLSerializerTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CXMLSerializerTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CXMLSerializerTest::EresUnittest_Base64)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializerTest::GetMDName
//
//	@doc:
//		Generate an XML string with or without identation using the XML serializer
//
//---------------------------------------------------------------------------
CWStringDynamic *
CXMLSerializerTest::Pstr(CMemoryPool *mp, BOOL indentation)
{
	CWStringDynamic *str = GPOS_NEW(mp) CWStringDynamic(mp);

	// create a string stream to hold the result of serialization
	COstreamString oss(str);

	CXMLSerializer xml_serializer(mp, oss, indentation);

	xml_serializer.StartDocument();

	xml_serializer.OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenPlan));

	CWStringConst strSubplan(GPOS_WSZ_LIT("Subplan"));
	xml_serializer.OpenElement(NULL, &strSubplan);
	xml_serializer.CloseElement(NULL, &strSubplan);

	xml_serializer.CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenPlan));

	return str;
}
//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializerTest::EresUnittest_Basic
//
//	@doc:
//		Testing XML serialization
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXMLSerializerTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// test XML serializer with indentation
	CWStringDynamic *pstrIndented = Pstr(mp, true /* indentation */);

	CWStringConst strExpectedIndented(GPOS_WSZ_LIT(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<dxl:Plan>\n  <Subplan/>\n</dxl:Plan>\n"));

	GPOS_RESULT eresIndented = GPOS_FAILED;

	if (pstrIndented->Equals(&strExpectedIndented))
	{
		eresIndented = GPOS_OK;
	}

	// test XML serializer without indentation
	CWStringDynamic *pstrNotIndented = Pstr(mp, false /* indentation */);

	CWStringConst strExpectedNotIndented(GPOS_WSZ_LIT(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?><dxl:Plan><Subplan/></dxl:Plan>"));

	GPOS_RESULT eresNotIndented = GPOS_FAILED;

	if (pstrNotIndented->Equals(&strExpectedNotIndented))
	{
		eresNotIndented = GPOS_OK;
	}

	GPOS_DELETE(pstrIndented);
	GPOS_DELETE(pstrNotIndented);

	if (GPOS_FAILED == eresIndented || GPOS_FAILED == eresNotIndented)
	{
		return GPOS_FAILED;
	}
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializerTest::EresUnittest_Base64
//
//	@doc:
//		Testing base64 encoding and decoding
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXMLSerializerTest::EresUnittest_Base64()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	const ULONG ulraSize = 5;
	ULONG rgulRandArr[ulraSize];

	CRandom cr;
	for (ULONG i = 0; i < ulraSize; i++)
	{
		rgulRandArr[i] = cr.Next();
	}

	CWStringDynamic *str = CDXLUtils::EncodeByteArrayToString(
		mp, (BYTE *) rgulRandArr, sizeof(rgulRandArr));

	ULONG len;

	ULONG *pulRandArrCopy =
		(ULONG *) CDXLUtils::DecodeByteArrayFromString(mp, str, &len);

	GPOS_ASSERT(len == sizeof(rgulRandArr));

	for (ULONG i = 0; i < ulraSize; i++)
	{
		if (rgulRandArr[i] != pulRandArrCopy[i])
		{
			return GPOS_FAILED;
		}
	}

	GPOS_DELETE(str);
	GPOS_DELETE_ARRAY(pulRandArrCopy);

	INT i = 1000;
	str = CDXLUtils::EncodeByteArrayToString(mp, (BYTE *) &i, sizeof(i));

	gpos::oswcout << "Base64 encoding of " << i << " is " << str->GetBuffer()
				  << std::endl;

	GPOS_DELETE(str);

	return GPOS_OK;
}

// EOF
