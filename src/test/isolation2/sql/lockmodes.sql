-- table to just store the master's data directory path on segment.
CREATE TABLE lockmodes_datadir(a int, dir text);
INSERT INTO lockmodes_datadir select 1,datadir from gp_segment_configuration where role='p' and content=-1;

1: set optimizer = off;

create or replace view show_locks_lockmodes as
  select locktype, mode, granted, relation::regclass
  from pg_locks
  where
    gp_segment_id = -1 and
    locktype = 'relation' and
    relation::regclass::text like 't_lockmods%';

show gp_enable_global_deadlock_detector;


-- 1. The firs part of test is with
--    gp_enable_global_deadlock_detector off

-- 1.1 test for heap tables
create table t_lockmods (c int) distributed randomly;
insert into t_lockmods select * from generate_series(1, 5);
analyze t_lockmods;

create table t_lockmods1 (c int) distributed randomly;

create table t_lockmods_rep(c int) distributed replicated;

-- See github issue: https://github.com/greenplum-db/gpdb/issues/9449
-- upsert may lock tuples on segment, so we should upgrade lock level
-- on QD if GDD is disabled.
create table t_lockmods_upsert(a int, b int) distributed by (a);
create unique index uidx_t_lockmodes_upsert on t_lockmods_upsert(a, b);
-- add analyze to avoid auto vacuum when executing first insert
analyze t_lockmods_upsert;

-- 1.1.1 select for (update|share|key share|no key update) should hold ExclusiveLock on range tables
1: begin;
1: explain select * from t_lockmods for update;
1: select * from t_lockmods for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods for no key update;
1: select * from t_lockmods for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods for share;
1: select * from t_lockmods for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods for key share;
1: select * from t_lockmods for key share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for update;
1: select * from t_lockmods, t_lockmods1 for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for no key update;
1: select * from t_lockmods, t_lockmods1 for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for share;
1: select * from t_lockmods, t_lockmods1 for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for key share;
1: select * from t_lockmods, t_lockmods1 for key share;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.1.2 update | delete should hold ExclusiveLock on result relations
1: begin;
1: update t_lockmods set c = c + 0;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: delete from t_lockmods;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.1.3 insert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods select * from generate_series(1, 5);
2: select * from show_locks_lockmodes;
1: abort;

-- 1.1.4 upsert should hold ExclusiveLock on result relations
1: begin;
1: insert into t_lockmods_upsert values (1, 1) on conflict(a, b) do update set b = 99;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.1.5 use cached plan should be consistent with no cached plan
1: prepare select_for_update as select * from t_lockmods for update;
1: prepare select_for_nokeyupdate as select * from t_lockmods for no key update;
1: prepare select_for_share as select * from t_lockmods for share;
1: prepare select_for_keyshare as select * from t_lockmods for key share;
1: prepare update_tlockmods as update t_lockmods set c = c + 0;
1: prepare delete_tlockmods as delete from t_lockmods;
1: prepare insert_tlockmods as insert into t_lockmods select * from generate_series(1, 5);
1: prepare upsert_tlockmods as insert into t_lockmods_upsert values (1, 1) on conflict(a, b) do update set b = 99;

1: begin;
1: execute select_for_update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_nokeyupdate;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_keyshare;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute update_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute delete_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute insert_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute upsert_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.2 test for AO table
create table t_lockmods_ao (c int) with (appendonly=true) distributed randomly;
insert into t_lockmods_ao select * from generate_series(1, 8);
analyze t_lockmods_ao;
create table t_lockmods_ao1 (c int) with (appendonly=true) distributed randomly;

-- 1.2.1 select for (update|share|key share|no key update) should hold ExclusiveLock on range tables
1: begin;
1: explain select * from t_lockmods_ao for update;
1: select * from t_lockmods_ao for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao for no key update;
1: select * from t_lockmods_ao for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao for share;
1: select * from t_lockmods_ao for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao for key share;
1: select * from t_lockmods_ao for key share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for update;
1: select * from t_lockmods_ao, t_lockmods_ao1 for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for no key update;
1: select * from t_lockmods_ao, t_lockmods_ao1 for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for share;
1: select * from t_lockmods_ao, t_lockmods_ao1 for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for key share;
1: select * from t_lockmods_ao, t_lockmods_ao1 for key share;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.2.2 update | delete should hold ExclusiveLock on result relations
1: begin;
1: update t_lockmods_ao set c = c + 0;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: delete from t_lockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.2.3 insert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods_ao select * from generate_series(1, 5);
2: select * from show_locks_lockmodes;
1: abort;

-- 1.2.4 use cached plan should be consistent with no cached plan
1: prepare select_for_update_ao as select * from t_lockmods_ao for update;
1: prepare select_for_nokeyupdate_ao as select * from t_lockmods_ao for no key update;
1: prepare select_for_share_ao as select * from t_lockmods_ao for share;
1: prepare select_for_keyshare_ao as select * from t_lockmods_ao for key share;
1: prepare update_tlockmods_ao as update t_lockmods_ao set c = c + 0;
1: prepare delete_tlockmods_ao as delete from t_lockmods_ao;
1: prepare insert_tlockmods_ao as insert into t_lockmods_ao select * from generate_series(1, 5);

1: begin;
1: execute select_for_update_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_nokeyupdate_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_share_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_keyshare_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute update_tlockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute delete_tlockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute insert_tlockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.3 With limit clause, such case should
-- acquire ExclusiveLock on the whole table and do not generate lockrows node
1: begin;
1: explain select * from t_lockmods order by c limit 1 for update;
1: select * from t_lockmods order by c limit 1 for update;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.4 For replicated table, we should lock the entire table on ExclusiveLock
1: begin;
1: explain select * from t_lockmods_rep for update;
1: select * from t_lockmods_rep for update;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.5 test order-by's plan
1: begin;
1: explain select * from t_lockmods order by c for update;
1: select * from t_lockmods order by c for update;
2: select * from show_locks_lockmodes;
1: abort;

-- 1.6 select for update NOWAIT/SKIP LOCKED
-- NOWAIT/SKIP LOCKED should not affect the table-level lock
1: begin;
1: select * from t_lockmods for share;
2&: select * from t_lockmods for update nowait;
1: abort;
2<:

1: begin;
1: select * from t_lockmods for share;
2&: select * from t_lockmods for update skip locked;
1: abort;
2<:

1q:
2q:

-- 1.8 Test on DML lock behavior on Partition tables on QDs.
-- This suite will test:
--   * DML on root
--   * DML on one specific leaf
-- For detailed behavior and notes, please refer below
-- cases's comments.
-- Details: https://groups.google.com/a/greenplum.org/g/gpdb-dev/c/wAPKpJzhbpM
-- Issue: https://github.com/greenplum-db/gpdb/issues/13652
1:DROP TABLE IF EXISTS t_lockmods_part_tbl_dml;

1:CREATE TABLE t_lockmods_part_tbl_dml (a int, b int, c int) PARTITION BY RANGE(b) (START(1) END(3) EVERY(1));
1:INSERT INTO t_lockmods_part_tbl_dml SELECT i, 1, i FROM generate_series(1,10)i;

-- 
1: BEGIN;
1: DELETE FROM t_lockmods_part_tbl_dml;
-- on QD, there's a lock on the root and the target partition
1: select * from show_locks_lockmodes;
1: ROLLBACK;

1: BEGIN;
1: INSERT INTO t_lockmods_part_tbl_dml SELECT i, 1, i FROM generate_series(1,10)i;
-- without GDD, it will lock all leaf partitions on QD
1: select * from show_locks_lockmodes;
1: ROLLBACK;

--
-- The session cannot be reused.
--
-- The macro RELCACHE_FORCE_RELEASE is defined iff USE_ASSERT_CHECKING is
-- defined, and when RELCACHE_FORCE_RELEASE is defined the relcache is
-- forcefully released when closing the relation.
--
-- The function generate_partition_qual() will behave differently depends on
-- the existence of the relcache.
--
-- - if the relation is not cached, it will open it in AccessShareLock mode,
--   and save the relpartbound in the relcache;
-- - if the relation is already cached, it will load the relpartbound from the
--   cache directly without opening the relation;
--
-- So as a result, in the following transactions we will see an extra
-- AccessShareLock lock in a --enable-cassert build compared to a
-- --disable-cassert build.
--
-- To make the test results stable, we do not reuse the sessions in the test,
-- all the tests are performed without the relcache.
1q:

1: BEGIN;
1: UPDATE t_lockmods_part_tbl_dml SET c = 1 WHERE c = 1;
-- on QD, there's a lock on the root and the target partition
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

1: BEGIN;
1: DELETE FROM t_lockmods_part_tbl_dml_1_prt_1;
-- since the delete operation is on leaf part, there will be a lock on QD
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

1: BEGIN;
1: UPDATE t_lockmods_part_tbl_dml_1_prt_1 SET c = 1 WHERE c = 1;
-- since the update operation is on leaf part, there will be a lock on QD
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

-- enable gdd
ALTER SYSTEM SET gp_enable_global_deadlock_detector TO on;
-- Use utility session on seg 0 to restart master. This way avoids the
-- situation where session issuing the restart doesn't disappear
-- itself.
1U:SELECT pg_ctl(dir, 'restart') from lockmodes_datadir;

1: show gp_enable_global_deadlock_detector;

1: set optimizer = off;

2: show gp_enable_global_deadlock_detector;

-- 2. The firs part of test is with
--    gp_enable_global_deadlock_detector on

-- 2.1 test for heap tables

-- 2.1.1 select for (update|share|no key update |key share) should hold ExclusiveLock on range tables
1: begin;
1: explain select * from t_lockmods for update;
1: select * from t_lockmods for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods for no key update;
1: select * from t_lockmods for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods for share;
1: select * from t_lockmods for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods for key share;
1: select * from t_lockmods for key share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for update;
1: select * from t_lockmods, t_lockmods1 for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for no key update;
1: select * from t_lockmods, t_lockmods1 for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for share;
1: select * from t_lockmods, t_lockmods1 for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods, t_lockmods1 for key share;
1: select * from t_lockmods, t_lockmods1 for key share;
2: select * from show_locks_lockmodes;
1: abort;


-- 2.1.2 update | delete should hold RowExclusiveLock on result relations
1: begin;
1: update t_lockmods set c = c + 0;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: delete from t_lockmods;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.1.3 insert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods select * from generate_series(1, 5);
2: select * from show_locks_lockmodes;
1: abort;

-- 2.1.4 upsert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods_upsert values (1, 1) on conflict(a, b) do update set b = 99;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.1.5 use cached plan should be consistent with no cached plan
1: prepare select_for_update as select * from t_lockmods for update;
1: prepare select_for_nokeyupdate as select * from t_lockmods for no key update;
1: prepare select_for_share as select * from t_lockmods for share;
1: prepare select_for_keyshare as select * from t_lockmods for key share;
1: prepare update_tlockmods as update t_lockmods set c = c + 0;
1: prepare delete_tlockmods as delete from t_lockmods;
1: prepare insert_tlockmods as insert into t_lockmods select * from generate_series(1, 5);
1: prepare upsert_tlockmods as insert into t_lockmods_upsert values (1, 1) on conflict(a, b) do update set b = 99;

1: begin;
1: execute select_for_update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_nokeyupdate;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_keyshare;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute update_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute delete_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute insert_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute upsert_tlockmods;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.2 test for AO table

-- 2.2.1 select for (update|share|key share|no key update) should hold ExclusiveLock on range tables
1: begin;
1: explain select * from t_lockmods_ao for update;
1: select * from t_lockmods_ao for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao for no key update;
1: select * from t_lockmods_ao for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao for share;
1: select * from t_lockmods_ao for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao for key share;
1: select * from t_lockmods_ao for key share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for update;
1: select * from t_lockmods_ao, t_lockmods_ao1 for update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for no key update;
1: select * from t_lockmods_ao, t_lockmods_ao1 for no key update;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for share;
1: select * from t_lockmods_ao, t_lockmods_ao1 for share;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: explain select * from t_lockmods_ao, t_lockmods_ao1 for key share;
1: select * from t_lockmods_ao, t_lockmods_ao1 for key share;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.2.2 update | delete should hold ExclusiveLock on result relations
1: begin;
1: update t_lockmods_ao set c = c + 0;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: delete from t_lockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.2.3 insert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods_ao select * from generate_series(1, 5);
2: select * from show_locks_lockmodes;
1: abort;

-- 2.2.4 use cached plan should be consistent with no cached plan
1: prepare select_for_update_ao as select * from t_lockmods_ao for update;
1: prepare select_for_nokeyupdate_ao as select * from t_lockmods_ao for no key update;
1: prepare select_for_share_ao as select * from t_lockmods_ao for share;
1: prepare select_for_keyshare_ao as select * from t_lockmods_ao for key share;
1: prepare update_tlockmods_ao as update t_lockmods_ao set c = c + 0;
1: prepare delete_tlockmods_ao as delete from t_lockmods_ao;
1: prepare insert_tlockmods_ao as insert into t_lockmods_ao select * from generate_series(1, 5);

1: begin;
1: execute select_for_update_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_nokeyupdate_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_share_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute select_for_keyshare_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute update_tlockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute delete_tlockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

1: begin;
1: execute insert_tlockmods_ao;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.3 With limit clause, such case should
-- acquire ExclusiveLock on the whole table and do not generate lockrows node
-- GPDB_96_MERGE_FIXME: It's not deterministic which row this returns. See
-- 2.5 test below.
1: begin;
1: explain select 'locked' as l from t_lockmods order by c limit 1 for update;
1: select 'locked' as l from t_lockmods order by c limit 1 for update;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.4 For replicated table, we should lock the entire table on ExclusiveLock
1: begin;
1: explain select * from t_lockmods_rep for update;
1: select * from t_lockmods_rep for update;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.5 test order-by's plan
1: begin;
1: explain select * from t_lockmods order by c for update;
1: select * from t_lockmods order by c for update;
2: select * from show_locks_lockmodes;
1: abort;

-- 2.6 select for update NOWAIT/SKIP LOCKED
-- with GDD, select for update could be optimized to not upgrade lock.
1: set optimizer = off;
2: set optimizer = off;
1: begin;
1: select * from t_lockmods where c<3 for share;
2: select * from t_lockmods for share;
2: select * from t_lockmods for update skip locked;
2: select * from t_lockmods where c>=3 for update nowait;
2: select * from t_lockmods for update nowait;
1: abort;

1q:
2q:

-- 2.7 Test on DML lock behavior on Partition tables on QDs.
-- This suite will test:
--   * DML on root
--   * DML on one specific leaf
-- For detailed behavior and notes, please refer below
-- cases's comments.

1: BEGIN;
1: DELETE FROM t_lockmods_part_tbl_dml;
-- on QD, there's a lock on the root and the target partition
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

1: BEGIN;
1: UPDATE t_lockmods_part_tbl_dml SET c = 1 WHERE c = 1;
-- on QD, there's a lock on the root and the target partition
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

1: BEGIN;
1: DELETE FROM t_lockmods_part_tbl_dml_1_prt_1;
-- since the delete operation is on leaf part, there will be a lock on QD
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

1: BEGIN;
1: UPDATE t_lockmods_part_tbl_dml_1_prt_1 SET c = 1 WHERE c = 1;
-- since the update operation is on leaf part, there will be a lock on QD
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

1: BEGIN;
1: INSERT INTO t_lockmods_part_tbl_dml SELECT i, 1, i FROM generate_series(1,10)i;
-- With GDD enabled, QD will only hold lock on root for insert
1: select * from show_locks_lockmodes;
1: ROLLBACK;
1q:

-- 2.8 Verify behaviors of select with locking clause (i.e. select for update)
-- when running concurrently with index creation, for Heap tables.
-- For AO/CO tables, refer to create_index_allows_readonly.source.

1: CREATE TABLE create_index_select_for_update_tbl(a int, b int);
1: INSERT INTO create_index_select_for_update_tbl SELECT i,i FROM generate_series(1,10)i;
1: set optimizer = off;

-- 2.8.1 with GDD enabled, expect no blocking
1: show gp_enable_global_deadlock_detector;

1: BEGIN;
1: SELECT * FROM create_index_select_for_update_tbl WHERE a = 2 FOR UPDATE;

2: set optimizer = off;

2: BEGIN;
-- expect no blocking
2: CREATE INDEX create_index_select_for_update_idx ON create_index_select_for_update_tbl(a);
2: COMMIT;

1: COMMIT;

2: DROP INDEX create_index_select_for_update_idx;

2: BEGIN;
2: CREATE INDEX create_index_select_for_update_idx ON create_index_select_for_update_tbl(a);

1: BEGIN;
-- expect no blocking
1: SELECT * FROM create_index_select_for_update_tbl WHERE a = 2 FOR UPDATE;
1: COMMIT;
-- close session to avoid renew session failure after restart
1q:

2: COMMIT;

2: DROP INDEX create_index_select_for_update_idx;

-- 2.8.2 with GDD disabled, expect blocking
-- reset gdd
2: ALTER SYSTEM RESET gp_enable_global_deadlock_detector;
-- close session to avoid renew session failure after restart
2q:
1U:SELECT pg_ctl(dir, 'restart') from lockmodes_datadir;

1: set optimizer = off;
1: show gp_enable_global_deadlock_detector;

1: BEGIN;
1: SELECT * FROM create_index_select_for_update_tbl WHERE a = 2 FOR UPDATE;

2: set optimizer = off;

2: BEGIN;
-- expect blocking
2&: CREATE INDEX create_index_select_for_update_idx ON create_index_select_for_update_tbl(a);

1: COMMIT;

2<:
2: COMMIT;

2: DROP INDEX create_index_select_for_update_idx;

2: BEGIN;
2: CREATE INDEX create_index_select_for_update_idx ON create_index_select_for_update_tbl(a);

1: BEGIN;
-- expect blocking
1&: SELECT * FROM create_index_select_for_update_tbl WHERE a = 2 FOR UPDATE;

2: COMMIT;

1<:
1: COMMIT;

1: drop table lockmodes_datadir;
1q:
2q:

-- Check that concurrent DROP on leaf partition won't impact analyze on the
-- parent since analyze will hold a ShareUpdateExclusiveLock and DROP will 
-- require an AccessExclusiveLock.
-- Case 1. The analyze result is expected when there's concurrent drop on child.
1:create table analyzedrop(a int) partition by range(a);
1:create table analyzedrop_1 partition of analyzedrop for values from (0) to (10);
1:create table analyzedrop_2 partition of analyzedrop for values from (10) to (20);
1:insert into analyzedrop select * from generate_series(0,19);
1:select gp_inject_fault_infinite('merge_leaf_stats_after_find_children', 'suspend', dbid) from gp_segment_configuration where content = -1 and role = 'p';
1&: analyze analyzedrop; 
2&: drop table analyzedrop_1;
3:select gp_inject_fault_infinite('merge_leaf_stats_after_find_children', 'reset', dbid) from gp_segment_configuration where content = -1 and role = 'p';
1<:
2<:
3:select * from pg_stats where tablename like 'analyzedrop%';
-- Case 2. No failure should happen when there's concurrent drop on parent as well.
1:select gp_inject_fault_infinite('merge_leaf_stats_after_find_children', 'suspend', dbid) from gp_segment_configuration where content = -1 and role = 'p';
1&: analyze analyzedrop; 
2&: drop table analyzedrop_2;
3&: drop table analyzedrop;
4:select gp_inject_fault_infinite('merge_leaf_stats_after_find_children', 'reset', dbid) from gp_segment_configuration where content = -1 and role = 'p';
1<:
2<:
3<:
--empty as table is dropped
4:select * from pg_stats where tablename like 'analyzedrop%';
