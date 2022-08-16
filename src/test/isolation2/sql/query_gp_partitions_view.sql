-- Test that pg_partitions view does not lock a table.
-- 6X used to acquire lock on partition tables when query
-- gp_partitions view. See `expr_has_vars` in ruleutils.c
-- for more details.
create table pg_partitions_ddl_tab(a int, b int)
distributed by (a) partition by range (b)
( start (1) end (20) every(5::float) );

1: begin;
1: lock pg_partitions_ddl_tab in access exclusive mode;
1: lock pg_partitions_ddl_tab_1_prt_2 in access exclusive mode;


-- The following query should not be blocked by session 1 because no
-- locks should be held by pg_partitions view.
select tablename, partitiontablename, partitionboundary from pg_partitions where tablename like 'pg_partitions_ddl%';

1: end;
1q:
