-- @Description Test vmem shown in the live view matches the amount in memory accounting
-- @author Zhongxian Gu

-- session1: issue the testing query
1: set explain_memory_verbosity = 'summary';
-- content/segment = 0; size = 100MB; sleep = 20 sec, critical section = false
1&: explain analyze select gp_allocate_palloc_test_all_segs(0, 100 * 1024 * 1024, 20, false);

-- session2: query the live view
2&: select pg_sleep(5);
2<:

2: select vmem_mb from session_state.session_state_memory_entries where segid = 0 and current_query ilike 'explain analyze select gp_allocate_palloc_test_all_segs%' and qe_count > 0;

-- session1: testing query finish
1<:
