//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTreeMapTest.h
//
//	@doc:
//		Test for tree map implementation
//---------------------------------------------------------------------------
#ifndef GPOPT_CTreeMapTest_H
#define GPOPT_CTreeMapTest_H

#include "gpos/base.h"

#include "gpopt/search/CTreeMap.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CTreeMapTest
//
//	@doc:
//		unittest for tree map
//
//---------------------------------------------------------------------------
class CTreeMapTest
{
	// fwd declaration
	class CNode;
	using CNodeArray = CDynamicPtrArray<CNode, CleanupRelease<CNode>>;

	// struct for resulting trees
	class CNode : public CRefCount
	{
	private:
		// element number
		ULONG m_ulData;

		// children
		CNodeArray *m_pdrgpnd;

	public:
		CNode(const CNode &) = delete;

		// ctor
		CNode(CMemoryPool *mp, const ULONG *pulData, CNodeArray *pdrgpnd);

		// dtor
		~CNode() override;

		// debug print
		IOstream &OsPrintWithIndent(IOstream &os, ULONG ulIndent = 0) const;
	};


private:
	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

	// factory function for result object
	static CNode *Pnd(CMemoryPool *mp, ULONG *pul, CNodeArray *pdrgpnd,
					  BOOL *fTestTrue);

	// shorthand for tests
	using TestMap =
		CTreeMap<ULONG, CNode, BOOL, HashValue<ULONG>, Equals<ULONG>>;

	// helper to generate loaded the tree map
	static TestMap *PtmapLoad(CMemoryPool *mp);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Count();
	static GPOS_RESULT EresUnittest_Unrank();
	static GPOS_RESULT EresUnittest_Memo();


#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_Cycle();
#endif	// GPOS_DEBUG

};	// CTreeMapTest

}  // namespace gpopt

#endif	// !GPOPT_CTreeMapTest_H


// EOF
