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

-- Test work file limit number after merge PG 1GB segment

-- Ensure the queries below need to spill to disk.
set statement_mem='1 MB';
-- Also test limit on number of files (gp_workfile_limit_files_per_query)
-- The query below will generate 6 temp files:
-- 1 gpadmin gpadmin 1.0G Mar 25 23:13 pgsql_tmpLogicalTape16802.3
-- 1 gpadmin gpadmin 1.0G Mar 25 23:13 pgsql_tmpLogicalTape16802.4
-- 1 gpadmin gpadmin 247M Mar 25 23:13 pgsql_tmpLogicalTape16802.5
-- 1 gpadmin gpadmin 1.0G Mar 25 23:08 pgsql_tmpslice0_tuplestore16802.0
-- 1 gpadmin gpadmin 1.0G Mar 25 23:09 pgsql_tmpslice0_tuplestore16802.1
-- 1 gpadmin gpadmin 623M Mar 25 23:09 pgsql_tmpslice0_tuplestore16802.2
-- On GP6, the query will generate 2 temp files:
-- 1 gpadmin gpadmin 1.5G Mar 24 22:50 pgsql_tmp_slice-1_tuplestore_1_3251.16
-- 1 gpadmin gpadmin 2.2G Mar 24 22:54 pgsql_tmp_Sort_2_3251.17
-- On master, each 1 GB segment file count as work file, and the work_set->perquery->num_files count as 6.
set gp_workfile_limit_files_per_query='6';
select count(distinct g) from generate_series(1, 200000000) g;

reset gp_workfile_limit_files_per_query;
reset statement_mem;

-- We cannot test the per-segment limit, because changing it requires
-- a postmaster restart. It's enforced in the same way as the per-query
-- limit, though, and it's simpler, so if the per-query limit works,
-- the per-segment limit probably works too.
