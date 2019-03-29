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

-- 1.1.1 select for update should hold ExclusiveLock on range tables
1: begin;
1: select * from t_lockmods for update;

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

-- 1.1.4 use cached plan should be consistent with no cached plan
1: prepare select_for_update as select * from t_lockmods for update;
1: prepare update_tlockmods as update t_lockmods set c = c + 0;
1: prepare delete_tlockmods as delete from t_lockmods;
1: prepare insert_tlockmods as insert into t_lockmods select * from generate_series(1, 5);

1: begin;
1: execute select_for_update;

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

-- 1.2 test for AO table
create table t_lockmods_ao (c int) with (appendonly=true) distributed randomly;
insert into t_lockmods_ao select * from generate_series(1, 8);

-- 1.2.1 select for update should hold ExclusiveLock on range tables
1: begin;
1: select * from t_lockmods_ao for update;

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
1: prepare update_tlockmods_ao as update t_lockmods_ao set c = c + 0;
1: prepare delete_tlockmods_ao as delete from t_lockmods_ao;
1: prepare insert_tlockmods_ao as insert into t_lockmods_ao select * from generate_series(1, 5);

1: begin;
1: execute select_for_update_ao;

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

-- 1.3 test for partition table
create table t_lockmods_part (c int) distributed randomly partition by range (c) (start(1)  end (5) every (1),  default partition extra);
insert into t_lockmods_part select * from generate_series(1, 8);

-- 1.3.1 select for update should hold ExclusiveLock on range tables
1: begin;
1: select * from t_lockmods_part for update;

2: select * from show_locks_lockmodes;

1: abort;

-- 1.3.2 update | delete should hold ExclusiveLock on result relations
1: begin;
1: update t_lockmods_part set c = c + 0;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: delete from t_lockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

-- 1.3.3 insert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods_part select * from generate_series(1, 5) ;

2: select * from show_locks_lockmodes;

1: abort;

-- 1.3.4 use cached plan should be consistent with no cached plan
1: prepare select_for_update_part as select * from t_lockmods_part for update;
1: prepare update_tlockmods_part as update t_lockmods_part set c = c + 0;
1: prepare delete_tlockmods_part as delete from t_lockmods_part;
1: prepare insert_tlockmods_part as insert into t_lockmods_part select * from generate_series(1, 5);

1: begin;
1: execute select_for_update_part;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: execute update_tlockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: execute delete_tlockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: execute insert_tlockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

1q:
2q:

-- start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v on;
! gpstop -rai;
-- end_ignore

1: set optimizer = off;

2: show gp_enable_global_deadlock_detector;

-- 2. The firs part of test is with
--    gp_enable_global_deadlock_detector on

-- 2.1 test for heap tables

-- 2.1.1 select for update should hold ExclusiveLock on range tables
1: begin;
1: select * from t_lockmods for update;

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

-- 2.1.4 use cached plan should be consistent with no cached plan
1: prepare select_for_update as select * from t_lockmods for update;
1: prepare update_tlockmods as update t_lockmods set c = c + 0;
1: prepare delete_tlockmods as delete from t_lockmods;
1: prepare insert_tlockmods as insert into t_lockmods select * from generate_series(1, 5);

1: begin;
1: execute select_for_update;

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

-- 2.2 test for AO table

-- 2.2.1 select for update should hold ExclusiveLock on range tables
1: begin;
1: select * from t_lockmods_ao for update;

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
1: prepare update_tlockmods_ao as update t_lockmods_ao set c = c + 0;
1: prepare delete_tlockmods_ao as delete from t_lockmods_ao;
1: prepare insert_tlockmods_ao as insert into t_lockmods_ao select * from generate_series(1, 5);

1: begin;
1: execute select_for_update_ao;

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

-- 2.3 test for partition table

-- 2.3.1 select for update should hold ExclusiveLock on range tables
1: begin;
1: select * from t_lockmods_part for update;

2: select * from show_locks_lockmodes;

1: abort;


-- 2.3.2 update | delete should hold RowExclusiveLock on result relations
1: begin;
1: update t_lockmods_part set c = c + 0;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: delete from t_lockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

-- 2.3.3 insert should hold RowExclusiveLock on result relations
1: begin;
1: insert into t_lockmods_part select * from generate_series(1, 5);

2: select * from show_locks_lockmodes;

1: abort;

-- 1.3.4 use cached plan should be consistent with no cached plan
1: prepare select_for_update_part as select * from t_lockmods_part for update;
1: prepare update_tlockmods_part as update t_lockmods_part set c = c + 0;
1: prepare delete_tlockmods_part as delete from t_lockmods_part;
1: prepare insert_tlockmods_part as insert into t_lockmods_part select * from generate_series(1, 5);

1: begin;
1: execute select_for_update_part;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: execute update_tlockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: execute delete_tlockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

1: begin;
1: execute insert_tlockmods_part;

2: select * from show_locks_lockmodes;

1: abort;

1q:
2q:

-- start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v off;
! gpstop -rai;
-- end_ignore

1: show gp_enable_global_deadlock_detector;
1q:
