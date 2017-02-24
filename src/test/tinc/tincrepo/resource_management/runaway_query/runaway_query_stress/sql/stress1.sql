-- @Description Test concurrency for runaway query termination
-- @author Zhongxian Gu
-- @concurrency 10

\c gptest

select gp_allocate_palloc_test_all_segs(0, 1 * 1024 * 1024, 2, false);

\c gptest

select gp_allocate_palloc_test_all_segs(0, 1 * 1024 * 1024, 4, false);

\c gptest

select gp_allocate_palloc_test_all_segs(0, 1 * 1024 * 1024, 6, false);

\c gptest

select gp_allocate_palloc_test_all_segs(0, 1 * 1024 * 1024, 8, false);

\c gptest

select gp_allocate_palloc_test_all_segs(0, 1 * 1024 * 1024, 10, false);

