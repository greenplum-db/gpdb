-- Test the scenario when the VACUUM FULL is interrupted on segment after
-- 'swap_relation_files' is finished.

-- There was a bug that swap_relation_files inplace update the old entry in the
-- pg_class and the pg_class entry has incorrect relfrozenxid after the
-- transaction is aborted.

1: CREATE TABLE vacuum_full_interrupt(a int, b int, c int);
1: CREATE INDEX vacuum_full_interrupt_idx on vacuum_full_interrupt(b);
1: INSERT INTO vacuum_full_interrupt SELECT i, i, i from generate_series(1,100)i;
1: ANALYZE vacuum_full_interrupt;
-- the relfrozenxid is the same as xmin when there's concurrent transactions.
-- the reltuples is 100
1: SELECT xmin=relfrozenxid relfrozenxid_not_changed, relhasindex, reltuples FROM pg_class WHERE relname='vacuum_full_interrupt';

-- break on QE after 'swap_relation_files' is finished
1: SELECT gp_inject_fault('after_swap_relation_files', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;
2&: VACUUM FULL vacuum_full_interrupt;
1: SELECT gp_wait_until_triggered_fault('after_swap_relation_files', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0; 

-- cancel VACUUM FULL
1: SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE query = 'VACUUM FULL vacuum_full_interrupt;';
1: SELECT gp_inject_fault('after_swap_relation_files', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;
2<:

-- the relfrozenxid should stay unchanged
-- the reltuples should be 100, but QD has already commit the transaction and the reltuples is updated to 0, this looks like a bug
2: SELECT xmin=relfrozenxid relfrozenxid_not_changed, relhasindex, reltuples FROM pg_class WHERE relname='vacuum_full_interrupt';
0U: SELECT xmin=relfrozenxid relfrozenxid_not_changed, relhasindex, reltuples FROM pg_class WHERE relname='vacuum_full_interrupt';

-- verify the index is correctly when insert new tuples, in bug also reset 'relhasindex' in pg_class.
2: INSERT INTO vacuum_full_interrupt SELECT i, i, i from generate_series(1,100)i;
2: SET optimizer=off;
2: SET enable_seqscan=off;
2: SET enable_bitmapscan=off;
2: SET enable_indexscan=on;
2: EXPLAIN SELECT * FROM vacuum_full_interrupt WHERE b=2;
2: SELECT * FROM vacuum_full_interrupt WHERE b=2;
