-- @Description Ensures that pinCount is released on SessionState after QE cleaned up by idle gang
-- @author Zhongxian

-- session1: issue the testing query
1: set gp_vmem_idle_resource_timeout=20000;
1&: select count(*), pg_sleep(5) from rp a where a.c1 > (select sum(c1) from rp b where b.c2 < a.c1);

-- session2: query the view to see query in session1 is running
2&: select pg_sleep(2);
2<:
2: select count(*) from session_state.session_level_memory_consumption where segid= 0 and current_query ilike 'select count(*), pg_sleep(5) from rp%' and qe_count > 0;

-- session1: the query ends
1<:

-- session2: query the view to make sure there is an idle process there
2: select count(*) from session_state.session_level_memory_consumption where segid= 0 and current_query ilike '<IDLE%' and qe_count > 0;

2&: select pg_sleep(30);
2<:

-- session2: make sure the idle process is gone
2: select count(*) from session_state.session_level_memory_consumption where segid= 0 and current_query ilike '<IDLE%';

2&: select pg_sleep(5);
2<:
