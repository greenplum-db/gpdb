//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC CORP.
//
//	@filename:
//		CDrvdPropCtxtScalar.h
//
//	@doc:
//		Container of information passed among expression nodes during
//		derivation of scalar properties
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CDrvdPropCtxtScalar_H
#define GPOPT_CDrvdPropCtxtScalar_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdPropCtxt.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDrvdPropCtxtScalar
//
//	@doc:
//		Container of information passed among expression nodes during
//		derivation of plan properties
//
//---------------------------------------------------------------------------
class CDrvdPropCtxtScalar : public CDrvdPropCtxt
{
private:
	// private copy ctor
	CDrvdPropCtxtScalar(const CDrvdPropCtxtScalar &);

protected:
	// copy function
	virtual CDrvdPropCtxt *
	PdpctxtCopy(CMemoryPool *mp) const
	{
		return GPOS_NEW(mp) CDrvdPropCtxtScalar(mp);
	}

	// add props to context
	virtual void
	AddProps(CDrvdProp *  // pdp
	)
	{
		// derived scalar context is currently empty
	}

public:
	// ctor
	CDrvdPropCtxtScalar(CMemoryPool *mp) : CDrvdPropCtxt(mp)
	{
	}

	// dtor
	virtual ~CDrvdPropCtxtScalar()
	{
	}

	// print
	virtual IOstream &
	OsPrint(IOstream &os) const
	{
		return os;
	}

#ifdef GPOS_DEBUG

	// is it a scalar property context?
	virtual BOOL
	FScalar() const
	{
		return true;
	}

#endif	// GPOS_DEBUG

	// conversion function
	static CDrvdPropCtxtScalar *
	PdpctxtscalarConvert(CDrvdPropCtxt *pdpctxt)
	{
		GPOS_ASSERT(NULL != pdpctxt);

		return reinterpret_cast<CDrvdPropCtxtScalar *>(pdpctxt);
	}


};	// class CDrvdPropCtxtScalar

}  // namespace gpopt


#endif	// !GPOPT_CDrvdPropCtxtScalar_H

// EOF
