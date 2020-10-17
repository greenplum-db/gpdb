/*---------------------------------------------------------------------------
 *	Greenplum Database
 *	Copyright (c) 2019 Pivotal Software, Inc.
 *
 *	@filename:
 *		CDebugCounter.cpp
 *
 *	@doc:
 *		Event counters for debugging purposes
 *
 *---------------------------------------------------------------------------*/

#include "gpos/common/CDebugCounter.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CAutoMemoryPool.h"

using namespace gpos;

#ifdef GPOS_DEBUG_COUNTERS

// initialize static variables

CDebugCounter *CDebugCounter::m_instance = NULL;

ULONG
CDebugCounter::SDebugCounterKey::HashValue(
	const CDebugCounter::SDebugCounterKey *key)
{
	ULONG result = 0;
	int key_size = key->m_counter_name.size();

	for (int i = 0; i < key_size; i++)
	{
		result += result * 257 + key->m_counter_name.at(i);
	}

	return result;
}

CDebugCounter::CDebugCounter(CMemoryPool *mp)
	: m_mp(mp),
	  m_start_marker_has_been_logged(false),
	  m_suppress_counting(false),
	  m_qry_number(0),
	  m_qry_name(""),
	  m_is_name_constant_get(false),
	  m_hashmap(NULL)
{
	m_hashmap = GPOS_NEW(mp) CounterKeyToValueMap(mp);
}

CDebugCounter::~CDebugCounter()
{
	CRefCount::SafeRelease(m_hashmap);
	m_hashmap = NULL;
}

void
CDebugCounter::Init()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	GPOS_RTL_ASSERT(NULL == m_instance);
	m_instance = GPOS_NEW(mp) CDebugCounter(mp);

	// detach safety
	(void) amp.Detach();
}

void
CDebugCounter::Shutdown()
{
	if (NULL != m_instance)
	{
		CMemoryPool *mp = m_instance->m_mp;

		GPOS_DELETE(m_instance);
		m_instance = NULL;
		CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(mp);
	}
}

void
CDebugCounter::NextQry(const char *next_qry_name)
{
	if (NULL != next_qry_name && '\0' != *next_qry_name)
	{
		m_instance->m_qry_name = next_qry_name;
		m_instance->m_is_name_constant_get = true;
	}
	else
	{
		m_instance->m_qry_number++;
		if (m_instance->m_is_name_constant_get)
		{
			// the previous query specified our name, use it
			// and reset the flag, as this is now a real query
			m_instance->m_is_name_constant_get = false;
		}
		else
		{
			// an unnamed query
			m_instance->m_qry_name = "";
		}
	}

	if (0 < m_instance->m_hashmap->Size())
	{
		if (!m_instance->m_is_name_constant_get)
		{
			if (!m_instance->m_start_marker_has_been_logged)
			{
				GPOS_TRACE(GPOS_WSZ_LIT("CDebugCounterEventLoggingStart"));
				m_instance->m_start_marker_has_been_logged = true;
			}

			// log all counter values
			CWStringDynamic wstr(m_instance->m_mp);
			// string stream for convenient formatting of wstr
			COstreamString os(&wstr);


			CounterKeyToValueMapIterator iter(m_instance->m_hashmap);

			while (iter.Advance())
			{
				const SDebugCounterValue *val = iter.Value();
				const char *typeString = "";
				BOOL use_long = true;

				switch (val->m_type)
				{
					case ECounterTypeCount:
						typeString = "count";
						break;
					case ECounterTypeSum:
						typeString = "sum";
						break;
					case ECounterTypeSumDouble:
						typeString = "sum_double";
						use_long = false;
						break;
					case ECounterTypeCpuTime:
						typeString = "cpu_usec";
						break;
					default:
						GPOS_RTL_ASSERT(!"Corrupted debug counter type");
						break;
				}

				wstr.Reset();
				os << "CDebugCounterEvent(qryid, qryname, counter, type, val), "
				   << m_instance->m_qry_number << ", "
				   << m_instance->m_qry_name.c_str() << ", "
				   << iter.Key()->m_counter_name.c_str() << ", " << typeString
				   << ", ";

				if (use_long)
				{
					os << val->m_counter_val_long;
				}
				else
				{
					os << val->m_counter_val_double;
				}
				GPOS_TRACE(wstr.GetBuffer());
			}
		}

		// clear the hash map for the next query by allocating a new, empty one in its stead
		m_instance->m_hashmap->Release();
		m_instance->m_hashmap =
			GPOS_NEW(m_instance->m_mp) CounterKeyToValueMap(m_instance->m_mp);
	}
}

BOOL
CDebugCounter::FindByName(const char *counter_name, SDebugCounterKey **key,
						  SDebugCounterValue **val, enum ECounterType typ)
{
	GPOS_RTL_ASSERT(NULL != key && NULL == *key);
	GPOS_RTL_ASSERT(NULL != val && NULL == *val);

	// search with a newly made key
	*key = GPOS_NEW(m_mp) SDebugCounterKey(counter_name);
	*val = m_hashmap->Find(*key);

	if (NULL != *val)
	{
		// make sure we don't mix counter types
		GPOS_RTL_ASSERT((*val)->m_type == typ);

		// return the yet unused key and the existing value
		return true;
	}

	// return a new key and value pair
	*val = GPOS_NEW(m_mp) SDebugCounterValue(typ);
	return false;
}

// insert or update a key value pair that was generated
// by FindByName()
void
CDebugCounter::InsertOrUpdateCounter(SDebugCounterKey *key,
									 SDebugCounterValue *val, BOOL update)
{
	if (update)
	{
		// we updated the key in-place, delete the extra
		// key that was used for searching
		key->Release();
	}
	else
	{
		// insert a new key/value pair
		m_hashmap->Insert(key, val);
	}
}


void
CDebugCounter::Bump(const char *counter_name)
{
	if (OkToProceed())
	{
		AutoDisable preventInfiniteRecursion;

		SDebugCounterKey *key = NULL;
		SDebugCounterValue *val = NULL;
		BOOL found =
			m_instance->FindByName(counter_name, &key, &val, ECounterTypeCount);

		val->m_counter_val_long++;

		m_instance->InsertOrUpdateCounter(key, val, found);
	}
}

void
CDebugCounter::Add(const char *counter_name, long delta)
{
	if (OkToProceed())
	{
		AutoDisable preventInfiniteRecursion;

		SDebugCounterKey *key = NULL;
		SDebugCounterValue *val = NULL;
		BOOL found =
			m_instance->FindByName(counter_name, &key, &val, ECounterTypeSum);

		val->m_counter_val_long += delta;
		m_instance->InsertOrUpdateCounter(key, val, found);
	}
}

void
CDebugCounter::AddDouble(const char *counter_name, double delta)
{
	if (OkToProceed())
	{
		AutoDisable preventInfiniteRecursion;

		SDebugCounterKey *key = NULL;
		SDebugCounterValue *val = NULL;
		BOOL found = m_instance->FindByName(counter_name, &key, &val,
											ECounterTypeSumDouble);

		val->m_counter_val_double += delta;
		m_instance->InsertOrUpdateCounter(key, val, found);
	}
}

void
CDebugCounter::StartCpuTime(const char *counter_name)
{
	if (OkToProceed())
	{
		AutoDisable preventInfiniteRecursion;

		SDebugCounterKey *key = NULL;
		SDebugCounterValue *val = NULL;
		BOOL found = m_instance->FindByName(counter_name, &key, &val,
											ECounterTypeCpuTime);

		if (NULL == val->m_cpu_timer)
		{
			val->m_cpu_timer = GPOS_NEW(m_instance->m_mp) CTimerUser();
		}
		else
		{
			val->m_cpu_timer->Restart();
		}
		val->m_timer_is_running = true;
		m_instance->InsertOrUpdateCounter(key, val, found);
	}
}

void
CDebugCounter::StopCpuTime(const char *counter_name)
{
	if (OkToProceed())
	{
		AutoDisable preventInfiniteRecursion;

		SDebugCounterKey *key = NULL;
		SDebugCounterValue *val = NULL;
		BOOL found = m_instance->FindByName(counter_name, &key, &val,
											ECounterTypeCpuTime);

		// note that we tolerate starting a timer but never stopping
		// it (it will be ignored), but we assert that if a timer is
		// being stopped and therefore logged, it has been started
		GPOS_RTL_ASSERT(NULL != val->m_cpu_timer && val->m_timer_is_running);
		val->m_counter_val_long += val->m_cpu_timer->ElapsedUS();
		val->m_timer_is_running = false;
		m_instance->InsertOrUpdateCounter(key, val, found);
	}
}

#endif /* GPOS_DEBUG_COUNTERS */
