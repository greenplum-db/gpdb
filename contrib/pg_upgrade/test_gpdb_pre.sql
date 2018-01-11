DROP TABLE IF EXISTS alter_ao_part_tables_column.sto_altap3 CASCADE;
DROP TABLE IF EXISTS alter_ao_part_tables_row.sto_altap3 CASCADE;
DROP TABLE IF EXISTS co_cr_sub_partzlib8192_1_2 CASCADE;
DROP TABLE IF EXISTS co_cr_sub_partzlib8192_1 CASCADE;
DROP TABLE IF EXISTS co_wt_sub_partrle_type8192_1_2 CASCADE;
DROP TABLE IF EXISTS co_wt_sub_partrle_type8192_1 CASCADE;
DROP TABLE IF EXISTS ao_wt_sub_partzlib8192_5 CASCADE;
DROP TABLE IF EXISTS ao_wt_sub_partzlib8192_5_2 CASCADE;
DROP TABLE IF EXISTS constraint_pt1 CASCADE;
DROP TABLE IF EXISTS constraint_pt2 CASCADE;
DROP TABLE IF EXISTS constraint_pt3 CASCADE;
DROP TABLE IF EXISTS contest_inherit CASCADE;

-- The indexes on mpp3033a partitions don't have their default names,
-- presumably because the default names are taken when the tests
-- are run. That's a problem, because in QD, pg_dump and restore will
-- create them with new, default, names, as part of the CREATE INDEX
-- command on the parent table. But when we do pg_dump and restore
-- on a QE node, it doesn't have the partition hierarchy available,
-- and will dump restore each index separately, with the original name.
DROP TABLE IF EXISTS mpp3033a CASCADE;
DROP TABLE IF EXISTS mpp3033b CASCADE;

DROP TABLE IF EXISTS mpp17707 CASCADE;

-- These partitioned tables have different indexes on different
-- partitions. pg_dump cannot currently reconstruct that situation
-- correctly.
DROP TABLE IF EXISTS mpp7635_aoi_table2 CASCADE;
DROP TABLE IF EXISTS partition_pruning.pt_lt_tab CASCADE;
DROP TABLE IF EXISTS dcl_messaging_test CASCADE;
DROP TABLE IF EXISTS my_tq_agg_opt_part CASCADE;
DROP TABLE IF EXISTS pt_indx_tab CASCADE;

-- These partitioned tables have a SERIAL column. That's also not
-- not reconstructed by pg_dump + restore correctly.
DROP TABLE IF EXISTS ao_wt_sub_partzlib8192_5_2_uncompr CASCADE;
DROP TABLE IF EXISTS ao_wt_sub_partzlib8192_5_uncompr CASCADE;
DROP TABLE IF EXISTS co_cr_sub_partzlib8192_1_2_uncompr CASCADE;
DROP TABLE IF EXISTS co_cr_sub_partzlib8192_1_uncompr CASCADE;
DROP TABLE IF EXISTS co_wt_sub_partrle_type8192_1_2_uncompr CASCADE;
DROP TABLE IF EXISTS co_wt_sub_partrle_type8192_1_uncompr CASCADE;
