include: helpers/server_helpers.sql;

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

2: ALTER SYSTEM RESET gp_enable_global_deadlock_detector;
1U:SELECT pg_ctl(dir, 'restart') from lockmodes_datadir;

1: show gp_enable_global_deadlock_detector;

1: drop table lockmodes_datadir;
1q:
