-- WARNING: This file is executed against the postgres database. If
-- objects are to be manipulated in other databases, make sure to
-- change to the correct database first.

-- The pg_get_viewdef() function was fixed to properly show views
-- defined with gp_dist_random() but it will diff in this binary swap
-- test suite. Ignore by removing the view.
\c regression;
DROP VIEW IF EXISTS locktest_segments;
DROP VIEW IF EXISTS notdisview;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_float4;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_float8;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_floatdouble;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_floatreal;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_int8;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_numeric;
DROP TABLE IF EXISTS partition_ddl2.mpp3080_numericbig;
DROP TABLE IF EXISTS partition_ddl2.mpp3282_partsupp;
DROP TABLE IF EXISTS partition_pruning.pt_lt_tab;
DROP TABLE IF EXISTS partition_pruning.pt_lt_tab_df;
DROP TABLE IF EXISTS public.distrib_part_test;
DROP TABLE IF EXISTS public.dml_ao_check_r;
DROP TABLE IF EXISTS public.dml_ao_pt_p;
DROP TABLE IF EXISTS public.dml_ao_r;
DROP TABLE IF EXISTS public.dml_co_check_r;
DROP TABLE IF EXISTS public.dml_co_r;
DROP TABLE IF EXISTS public.dml_heap_check_r;
DROP TABLE IF EXISTS public.dml_heap_pt_p;
DROP TABLE IF EXISTS public.dml_heap_r;
DROP TABLE IF EXISTS public.dml_union_r;
DROP TABLE IF EXISTS public.dml_union_s;
DROP TABLE IF EXISTS public.insert_tbl;
DROP VIEW IF EXISTS public.locktest_master;
DROP VIEW IF EXISTS public.locktest_segments_dist;
DROP TABLE IF EXISTS public.pfoo;
DROP MATERIALIZED VIEW IF EXISTS public.table_relfilenode;
DROP TABLE IF EXISTS public.test_cc;
DROP TABLE IF EXISTS public.volatilefn_dml_int8_candidate;
DROP TABLE IF EXISTS qp_dml_oids.dml_ao;
DROP TABLE IF EXISTS qp_dml_oids.dml_heap_r;
DROP TABLE IF EXISTS partition_ddl2.mpp3304_customer;
DROP TABLE IF EXISTS public.aopart_orders;
DROP TABLE IF EXISTS public.dml_co_pt_p;
DROP TABLE IF EXISTS public.multi_segfile_parttab;
DROP TABLE IF EXISTS public.orders;
DROP TABLE IF EXISTS public.partsupp_1;
DROP TABLE IF EXISTS public.t_ao_alias_31345_partsupp;
DROP TABLE IF EXISTS public.volatilefn_dml_int8;
DROP TABLE IF EXISTS qp_query_execution.lossmithe_colstor;
DROP TABLE IF EXISTS public.mergeappend_test;
DROP TABLE IF EXISTS public.multivarblock_parttab;
DROP TABLE IF EXISTS public.aopart_lineitem CASCADE;

-- Drop tables carrying rle with zstd compression. That is not available in
-- earlier minor versions.
DROP TABLE co_rle_zstd1;
DROP TABLE co_rle_zstd3;
DROP TABLE co6;

\c isolation2test;
DROP VIEW IF EXISTS locktest_segments;
DROP VIEW IF EXISTS locktest_master;
DROP VIEW IF EXISTS locktest_segments_dist;
DROP TABLE IF EXISTS pg_partitions_ddl_tab;
DROP VIEW IF EXISTS show_locks_lockmodes;
