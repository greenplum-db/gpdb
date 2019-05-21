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
				const char *m_path;

				// input query
				Query *m_query;
			};

			// input query
			Query *m_query;

			// path where socket is initialized
			const char *m_path;

			// memory pool
			CMemoryPool *m_mp;

			// communicator
			CCommunicator *m_communicator;

			// default id for the source system
			static
			const CSystemId m_default_sysid;

			// error severity levels

			// array mapping GPOS to elog() error severity
			static
			ULONG elog_to_severity_map[CException::ExsevSentinel][2];

			// ctor
			COptClient
				(
				SOptParams *op
				)
				:
				m_query(op->m_query),
				m_path(op->m_path),
				m_mp(NULL),
				m_communicator(NULL)
			{
				GPOS_ASSERT(NULL != m_query);
				GPOS_ASSERT(NULL != m_path);
			}

			// dtor
			~COptClient()
			{}

			// request optimization from server
			PlannedStmt *GPOPTOptimizedPlan();

			// set traceflags
			void SetTraceflags();

			// send query optimization request to server
			void SendRequest(CMDAccessor *md_accessor);

			// retrieve DXL plan
			const CHAR *SzPlanDXL(IMDProvider *md_provider);

			// send MD response
			void SendMDResponse(CMDProviderCommProxy *md_provider_proxy, const WCHAR *req);

			// build planned statement from serialized plan
			PlannedStmt *ConstructPlanStmt(CMDAccessor *md_accessor, const CHAR *serialized_plan);

			// elog wrapper
			void Elog(ULONG severity, const WCHAR *msg);

		public:

			// invoke optimizer instance
			static
			void *Run(void *pv);

	}; // class COptClient
}

#endif // !COptClient_H


// EOF
