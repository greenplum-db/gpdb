--
-- Test workfile limits
--

-- Ensure the queries below need to spill to disk.
set statement_mem='1 MB';

-- SRF materializes the result in a tuplestore. Check that
-- gp_workfile_limit_per_query is enforced.
select count(distinct g) from generate_series(1, 1000000) g;

set gp_workfile_limit_per_query='5 MB';
select count(distinct g) from generate_series(1, 1000000) g;

reset gp_workfile_limit_per_query;

-- Also test limit on number of files (gp_workfile_limit_files_per_query)
set gp_workfile_limit_files_per_query='4';
select count(g) from generate_series(1, 500000) g
union
select count(g) from generate_series(1, 500000) g
union
select count(g) from generate_series(1, 500000) g
order by 1;

set gp_workfile_limit_files_per_query='2';
select count(g) from generate_series(1, 500000) g
union
select count(g) from generate_series(1, 500000) g
union
select count(g) from generate_series(1, 500000) g
order by 1;


-- We cannot test the per-segment limit, because changing it requires
-- a postmaster restart. It's enforced in the same way as the per-query
-- limit, though, and it's simpler, so if the per-query limit works,
-- the per-segment limit probably works too.
