-- @Description Test releasing session state when running very many sessions one after the other
-- @author George Caragea
-- @concurrency 1
-- We want at least max_connections + 1 iterations here. max_connections is typically set for 250
-- @iterations 550

select gp_allocate_palloc_test_all_segs(-2, 1 * 1024 * 1024, 0, false);

begin;
DECLARE f CURSOR WITH HOLD FOR
select * from stress_iterations_table
ORDER BY num, letter;
commit;

FETCH FROM f;

FETCH FROM f;

