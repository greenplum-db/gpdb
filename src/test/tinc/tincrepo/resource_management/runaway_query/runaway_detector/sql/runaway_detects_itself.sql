-- @Description Only one session, runaway query detects itself as runaway
-- @author George Caragea
-- @vlimMB 900 
-- @slimMB 0
-- @redzone 80

-- Check to see that there is only one session running
1: select count(*) from session_state.session_level_memory_consumption where segid = -1;

-- check that exactly no backend is marked as runaway
1: select count(*) from session_state.session_level_memory_consumption where is_runaway='t';

-- content/segment = 0; size = 455MB; sleep = 20 sec; crit_section = false
1&: select gp_allocate_palloc_test_all_segs(0, 870 * 1024 * 1024, 20, false);

-- give session 1 enough time to do the allocation
2: select pg_sleep(3);

-- check that exactly one backend is marked as runaway
2: select count(*) from session_state.session_level_memory_consumption where is_runaway='t';

1<:

