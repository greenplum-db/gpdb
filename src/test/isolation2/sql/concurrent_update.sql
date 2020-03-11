-- Test concurrent update a table with a varying length type
CREATE TABLE t_concurrent_update(a int, b int, c char(84));
INSERT INTO t_concurrent_update VALUES(1,1,'test');

1: BEGIN;
1: SET optimizer=off;
1: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
2: SET optimizer=off;
2&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
1: END;
2<:
1: SELECT * FROM t_concurrent_update;
1q:
2q:

DROP TABLE t_concurrent_update;


-- Test the concurrent update transaction order on the segment is reflected on master
-- enable gdd
--start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v on;
! gpstop -rai;
--end_ignore

1: SHOW gp_enable_global_deadlock_detector;
1: CREATE TABLE t_concurrent_update(a int, b int);
1: INSERT INTO t_concurrent_update VALUES(1,1);

2: BEGIN;
2: SET optimizer=off;
2: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
3: BEGIN;
3: SET optimizer=off;
-- transaction 3 will wait transaction 1 on the segment
3&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;

-- transaction 2 suspend before commit, but it will wake up transaction 3 on segment
2: select gp_inject_fault('before_xact_end_procarray', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
2&: END;
1: select gp_wait_until_triggered_fault('before_xact_end_procarray', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
-- transaction 3 should wait transaction 2 commit on master
3<:
3&: END;
1: select gp_inject_fault('before_xact_end_procarray', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
-- the query should not get the incorrect distributed snapshot: transaction 1 in-progress
-- and transaction 2 finished
1: SELECT * FROM t_concurrent_update;
2<:
3<:
2q:
3q:

1: SELECT * FROM t_concurrent_update;
1: DROP TABLE t_concurrent_update;

--start_ignore
! gpconfig -r gp_enable_global_deadlock_detector;
! gpstop -rai;
--end_ignore 
1q:
