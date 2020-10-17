//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CXMLSerializerTest.h
//
//	@doc:
//		Tests the XML serializer
//---------------------------------------------------------------------------


#ifndef GPOPT_CXMLSerializerTest_H
#define GPOPT_CXMLSerializerTest_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"

// fwd decl
namespace gpos
{
class CWStringDynamic;
}

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXMLSerializerTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------

class CXMLSerializerTest
{
private:
	// helper function for using the XML serializer to generate a document
	// with or without indentation
	static CWStringDynamic *Pstr(CMemoryPool *mp, BOOL indentation);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_NoIndent();
	static GPOS_RESULT EresUnittest_Base64();

};	// class CXMLSerializerTest
}  // namespace gpdxl

#endif	// !GPOPT_CXMLSerializerTest_H

// EOF
