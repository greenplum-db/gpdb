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
		typedef CDynamicPtrArray<CNode, CleanupRelease<CNode> > CNodeArray;

		// struct for resulting trees
		class CNode : public CRefCount
		{
			private: 
			
				// element number
				ULONG m_ulData;
				
				// children
				CNodeArray *m_pdrgpnd;

				// private copy ctor
				CNode(const CNode &);

			public:
				
				// ctor
				CNode(CMemoryPool *mp, ULONG *pulData, CNodeArray *pdrgpnd);
				
				// dtor
				~CNode();
				
				// debug print
				IOstream &OsPrintWithIndent(IOstream &os, ULONG ulIndent = 0) const;
		};

				
		private:

			// counter used to mark last successful test
			static
			ULONG m_ulTestCounter;

			// factory function for result object
			static
			CNode *Pnd(CMemoryPool *mp, ULONG *pul, CNodeArray *pdrgpnd, BOOL *fTestTrue);
		
			// shorthand for tests
			typedef CTreeMap<ULONG, CNode, BOOL, HashValue<ULONG>, Equals<ULONG> > TestMap;

			// helper to generate loaded the tree map
			static 
			TestMap *PtmapLoad(CMemoryPool *mp);

		public:

			// unittests
			static GPOS_RESULT EresUnittest();
			static GPOS_RESULT EresUnittest_Basic();
			static GPOS_RESULT EresUnittest_Count();
			static GPOS_RESULT EresUnittest_Unrank();
			static GPOS_RESULT EresUnittest_Memo();
			
#ifndef GPOS_DEBUG
			// this test is run in optimized build because of long optimization time
			static GPOS_RESULT EresUnittest_FailedPlanEnumerationTests();
#endif // GPOS_DEBUG



#ifdef GPOS_DEBUG
			static GPOS_RESULT EresUnittest_Cycle();
#endif // GPOS_DEBUG

	}; // CTreeMapTest

}

#endif // !GPOPT_CTreeMapTest_H


// EOF
