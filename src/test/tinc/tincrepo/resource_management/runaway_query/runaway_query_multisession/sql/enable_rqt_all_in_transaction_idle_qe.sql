-- @Description Testing TRQ: A query within a transaction block, some idle QEs, terminates correctly after being detected as Run-away
-- @author Zhongxian Gu
-- @vlimMB 400 
-- @slimMB 0
-- @redzone 80

-- session1: issue the query that has only one active QE; 
-- content/segment = 0; size = 100MB; sleep = 15 sec
1&: select gp_allocate_palloc_gradual_test_all_segs(0, 100*1024*1024, 15);

-- session2: Check the active QE, make sure there is only one.
2&: select pg_sleep(2);
2<:
2: select qe_count < 3 from session_state.session_level_memory_consumption where segid= 0 and current_query ilike 'select gp_allocate_palloc_gradual_test_all_segs%';

-- session1: the query should finish properly
1<:

-- session1: issue a query that has more than one active QEs
1: begin;
1: select count(*) from rqt_it_iq a, rqt_it_iq b where a.c1 < b.c2 and a.c2 > (select count(*) from rqt_it_iq c where c.c2 >= a.c1);

-- session2: Make sure there are more than one idle QEs
2&: select pg_sleep(5);
2<:
2: select qe_count > 3 from session_state.session_level_memory_consumption where segid= 0 and current_query ilike '<IDLE>%';

-- session1: issue the query that has only one active QE and memory-intensive in a transaction
-- content/segment = 0; size = 450MB; sleep = 20 sec
1&: select gp_allocate_palloc_gradual_test_all_segs(0, 450*1024*1024, 20); commit;

-- session1: the query should be terminated after being detected as Run-away
1<:
