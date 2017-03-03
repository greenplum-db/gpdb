-- @gucs gp_create_table_random_default_distribution=off
\echo -- start_ignore
-- ignore this output
\c mpp11880
\echo -- end_ignore

-- Check db after restore with GPDBRESTORE
select count(*) from public.test_10m;

-- drop parttest_dump database
\echo -- start_ignore
-- ignore this output
\c template1
\echo -- end_ignore
drop database mpp11880
