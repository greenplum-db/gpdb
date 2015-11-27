//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		COptServer.h
//
//	@doc:
//		API for optimizer server
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef COptServer_H
#define COptServer_H

#include "naucrates/md/CSystemId.h"

#include "gpos/base.h"
#include "gpos/common/CSyncHashtable.h"
#include "gpos/common/CSyncHashtableAccessByKey.h"
#include "gpos/common/CSyncHashtableAccessByIter.h"
#include "gpos/common/CSyncHashtableIter.h"
#include "gpos/net/CSocket.h"
#include "gpos/sync/CSpinlock.h"
#include "gpos/task/CTask.h"

// forward declarations
namespace gpopt
{
	class CExpression;
	class CMDAccessor;
	class CMiniDumperDXL;
	class CQueryContext;
}

namespace gpnaucrates
{
	class CCommunicator;
}


namespace gpoptudfs
{
	using namespace gpos;
	using namespace gpopt;
	using namespace gpnaucrates;

	//---------------------------------------------------------------------------
	//	@class:
	//		COptServer
	//
	//	@doc:
	//		Optimizer server; processes optimization requests from QDs;
	//
	//---------------------------------------------------------------------------
	class COptServer
	{
		private:

			// connection descriptor
			struct SConnectionDescriptor
			{
				// ID
				ULONG_PTR m_ulpId;

				// task
				CTask *m_ptsk;

				// socket
				CSocket *m_psocket;

				// link for hashtable
				SLink m_link;

				// invalid connection id
				static
				ULONG_PTR m_ulpInvalid;

				// ctor
				SConnectionDescriptor
					(
					CTask *ptsk,
					CSocket *psocket
					)
					:
					m_ulpId((ULONG_PTR) ptsk),
					m_ptsk(ptsk),
					m_psocket(psocket)
				{}

			};

			typedef CSyncHashtable<SConnectionDescriptor, ULONG_PTR, CSpinlockOS>
				ConnectionHT;

			typedef CSyncHashtableAccessByKey<SConnectionDescriptor, ULONG_PTR, CSpinlockOS>
				ConnectionKeyAccessor;

			typedef CSyncHashtableIter<SConnectionDescriptor, ULONG_PTR, CSpinlockOS>
				ConnectionIter;

			typedef CSyncHashtableAccessByIter<SConnectionDescriptor, ULONG_PTR, CSpinlockOS>
				ConnectionIterAccessor;

			// path where socket is initialized
			const CHAR *m_szSocketPath;

			// memory pool for connections
			IMemoryPool *m_pmp;

			// hashtable of connections
			ConnectionHT *m_pshtConnections;

			// default id for the source system
			static
			const CSystemId m_sysidDefault;

			// ctor
			explicit
			COptServer(const CHAR *szPath);

			// dtor
			~COptServer();

			// start serving requests
			void Loop();

			// initialize hashtable
			void InitHT();

			// register connection for status checking
			void TrackConnection(CTask *ptsk, CSocket *psocket);

			// release connection
			void ReleaseConnection(CTask *ptsk);

			// connection check task
			static
			void * PvCheckConnections(void *pv);

			// optimization task
			static
			void *PvOptimize(void *pv);

			// receive optimization request and construct query context for it
			static
			CQueryContext *PqcRecvQuery(IMemoryPool *pmp, CCommunicator *pcomm, CMDAccessor *pmda);

			// extract query plan, serialize it and send it to client
			static
			void SendPlan
				(
				IMemoryPool *pmp,
				CCommunicator *pcomm,
				CMDAccessor *pmda,
				CQueryContext *pqc,
				CExpression *pexprPlan
				);

			// dump collected artifacts to file
			static
			void FinalizeMinidump(CMiniDumperDXL *pmdmp);

		public:

			// invoke optimizer instance
			static
			void *PvRun(void *pv);

	}; // class COptServer
}

#endif // !COptServer_H


// EOF
