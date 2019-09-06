-- Ensures that deadlock between an INSERT with planner and
-- appendonly VACUUM drop phase can be detected.
-- ORCA during translation of INSERT statement tries to get AccessShareLock on
-- leaf partition but Vacuum already has AccessExclusiveLock on the leaf
-- partition so it keeps waiting and the deadlock is not observed.
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration;

-- Helper function
CREATE or REPLACE FUNCTION wait_until_acquired_lock_on_rel (rel_name text, lmode text, segment_id integer) RETURNS /*in func*/
  bool AS $$ /*in func*/
declare /*in func*/
  result bool; /*in func*/
begin /*in func*/
  result := false; /*in func*/
  -- Wait until lock is acquired /*in func*/
  while result IS NOT true loop /*in func*/
    select l.granted INTO result /*in func*/
    from pg_locks l /*in func*/
    where l.relation::regclass = rel_name::regclass /*in func*/
      and l.mode=lmode /*in func*/
      and l.gp_segment_id=segment_id; /*in func*/
    perform pg_sleep(0.1); /*in func*/
  end loop; /*in func*/
  return result; /*in func*/
end; /*in func*/
$$ language plpgsql;

-- Given an append only table with partitions that is ready to be compacted
CREATE TABLE ao_part_tbl (a int, b int) with (appendonly=true, orientation=row)
  DISTRIBUTED BY (a)
  PARTITION BY RANGE (b)
(START (1) END (2) EVERY (1));

INSERT INTO ao_part_tbl VALUES (1, 1);
DELETE FROM ao_part_tbl;

-- Check gdd is enabled
show gp_enable_global_deadlock_detector;

-- VACUUM drop phase is blocked before it opens the child relation on the primary 
SELECT gp_inject_fault('vacuum_relation_open_relation_during_drop_phase', 'suspend', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- VACUUM takes AccessExclusiveLock on the leaf partition on QD
1&: VACUUM ao_part_tbl;
SELECT gp_wait_until_triggered_fault('vacuum_relation_open_relation_during_drop_phase', 1, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- And INSERT is blocked until it acquires the RowExclusiveLock on the child relation
2: set optimizer to off;
-- INSERT takes RowExclusiveLock on the leaf partition on QE
2&: INSERT INTO ao_part_tbl VALUES (1, 1);
SELECT wait_until_acquired_lock_on_rel('ao_part_tbl_1_prt_1', 'RowExclusiveLock', content) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reset the fault on VACUUM. 
SELECT gp_inject_fault('vacuum_relation_open_relation_during_drop_phase', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- VACUUM waits for AccessExclusiveLock on the leaf table on QE
1<:
-- INSERT watis for AccessShareLock on the leaf table on QD, hence deadlock
2<:

-- since gdd is on, Session 2 will be cancelled.

1:ROLLBACK;
2:ROLLBACK;
DROP TABLE IF EXISTS ao_part_tbl;
