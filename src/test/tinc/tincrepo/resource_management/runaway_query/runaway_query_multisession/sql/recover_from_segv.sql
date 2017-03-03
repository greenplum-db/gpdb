-- @Description Testing TRQ: after a PM reset, correct reinitialization of VMEM and SessionState
-- @author Zhongxian Gu
-- @vlimMB 1000 
-- @slimMB 0
-- @redzone 80

-- Check that only one session is running
1: select count(*) from session_state.session_level_memory_consumption where segid = 0;

-- Check the vmem tracked for only one session running
1: select vmem_mb > 995 from gp_available_vmem_mb_test_all_segs where segid = 0;

-- Start another session; set gp_debug_linger to 0 so we clean up immediately on FATAL
2: set gp_debug_linger = 0;

-- Set the timeout to keep idle QEs around to 60 seconds so they don't get cleaned up 
2: set gp_vmem_idle_resource_timeout = 60000;

-- Check the vmem tracked for when two sessions are running, second one still idle
1: select vmem_mb > 995 from gp_available_vmem_mb_test_all_segs where segid = 0;

-- content/segment = 0; size = 250MB; sleep = 1 sec
2: select gp_allocate_top_memory_ctxt_test_all_segs(0, 250 * 1024 * 1024, 1);

1&: select pg_sleep(2);
1<:

-- Check that now we have two sessions active
1: select count(*) from session_state.session_level_memory_consumption where segid = 0;

-- Check the vmem tracked for when two sessions are running, one using some memory
1: select (vmem_mb > 745 AND vmem_mb < 751) from gp_available_vmem_mb_test_all_segs where segid = 0;


-- Have session 2 to trigger a PM reset
--  content/segment = 0; FAULT_TYPE_FATAL = 4 (SEGV); sleep = 0
2: select gp_inject_fault_test_all_segs(0, 4, 0);

-- only one active session on segment 0 should be left
3: select count(*) from session_state.session_level_memory_consumption where segid = 0;

-- Check that now vmem tracked is back to normal
3: select vmem_mb > 995 from gp_available_vmem_mb_test_all_segs where segid = 0;

