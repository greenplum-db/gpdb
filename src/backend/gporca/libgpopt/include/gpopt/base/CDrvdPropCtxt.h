//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CDrvdPropCtxt.h
//
//	@doc:
//		Base class for derived properties context;
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CDrvdPropCtxt_H
#define GPOPT_CDrvdPropCtxt_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"


namespace gpopt
{
using namespace gpos;

// fwd declarations
class CDrvdPropCtxt;
class CDrvdProp;

// dynamic array for properties
using CDrvdPropCtxtArray = CDynamicPtrArray<CDrvdPropCtxt, CleanupRelease>;

//---------------------------------------------------------------------------
//	@class:
//		CDrvdPropCtxt
//
//	@doc:
//		Container of information passed among expression nodes during
//		property derivation
//
//---------------------------------------------------------------------------
class CDrvdPropCtxt : public CRefCount
{
private:
protected:
	// memory pool
	CMemoryPool *m_mp;

	// copy function
	virtual CDrvdPropCtxt *PdpctxtCopy(CMemoryPool *mp) const = 0;

	// add props to context
	virtual void AddProps(CDrvdProp *pdp) = 0;

public:
	CDrvdPropCtxt(const CDrvdPropCtxt &) = delete;

	// ctor
	CDrvdPropCtxt(CMemoryPool *mp) : m_mp(mp)
	{
	}

	// dtor
	~CDrvdPropCtxt() override = default;

#ifdef GPOS_DEBUG

	// is it a relational property context?
	virtual BOOL
	FRelational() const
	{
		return false;
	}

	// is it a plan property context?
	virtual BOOL
	FPlan() const
	{
		return false;
	}

	// is it a scalar property context?
	virtual BOOL
	FScalar() const
	{
		return false;
	}

#endif	// GPOS_DEBUG

	// copy function
	static CDrvdPropCtxt *
	PdpctxtCopy(CMemoryPool *mp, CDrvdPropCtxt *pdpctxt)
	{
		if (nullptr == pdpctxt)
		{
			return nullptr;
		}

		return pdpctxt->PdpctxtCopy(mp);
	}

	// add derived props to context
	static void
	AddDerivedProps(CDrvdProp *pdp, CDrvdPropCtxt *pdpctxt)
	{
		if (nullptr != pdpctxt)
		{
			pdpctxt->AddProps(pdp);
		}
	}

};	// class CDrvdPropCtxt

}  // namespace gpopt


#endif	// !GPOPT_CDrvdPropCtxt_H

// EOF
