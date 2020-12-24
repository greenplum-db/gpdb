/*---------------------------------------------------------------------------
 *	Greenplum Database
 *	Copyright (c) 2019 VMware, Inc. or its affiliates.
 *
 *	@filename:
 *		CDebugCounter.h
 *
 *	@doc:
 *		Event counters for debugging purposes
 *
 *---------------------------------------------------------------------------*/

#ifndef GPOS_CDebugCounter_h
#define GPOS_CDebugCounter_h

/*--------------------------------------------------------------------------
                              How to use this class
   --------------------------------------------------------------------------

 1. Ensure the GPOS_DEBUG_COUNTERS define is enabled:

    Use a debug build or change the code below to enable GPOS_DEBUG_COUNTERS
    unconditionally, so you can measure a retail build

 2. Make up name(s) of the event or events you want to count

 3. Add one or more of the following lines to your code:

      #include "gpos/common/CDebugCounter.h"

      // to simply count how many times we reached this point:
      GPOS_DEBUG_COUNTER_BUMP("my counter name 1");

      // to sum up things to be counted for each query:
      GPOS_DEBUG_COUNTER_ADD("my counter name 2", num_widgets_allocated);

      // to sum up a floating point number
      GPOS_DEBUG_COUNTER_ADD_DOUBLE("my counter name 3", scan_cost);

      // to count the total time spent to do something
      GPOS_DEBUG_COUNTER_START_CPU_TIME("my counter name 4");
      // do something
      GPOS_DEBUG_COUNTER_STOP_CPU_TIME("my counter name 4");

    Note that you can do this multiple times per statement. ORCA will
    log a summary of the counter values after each statement.

 4. Run some statements

    To make it easier to read the resulting log with multiple queries,
    you can name a query by adding a constant select with a single string
    starting with exactly the text 'query name: ':

      select 'query name: my so very challenging query 7';
      -- now the actual query
      explain select ...;

 5. Extract the log information into a CSV format, using the
    script gporca/scripts/get_debug_event_counters.py

    Run this script with the --help option to get more info.
    Usually (if you have the MASTER_DATA_DIRECTORY environment
    variable set), you can just run the script without any arguments.
    The script can also store the result in an SQL table.

 6. Clean up the code before submitting a PR
    - completely remove all of your modifications
    - in rare cases, if you really do want to keep some lines with
      GPOS_DEBUG_COUNTER_... macros in the code, surround them
      with #ifdefs that are controlled from within this header file,
      CDebugCounter.h.

      // leave a commented-out line to enable your feature
      // uncomment this to enable counting of xforms
      // #define GPOS_DEBUG_COUNTER_XFORMS

      // in the instrumented code
      #ifdef GPOS_DEBUG_COUNTER_XFORMS
      GPOS_DEBUG_COUNTER_BUMP(xform_name);
      #endif

  ---------------------------------------------------------------------------*/


#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CHashMap.h"
#include "gpos/common/CHashMapIter.h"
#include "gpos/common/CTimerUser.h"

#ifdef GPOS_DEBUG
// for now, disable this on Linux, as it caused the CentOS 6 and 7 builds to fail
// (for some not well-known cause, possibly due to an inconsistency in a test script)
#ifndef __linux__
// enable this feature
#define GPOS_DEBUG_COUNTERS
#endif
#endif

// define macros
#ifdef GPOS_DEBUG_COUNTERS

#define GPOS_DEBUG_COUNTER_BUMP(c) gpos::CDebugCounter::Bump(c)
#define GPOS_DEBUG_COUNTER_ADD(c, d) gpos::CDebugCounter::Add(c, d)
#define GPOS_DEBUG_COUNTER_ADD_DOUBLE(c, d) gpos::CDebugCounter::AddDouble(c, d)
#define GPOS_DEBUG_COUNTER_START_CPU_TIME(c) \
	gpos::CDebugCounter::StartCpuTime(c)
#define GPOS_DEBUG_COUNTER_STOP_CPU_TIME(c) gpos::CDebugCounter::StopCpuTime(c)
#else
// empty definitions otherwise
#define GPOS_DEBUG_COUNTER_BUMP(c)
#define GPOS_DEBUG_COUNTER_ADD(c, d)
#define GPOS_DEBUG_COUNTER_ADD_DOUBLE(c, d)
#define GPOS_DEBUG_COUNTER_START_CPU_TIME(c)
#define GPOS_DEBUG_COUNTER_STOP_CPU_TIME(c)
#endif

namespace gpos
{
#ifdef GPOS_DEBUG_COUNTERS
class CDebugCounter
{
	// types of supported counters
	enum ECounterType
	{
		ECounterTypeCount,
		ECounterTypeSum,
		ECounterTypeSumDouble,
		ECounterTypeCpuTime,
		ECounterTypeSentinel
	};

	// a key that identifies a counter for a given query
	struct SDebugCounterKey : public CRefCount
	{
		SDebugCounterKey(const char *counter_name)
			: m_counter_name(counter_name)
		{
		}

		~SDebugCounterKey()
		{
		}

		static ULONG HashValue(const SDebugCounterKey *key);
		static BOOL
		Equals(const SDebugCounterKey *first, const SDebugCounterKey *second)
		{
			return first->m_counter_name == second->m_counter_name;
		}

		std::string m_counter_name;
	};

	// a counter value (a union of all possible counter types)
	struct SDebugCounterValue : public CRefCount
	{
		SDebugCounterValue(ECounterType typ)
			: m_type(typ),
			  m_counter_val_long(0),
			  m_counter_val_double(0.0),
			  m_cpu_timer(NULL),
			  m_timer_is_running(false)
		{
		}
		~SDebugCounterValue()
		{
			if (NULL != m_cpu_timer)
				GPOS_DELETE(m_cpu_timer);
		}

		enum ECounterType m_type;
		// longs are used for counts and sums of integers
		ULLONG m_counter_val_long;
		// doubles are used for sums of floating point values
		double m_counter_val_double;
		// longs, in conjunction with a timer, are used for CPU timer measurements
		ITimer *m_cpu_timer;
		BOOL m_timer_is_running;
	};

	// a hash table to store named counters
	// (excluding run and statement number, since we only store counters for the current query)
	typedef CHashMap<SDebugCounterKey, SDebugCounterValue,
					 SDebugCounterKey::HashValue, SDebugCounterKey::Equals,
					 CleanupRelease<SDebugCounterKey>,
					 CleanupRelease<SDebugCounterValue> >
		CounterKeyToValueMap;

	// ...and an iterator type for the hash table type above
	typedef CHashMapIter<SDebugCounterKey, SDebugCounterValue,
						 SDebugCounterKey::HashValue, SDebugCounterKey::Equals,
						 CleanupRelease<SDebugCounterKey>,
						 CleanupRelease<SDebugCounterValue> >
		CounterKeyToValueMapIterator;

public:
	// methods used by ORCA to provide the infrastructure for event counting
	CDebugCounter(CMemoryPool *mp);

	~CDebugCounter();

	// The static CDebugCounter lives from gpos_init to gpos_terminate
	static void Init();
	static void Shutdown();

	// Prepare for running the next query, providing an optional name for the next
	// query. Note that if a name is provided, we assume the current query is a
	// no-op, just used to provide the query name
	static void NextQry(const char *next_qry_name);

	// methods used by developers who want to count events

	// record an event to be counted
	static void Bump(const char *counter_name);

	// add an integer or floating point quantity
	static void Add(const char *counter_name, long delta);
	static void AddDouble(const char *counter_name, double delta);

	// start/stop recording time for a time-based event
	static void StartCpuTime(const char *counter_name);
	static void StopCpuTime(const char *counter_name);

private:
	// put this on the stack to temporarily disable debug counters while
	// in one of our methods
	class AutoDisable
	{
	public:
		AutoDisable()
		{
			m_instance->m_suppress_counting = true;
		}
		~AutoDisable()
		{
			m_instance->m_suppress_counting = false;
		}
	};

	// prevent crashes and infinite recursion when trying to call these methods at the wrong time
	static bool
	OkToProceed()
	{
		return (NULL != m_instance && !m_instance->m_suppress_counting);
	}

	// search for a counter by name and return
	// allocated (existing or new) key and value structs
	BOOL FindByName(const char *counter_name, SDebugCounterKey **key,
					SDebugCounterValue **val, enum ECounterType typ);

	// insert or update a key value pair that was generated
	// by FindByName()
	void InsertOrUpdateCounter(SDebugCounterKey *key, SDebugCounterValue *val,
							   BOOL update);

	CMemoryPool *m_mp;

	// the single instance of this object, allocated by Init()
	static CDebugCounter *m_instance;

	// add a start marker to help finding events from one session
	BOOL m_start_marker_has_been_logged;

	// turn off debug counters triggered by internal calls
	BOOL m_suppress_counting;

	// statement number, increased every time a SQL statement gets optimized by ORCA
	ULONG m_qry_number;

	// optional query name, assigned by explaining or running a query of a special form,
	// see above
	std::string m_qry_name;
	// is this statement a constant get to name the next query?
	BOOL m_is_name_constant_get;

	// hash map, holding all the counter values for
	// the current query
	CounterKeyToValueMap *m_hashmap;
};

#endif /* GPOS_DEBUG_COUNTERS */
}  // namespace gpos
#endif /* GPOS_CDebugCounter_h */
