//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC CORP.
//
//	@filename:
//		CDrvdPropCtxtRelational.h
//
//	@doc:
//		Container of information passed among expression nodes during
//		derivation of relational properties
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CDrvdPropCtxtRelational_H
#define GPOPT_CDrvdPropCtxtRelational_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdPropCtxt.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDrvdPropCtxtRelational
//
//	@doc:
//		Container of information passed among expression nodes during
//		derivation of relational properties
//
//---------------------------------------------------------------------------
class CDrvdPropCtxtRelational : public CDrvdPropCtxt
{
private:
protected:
	// copy function
	CDrvdPropCtxt *
	PdpctxtCopy(CMemoryPool *mp) const override
	{
		return GPOS_NEW(mp) CDrvdPropCtxtRelational(mp);
	}

	// add props to context
	void
	AddProps(CDrvdProp *  // pdp
			 ) override
	{
		// derived relational context is currently empty
	}

public:
	CDrvdPropCtxtRelational(const CDrvdPropCtxtRelational &) = delete;

	// ctor
	CDrvdPropCtxtRelational(CMemoryPool *mp) : CDrvdPropCtxt(mp)
	{
	}

	// dtor
	~CDrvdPropCtxtRelational() override = default;


#ifdef GPOS_DEBUG

	// is it a relational property context?
	BOOL
	FRelational() const override
	{
		return true;
	}

#endif	// GPOS_DEBUG

	// conversion function
	static CDrvdPropCtxtRelational *
	PdpctxtrelConvert(CDrvdPropCtxt *pdpctxt)
	{
		GPOS_ASSERT(nullptr != pdpctxt);

		return dynamic_cast<CDrvdPropCtxtRelational *>(pdpctxt);
	}

};	// class CDrvdPropCtxtRelational

}  // namespace gpopt


#endif	// !GPOPT_CDrvdPropCtxtRelational_H

// EOF
