-- @Description Ensures that pinCount is released on SessionState when a QE calls elog(FATAL)
-- @author George Caragea
-- @vlimMB 1100 
-- @slimMB 0
-- @redzone 80

-- Check that only one session is running
1: select count(*) from session_state.session_level_memory_consumption where segid = 0;

-- Start another session; set gp_debug_linger to 0 so we clean up immediately on FATAL
2: set gp_debug_linger = 0;

-- Set the timeout to keep idle QEs around to 60 seconds so they don't get cleaned up 
2: set gp_vmem_idle_resource_timeout = 60000;

-- content/segment = 0; size = 120MB; sleep = 1 sec
2: select gp_allocate_top_memory_ctxt_test_all_segs(0, 120 * 1024 * 1024, 1);

1&: select pg_sleep(2);
1<:

-- Check that now we have two sessions active
1: select count(*) from session_state.session_level_memory_consumption where segid = 0;

-- Have session 2 call a elog(FATAL) on segment 0
--  content/segment = 0; FAULT_TYPE_FATAL = 2; sleep = 0
2: select gp_inject_fault_test_all_segs(0, 2, 0);

-- make sure session 2 is still around and accepting queries
2: select 1;

1&: select pg_sleep(5);
1<:

-- only one active session on segment 0 should be left
1: select count(*) from session_state.session_level_memory_consumption where segid = 0;

