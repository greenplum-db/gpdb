//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CCostModelParamsGPDBLegacy.h
//
//	@doc:
//		Parameters in GPDB's legacy cost model
//---------------------------------------------------------------------------
#ifndef GPDBCOST_CCostModelParamsGPDBLegacy_H
#define GPDBCOST_CCostModelParamsGPDBLegacy_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/common/CRefCount.h"
#include "gpos/string/CWStringConst.h"

#include "gpopt/cost/ICostModelParams.h"



namespace gpopt
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		CCostModelParamsGPDBLegacy
//
//	@doc:
//		Parameters in GPDB's legacy cost model
//
//---------------------------------------------------------------------------
class CCostModelParamsGPDBLegacy : public ICostModelParams
{
public:
	// enumeration of cost model params
	enum ECostParam
	{
		EcpSeqIOBandwidth = 0,	// sequential i/o bandwidth
		EcpRandomIOBandwidth,	// random i/o bandwidth
		EcpTupProcBandwidth,	// tuple processing bandwidth
		EcpTupUpdateBandwith,	// tuple update bandwidth
		EcpNetBandwidth,		// network bandwidth
		EcpSegments,			// number of segments
		EcpNLJOuterFactor,		// factor for nested loop outer child
		EcpNLJFactor,			// factor for nested loop
		EcpHJFactor,			// hash join factor - to represent spilling cost
		EcpHashFactor,			// hash building factor
		EcpDefaultCost,			// default cost

		EcpSentinel
	};

private:
	// memory pool
	CMemoryPool *m_mp;

	// array of parameters
	// cost param enum is used as index in this array
	SCostParam *m_rgpcp[EcpSentinel];

	// default value of sequential i/o bandwidth
	static const CDouble DSeqIOBandwidthVal;

	// default value of random i/o bandwidth
	static const CDouble DRandomIOBandwidthVal;

	// default value of tuple processing bandwidth
	static const CDouble DTupProcBandwidthVal;

	// default value of tuple update bandwidth
	static const CDouble DTupUpdateBandwidthVal;

	// default value of network bandwidth
	static const CDouble DNetBandwidthVal;

	// default value of number of segments
	static const CDouble DSegmentsVal;

	// default value of nested loop outer child factor
	static const CDouble DNLJOuterFactorVal;

	// default value of nested loop factor
	static const CDouble DNLJFactorVal;

	// default value of hash join factor
	static const CDouble DHJFactorVal;

	// default value of hash building factor
	static const CDouble DHashFactorVal;

	// default cost value when one is not computed
	static const CDouble DDefaultCostVal;

public:
	CCostModelParamsGPDBLegacy(CCostModelParamsGPDBLegacy &) = delete;

	// ctor
	explicit CCostModelParamsGPDBLegacy(CMemoryPool *mp);

	// dtor
	~CCostModelParamsGPDBLegacy() override;

	// lookup param by id
	SCostParam *PcpLookup(ULONG id) const override;

	// lookup param by name
	SCostParam *PcpLookup(const CHAR *szName) const override;

	// set param by id
	void SetParam(ULONG id, CDouble dVal, CDouble dLowerBound,
				  CDouble dUpperBound) override;

	// set param by name
	void SetParam(const CHAR *szName, CDouble dVal, CDouble dLowerBound,
				  CDouble dUpperBound) override;

	// print function
	IOstream &OsPrint(IOstream &os) const override;

	BOOL Equals(ICostModelParams *pcm) const override;

	const CHAR *SzNameLookup(ULONG id) const override;

};	// class CCostModelParamsGPDBLegacy

}  // namespace gpopt

#endif	// !GPDBCOST_CCostModelParamsGPDBLegacy_H

// EOF
