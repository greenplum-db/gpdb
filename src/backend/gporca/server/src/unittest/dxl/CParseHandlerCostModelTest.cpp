//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.



#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>

#include "unittest/dxl/CParseHandlerCostModelTest.h"

#include <memory>

#include "gpdbcost/CCostModelGPDB.h"
#include "gpdbcost/CCostModelParamsGPDB.h"
#include "gpdbcost/CCostModelParamsGPDBLegacy.h"


#include "gpos/base.h"
#include "gpos/test/CUnittest.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/common/CAutoRg.h"
#include "gpos/io/COstreamString.h"
#include "naucrates/dxl/parser/CParseHandlerCostModel.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/CCostModelConfigSerializer.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using gpdbcost::CCostModelGPDB;
using gpdbcost::CCostModelParamsGPDB;

XERCES_CPP_NAMESPACE_USE


namespace
{
	class Fixture
	{
		private:
			CAutoMemoryPool m_amp;
			gpos::CAutoP<CDXLMemoryManager> m_apmm;
			std::unique_ptr<SAX2XMLReader> m_apxmlreader;
			gpos::CAutoP<CParseHandlerManager> m_apphm;
			gpos::CAutoP<CParseHandlerCostModel> m_apphCostModel;

	public:
			Fixture():
					m_apmm(GPOS_NEW(Pmp()) CDXLMemoryManager(Pmp())),
					m_apxmlreader(XMLReaderFactory::createXMLReader(GetDXLMemoryManager())),
					m_apphm(GPOS_NEW(Pmp()) CParseHandlerManager(GetDXLMemoryManager(), Pxmlreader())),
					m_apphCostModel(GPOS_NEW(Pmp()) CParseHandlerCostModel(Pmp(), Pphm(), NULL))
			{
				m_apphm->ActivateParseHandler(PphCostModel());
			}

			CMemoryPool *Pmp() const
			{
				return m_amp.Pmp();
			}

			CDXLMemoryManager *GetDXLMemoryManager()
			{
				return m_apmm.Value();
			}

			SAX2XMLReader *Pxmlreader()
			{
				return m_apxmlreader.get();
			}

			CParseHandlerManager *Pphm()
			{
				return m_apphm.Value();
			}

			CParseHandlerCostModel *PphCostModel()
			{
				return m_apphCostModel.Value();
			}

			void Parse(const XMLByte dxl_string[], size_t size)
			{
				MemBufInputSource mbis(
					dxl_string,
					size,
					"dxl test",
					false,
					GetDXLMemoryManager()
				);
				Pxmlreader()->parse(mbis);
			}
	};
}

static gpos::GPOS_RESULT Eres_ParseCalibratedCostModel()
{
	const CHAR dxl_filename[] = "../data/dxl/parse_tests/CostModelConfigCalibrated.xml";
	Fixture fixture;

	CMemoryPool *mp = fixture.Pmp();

	gpos::CAutoRg<CHAR> a_szDXL(CDXLUtils::Read(mp, dxl_filename));

	CParseHandlerCostModel *pphcm = fixture.PphCostModel();

	fixture.Parse((const XMLByte *)a_szDXL.Rgt(), strlen(a_szDXL.Rgt()));

	ICostModel *pcm = pphcm->GetCostModel();

	GPOS_RTL_ASSERT(ICostModel::EcmtGPDBCalibrated == pcm->Ecmt());
	GPOS_RTL_ASSERT(3 == pcm->UlHosts());

	CAutoRef<CCostModelParamsGPDB> pcpExpected(GPOS_NEW(mp) CCostModelParamsGPDB(mp));
	pcpExpected->SetParam(CCostModelParamsGPDB::EcpNLJFactor, 1024.0, 1023.0, 1025.0);
	GPOS_RTL_ASSERT(pcpExpected->Equals(pcm->GetCostModelParams()));


	return gpos::GPOS_OK;
}

static gpos::GPOS_RESULT Eres_SerializeCalibratedCostModel()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	const WCHAR *const wszExpectedString = L"<dxl:CostModelConfig CostModelType=\"1\" SegmentsForCosting=\"3\">"
								   "<dxl:CostParams>"
								   "<dxl:CostParam Name=\"NLJFactor\" Value=\"1024.000000\" LowerBound=\"1023.000000\" UpperBound=\"1025.000000\"/>"
								   "</dxl:CostParams>"
								   "</dxl:CostModelConfig>";
	gpos::CAutoP<CWStringDynamic> apwsExpected(GPOS_NEW(mp) CWStringDynamic(mp, wszExpectedString));

	const ULONG ulSegments = 3;
	CCostModelParamsGPDB *pcp = GPOS_NEW(mp) CCostModelParamsGPDB(mp);
	pcp->SetParam(CCostModelParamsGPDB::EcpNLJFactor, 1024.0, 1023.0, 1025.0);
	gpos::CAutoRef<CCostModelGPDB> apcm(GPOS_NEW(mp) CCostModelGPDB(mp, ulSegments, pcp));

	CWStringDynamic wsActual(mp);
	COstreamString os(&wsActual);
	CXMLSerializer xml_serializer(mp, os, false);
	CCostModelConfigSerializer cmcSerializer(apcm.Value());
	cmcSerializer.Serialize(xml_serializer);

	GPOS_RTL_ASSERT(apwsExpected->Equals(&wsActual));

	return gpos::GPOS_OK;
}

static gpos::GPOS_RESULT Eres_ParseLegacyCostModel()
{
	const CHAR dxl_filename[] = "../data/dxl/parse_tests/CostModelConfigLegacy.xml";
	Fixture fixture;

	CMemoryPool *mp = fixture.Pmp();

	gpos::CAutoRg<CHAR> a_szDXL(CDXLUtils::Read(mp, dxl_filename));

	CParseHandlerCostModel *pphcm = fixture.PphCostModel();

	fixture.Parse((const XMLByte *)a_szDXL.Rgt(), strlen(a_szDXL.Rgt()));

	ICostModel *pcm = pphcm->GetCostModel();

	GPOS_RTL_ASSERT(ICostModel::EcmtGPDBLegacy == pcm->Ecmt());
	GPOS_RTL_ASSERT(3 == pcm->UlHosts());

	CAutoRef<CCostModelParamsGPDBLegacy> pcpExpected(GPOS_NEW(mp) CCostModelParamsGPDBLegacy(mp));
	GPOS_RTL_ASSERT(pcpExpected->Equals(pcm->GetCostModelParams()));

	return gpos::GPOS_OK;
}

gpos::GPOS_RESULT CParseHandlerCostModelTest::EresUnittest()
{
	CUnittest rgut[] =
			{
				GPOS_UNITTEST_FUNC(Eres_ParseCalibratedCostModel),
				GPOS_UNITTEST_FUNC(Eres_SerializeCalibratedCostModel),
				GPOS_UNITTEST_FUNC(Eres_ParseLegacyCostModel),
			};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}
