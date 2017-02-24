-- @Description Two sessions, one lightweight session detects the runaway
-- @author George Caragea
-- @vlimMB 900 
-- @slimMB 0
-- @redzone 80

-- Check to see that there is only one session running
1: select count(*) from session_state.session_level_memory_consumption where segid = -1;

-- check that exactly no backend is marked as runaway
1: select count(*) from session_state.session_level_memory_consumption where is_runaway='t';

-- content/segment = 0; size = 650MB; sleep = 30 sec; crit_section = false
1&: select gp_allocate_palloc_test_all_segs(0, 650 * 1024 * 1024, 30, false);

-- give session 1 enough time to do the allocation
2: select pg_sleep(3);

-- check that no backend is marked as runaway
2: select count(*) from session_state.session_level_memory_consumption where is_runaway='t';

-- content/segment = 0; size = 100MB; sleep = 0 sec; crit_section = false
2: select gp_allocate_palloc_test_all_segs(0, 100 * 1024 * 1024, 0, false);

-- check that exactly one backend is marked as runaway
2: select count(*) from session_state.session_level_memory_consumption where is_runaway='t';

1<:

