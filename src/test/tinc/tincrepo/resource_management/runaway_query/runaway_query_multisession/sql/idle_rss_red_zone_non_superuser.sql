-- @Description Testing TRQ: After cleaning up an RSS, system is still in red-zone, and the same session is the top consumer
-- @author Zhongxian Gu
-- @vlimMB 1100 
-- @slimMB 0
-- @redzone 10

-- session1: log in as non-super use and wait to issue a query
1: set role rqt_temp_role;

-- session2: issue a query that consumes large memory and will be terminated as a runaway query
-- content/segment = 0; size = 120MB; sleep = 1 sec
2: select gp_allocate_top_memory_ctxt_test_all_segs(0, 120 * 1024 * 1024, 1);

-- session3: query the view to make sure it's idle yet still consumes memory
3&: select pg_sleep(2);
3<:

3: select vmem_mb > 110, is_runaway from session_state.session_level_memory_consumption where segid= 0 and current_query ilike '<IDLE>%';

-- session1: A non-super user issues a query. This query should be terminated.
1: select count(*) from test as a where a.c2 >= (select sum(b.c1) from test as b where a.c1 > b.c2);

