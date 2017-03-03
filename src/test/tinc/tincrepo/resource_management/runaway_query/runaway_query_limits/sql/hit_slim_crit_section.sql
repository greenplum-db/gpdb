-- @Description Ensures that one session does not hit SLIM in critical section
-- @author George Caragea
-- @vlimMB 900 
-- @slimMB 600

-- content/segment = 0; size = 601MB; sleep = 0 sec; crit_section = true
select gp_allocate_palloc_test_all_segs(0, 601 * 1024 * 1024, 0, true);
