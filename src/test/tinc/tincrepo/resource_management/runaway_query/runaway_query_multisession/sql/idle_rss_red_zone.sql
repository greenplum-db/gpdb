-- @Description Testing TRQ: After cleaning up an RSS, system is still in red-zone, and the same session is the top consumer, check the system is stable.
-- @author Zhongxian Gu
-- @vlimMB 1100 
-- @slimMB 0
-- @redzone 10

-- session1: issue a query that consumes large memory and will be terminated as a runaway query
-- content/segment = 0; size = 120MB; sleep = 1 sec
1: select gp_allocate_top_memory_ctxt_test_all_segs(0, 120 * 1024 * 1024, 1);

-- session2: query the view to make sure it's idle yet still consumes memory
2&: select pg_sleep(2);
2<:

2: select vmem_mb > 110, is_runaway from session_state.session_level_memory_consumption where segid= 0 and current_query ilike '<IDLE>%';

-- session2: issue queries with small usage of memory but processing many tuples to check system stability
-- these queries should run and finish without error
2&: select count(*) from test as a where a.c2 >= (select sum(b.c1) from test as b where a.c1 > b.c2);

2<:

2&: select count(*) from test as a where a.c2 >= (select sum(b.c1) from test as b where a.c1 > b.c2);

2<:

2&: select count(*) from test as a where a.c2 >= (select sum(b.c1) from test as b where a.c1 > b.c2);

2<:

-- session2: issue a query that consumes larger memory than the idle process and it should be terminated as primary-runaway query
2: select gp_allocate_top_memory_ctxt_test_all_segs(0, 125 * 1024 * 1024, 1);
