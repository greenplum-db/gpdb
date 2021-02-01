#include "unittest/gpopt/operators/CScalarIsDistinctFromTest.h"

#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/string/CWStringConst.h"

#include "gpopt/operators/CScalarIsDistinctFrom.h"

#include "unittest/gpopt/CTestUtils.h"

namespace gpopt
{
using namespace gpos;

class SEberFixture
{
private:
	const CAutoMemoryPool m_amp;
	CMDAccessor m_mda;
	const CAutoOptCtxt m_aoc;
	CScalarIsDistinctFrom *const m_pScalarIDF;

	static IMDProvider *
	Pmdp()
	{
		CTestUtils::m_pmdpf->AddRef();
		return CTestUtils::m_pmdpf;
	}

public:
	SEberFixture()
		: m_amp(),
		  m_mda(m_amp.Pmp(), CMDCache::Pcache(), CTestUtils::m_sysidDefault,
				Pmdp()),
		  m_aoc(m_amp.Pmp(), &m_mda, NULL /* pceeval */,
				CTestUtils::GetCostModel(m_amp.Pmp())),
		  m_pScalarIDF(GPOS_NEW(m_amp.Pmp()) CScalarIsDistinctFrom(
			  Pmp(), GPOS_NEW(m_amp.Pmp()) CMDIdGPDB(GPDB_INT4_EQ_OP),
			  GPOS_NEW(m_amp.Pmp()) CWStringConst(GPOS_WSZ_LIT("="))))
	{
	}

	~SEberFixture()
	{
		m_pScalarIDF->Release();
	}

	CMemoryPool *
	Pmp() const
	{
		return m_amp.Pmp();
	}

	CScalarIsDistinctFrom *
	PScalarIDF() const
	{
		return m_pScalarIDF;
	}
};

static GPOS_RESULT
EresUnittest_Eber_WhenBothInputsAreNull()
{
	SEberFixture fixture;
	CMemoryPool *mp = fixture.Pmp();

	ULongPtrArray *pdrgpulChildren = GPOS_NEW(mp) ULongPtrArray(mp);
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberNull));
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberNull));

	CScalarIsDistinctFrom *pScalarIDF = fixture.PScalarIDF();

	CScalar::EBoolEvalResult eberResult = pScalarIDF->Eber(pdrgpulChildren);
	GPOS_RTL_ASSERT(eberResult == CScalar::EberFalse);

	pdrgpulChildren->Release();

	return GPOS_OK;
}

static GPOS_RESULT
EresUnittest_Eber_WhenFirstInputIsUnknown()
{
	SEberFixture fixture;
	CMemoryPool *mp = fixture.Pmp();

	ULongPtrArray *pdrgpulChildren = GPOS_NEW(mp) ULongPtrArray(mp);
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberAny));
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberNull));

	CScalarIsDistinctFrom *pScalarIDF = fixture.PScalarIDF();

	CScalar::EBoolEvalResult eberResult = pScalarIDF->Eber(pdrgpulChildren);
	GPOS_RTL_ASSERT(eberResult == CScalar::EberAny);

	pdrgpulChildren->Release();

	return GPOS_OK;
}

static GPOS_RESULT
EresUnittest_Eber_WhenSecondInputIsUnknown()
{
	SEberFixture fixture;
	CMemoryPool *mp = fixture.Pmp();

	ULongPtrArray *pdrgpulChildren = GPOS_NEW(mp) ULongPtrArray(mp);
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberNull));
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberAny));

	CScalarIsDistinctFrom *pScalarIDF = fixture.PScalarIDF();

	CScalar::EBoolEvalResult eberResult = pScalarIDF->Eber(pdrgpulChildren);
	GPOS_RTL_ASSERT(eberResult == CScalar::EberAny);

	pdrgpulChildren->Release();

	return GPOS_OK;
}

static GPOS_RESULT
EresUnittest_Eber_WhenFirstInputDiffersFromSecondInput()
{
	SEberFixture fixture;
	CMemoryPool *mp = fixture.Pmp();

	ULongPtrArray *pdrgpulChildren = GPOS_NEW(mp) ULongPtrArray(mp);
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberNull));
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberTrue));

	CScalarIsDistinctFrom *pScalarIDF = fixture.PScalarIDF();

	CScalar::EBoolEvalResult eberResult = pScalarIDF->Eber(pdrgpulChildren);
	GPOS_RTL_ASSERT(eberResult == CScalar::EberTrue);

	pdrgpulChildren->Release();

	return GPOS_OK;
}

static GPOS_RESULT
EresUnittest_Eber_WhenBothInputsAreSameAndNotNull()
{
	SEberFixture fixture;
	CMemoryPool *mp = fixture.Pmp();

	ULongPtrArray *pdrgpulChildren = GPOS_NEW(mp) ULongPtrArray(mp);
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberTrue));
	pdrgpulChildren->Append(GPOS_NEW(mp) ULONG(CScalar::EberTrue));

	CScalarIsDistinctFrom *pScalarIDF = fixture.PScalarIDF();

	CScalar::EBoolEvalResult eberResult = pScalarIDF->Eber(pdrgpulChildren);
	GPOS_RTL_ASSERT(eberResult == CScalar::EberFalse);

	pdrgpulChildren->Release();

	return GPOS_OK;
}

GPOS_RESULT
CScalarIsDistinctFromTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_Eber_WhenBothInputsAreNull),
		GPOS_UNITTEST_FUNC(EresUnittest_Eber_WhenFirstInputIsUnknown),
		GPOS_UNITTEST_FUNC(EresUnittest_Eber_WhenSecondInputIsUnknown),
		GPOS_UNITTEST_FUNC(
			EresUnittest_Eber_WhenFirstInputDiffersFromSecondInput),
		GPOS_UNITTEST_FUNC(EresUnittest_Eber_WhenBothInputsAreSameAndNotNull),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}
}  // namespace gpopt
