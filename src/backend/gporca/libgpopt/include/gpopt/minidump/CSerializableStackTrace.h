//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSerializableStackTrace.h
//
//	@doc:
//		Serializable stack trace object
//---------------------------------------------------------------------------
#ifndef GPOPT_CSerializableStackTrace_H
#define GPOPT_CSerializableStackTrace_H

#include "gpos/base.h"
#include "gpos/error/CSerializable.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLNode.h"

using namespace gpos;
using namespace gpdxl;


namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CSerializableStackTrace
//
//	@doc:
//		Serializable stacktrace object
//
//---------------------------------------------------------------------------
class CSerializableStackTrace : public CSerializable
{
public:
	// ctor
	CSerializableStackTrace();

	// dtor
	virtual ~CSerializableStackTrace();

	// serialize object to passed stream
	virtual void Serialize(COstream &oos);

};	// class CSerializableStackTrace
}  // namespace gpopt

#endif	// !GPOPT_CSerializableStackTrace_H

// EOF
