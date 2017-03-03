-- @Description Enable runaway query termination
-- @author Zhongxian Gu
-- @vlimMB 400 
-- @slimMB 0
-- @redzone 80

-- will hit red zone and get terminated
-- content/segment = 0; size = 450MB; sleep = 20 sec
select gp_allocate_palloc_gradual_test_all_segs(0, 450*1024*1024, 20);