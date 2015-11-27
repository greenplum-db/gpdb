//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		COptClient.h
//
//	@doc:
//		API for optimizer client
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef COptClient_H
#define COptClient_H

#include "gpos/base.h"

#include "naucrates/md/CSystemId.h"

// forward declarations
namespace gpopt
{
	class CMDAccessor;
}

namespace gpnaucrates
{
	class CCommunicator;
}

namespace gpmd
{
	class IMDProvider;
	class CMDProviderCommProxy;
}


namespace gpoptudfs
{
	using namespace gpos;
	using namespace gpopt;
	using namespace gpmd;
	using namespace gpnaucrates;

	//---------------------------------------------------------------------------
	//	@class:
	//		COptClient
	//
	//	@doc:
	//		Optimizer client;
	//		passes optimization request to server, provides metadata and
	//		builds planned statement from returned query plan;
	//
	//---------------------------------------------------------------------------
	class COptClient
	{
		private:

			// struct containing optimization request parameters;
			// needs to be in sync with the argument passed by the client;
			struct SOptParams
			{
				// path where socket is initialized
				const char *m_szPath;

				// input query
				Query *m_pquery;
			};

			// input query
			Query *m_pquery;

			// path where socket is initialized
			const char *m_szPath;

			// memory pool
			IMemoryPool *m_pmp;

			// communicator
			CCommunicator *m_pcomm;

			// default id for the source system
			static
			const CSystemId m_sysidDefault;

			// error severity levels

			// array mapping GPOS to elog() error severity
			static
			ULONG m_rgrgulSev[CException::ExsevSentinel][2];

			// ctor
			COptClient
				(
				SOptParams *pop
				)
				:
				m_pquery(pop->m_pquery),
				m_szPath(pop->m_szPath),
				m_pmp(NULL),
				m_pcomm(NULL)
			{
				GPOS_ASSERT(NULL != m_pquery);
				GPOS_ASSERT(NULL != m_szPath);
			}

			// dtor
			~COptClient()
			{}

			// request optimization from server
			PlannedStmt *PplstmtOptimize();

			// set traceflags
			void SetTraceflags();

			// send query optimization request to server
			void SendRequest(CMDAccessor *pmda);

			// retrieve DXL plan
			const CHAR *SzPlanDXL(IMDProvider *pmdp);

			// send MD response
			void SendMDResponse(CMDProviderCommProxy *pmdpcp, const WCHAR *wszReq);

			// build planned statement from serialized plan
			PlannedStmt *PplstmtConstruct(CMDAccessor *pmda, const CHAR *szPlan);

			// elog wrapper
			void Elog(ULONG ulSev, const WCHAR *wszMsg);

		public:

			// invoke optimizer instance
			static
			void *PvRun(void *pv);

	}; // class COptClient
}

#endif // !COptClient_H


// EOF
