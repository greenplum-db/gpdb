-- @Description Disable runaway query termination
-- @author Zhongxian Gu
-- @vlimMB 400 
-- @slimMB 0
-- @redzone 0

-- will hit OOM
-- content/segment = 0; size = 450MB; sleep = 20 sec
select gp_allocate_palloc_gradual_test_all_segs(0, 450*1024*1024, 20);