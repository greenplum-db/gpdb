//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConfigParamMapping.h
//
//	@doc:
//		Mapping of GPDB config params to traceflags
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CGUCMapping_H
#define GPOPT_CGUCMapping_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"
#include "gpos/memory/IMemoryPool.h"

#include "naucrates/traceflags/traceflags.h"

using namespace gpos;

namespace gpdxl
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CConfigParamMapping
	//
	//	@doc:
	//		Functionality for mapping GPDB config params to traceflags
	//
	//---------------------------------------------------------------------------
	class CConfigParamMapping
	{
		private:
			//------------------------------------------------------------------
			//	@class:
			//		SConfigMappingElem
			//
			//	@doc:
			//		Unit describing the mapping of a single GPDB config param
			//		to a trace flag
			//
			//------------------------------------------------------------------
			struct SConfigMappingElem
			{
				// trace flag
				EOptTraceFlag m_etf;

				// config param address
				BOOL *m_pfParam;

				// if true, we negate the config param value before setting traceflag value
				BOOL m_fNegate;

				// description
				const WCHAR *wszDescription;
			};

			// array of mapping elements
			static SConfigMappingElem m_elem[];

			// private ctor
			CConfigParamMapping(const CConfigParamMapping &);

		public:
			// pack enabled optimizer config params in a traceflag bitset
			static
			CBitSet *PbsPack(IMemoryPool *pmp, ULONG ulXforms);
	};
}

#endif // ! GPOPT_CGUCMapping_H

// EOF

