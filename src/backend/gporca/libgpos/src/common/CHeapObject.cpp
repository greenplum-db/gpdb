//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CHeapObject.cpp
//
//	@doc:
//		Implementation of class of all objects that must reside on the heap;
//		There used to be an assertion for that here, but it was too fragile.
//---------------------------------------------------------------------------

#include "gpos/common/CHeapObject.h"

#include "gpos/utils.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CHeapObject::CHeapObject
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CHeapObject::CHeapObject()
{
}


// EOF
