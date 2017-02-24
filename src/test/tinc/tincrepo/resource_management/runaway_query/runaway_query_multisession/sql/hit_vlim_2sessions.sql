-- @Description Ensures that two sessions together allocate more than VLIM and one of them errors out
-- @author George Caragea
-- @vlimMB 900 
-- @slimMB 600

-- content/segment = 0; size = 455MB; sleep = 20 sec; crit_section = false
1&: select gp_allocate_palloc_test_all_segs(0, 455 * 1024 * 1024, 20, false);

-- give session 1 enough time to do the allocation
2&: select pg_sleep(10);
2<:

-- content/segment = 0; size = 450MB; sleep = 20 sec; crit_section = false
2&: select gp_allocate_palloc_test_all_segs(0, 450 * 1024 * 1024, 20, false);

1<:
2<:
