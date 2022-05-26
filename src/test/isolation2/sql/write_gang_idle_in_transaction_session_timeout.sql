-- GUC idle_in_transaction_session_timeout MUST not take effect on QE,
-- this test guard that.
-- In this test, session 2 uses a cursor, which will spawn a write gang
-- and a read gang. And we set idle_in_transaction_session_timeout
-- to 1s, when FETCH is executed, the read gang will suspend 1.5s because
-- of the fault injection. However, without the fix, the write gang will be
-- terminated 1s later when FETCH is issued due to the timeout of
-- idle_in_transaction_session_timeout. So when the reader is going to read the
-- shared snapshot, ERROR will be raised.

1: CREATE TABLE t_idle_trx_timeout (a int) DISTRIBUTED BY(a);
1: INSERT INTO t_idle_trx_timeout VALUES (2),(3);
1: SELECT gp_segment_id, * FROM t_idle_trx_timeout;

1: SELECT gp_inject_fault_infinite('before_read_shared_snapshot_for_cursor', 'suspend', dbid)
    FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
2: SET idle_in_transaction_session_timeout = 1000;
1&: SELECT gp_wait_until_triggered_fault('before_read_shared_snapshot_for_cursor', 1, dbid)
     FROM gp_segment_configuration where content =0 AND role = 'p';
2: BEGIN;
2: DECLARE cur CURSOR FOR SELECT * FROM t_idle_trx_timeout;
2&: FETCH cur;
1<:
1: SELECT pg_sleep(1.5);
1: SELECT gp_inject_fault_infinite('before_read_shared_snapshot_for_cursor', 'reset', dbid) 
    FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
2<:
2: FETCH cur;
2: END;

1: DROP TABLE t_idle_trx_timeout;

