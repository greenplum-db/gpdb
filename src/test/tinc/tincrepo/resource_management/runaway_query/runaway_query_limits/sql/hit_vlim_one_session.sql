-- @Description One session allocating > VLIM hits VLIM
-- @author George Caragea
-- @vlimMB 900 
-- @slimMB 0

-- content/segment = 0; size = 901MB; sleep = 0 sec; crit_section = false
select gp_allocate_palloc_test_all_segs(0, 901 * 1024 * 1024, 0, false);
