-- @Description Testing TRQ: A query within a transaction block, all QEs active, terminates correctly after being detected as Run-away
-- @author Zhongxian Gu
-- @vlimMB 400 
-- @slimMB 0
-- @redzone 80

-- will hit red zone and get terminated
-- content/segment = 0; size = 450MB; sleep = 20 sec
begin;
select gp_allocate_palloc_gradual_test_all_segs(0, 450*1024*1024, 20);
commit;