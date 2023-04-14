-- GUC idle_in_transaction_session_timeout MUST not take effect on QE,
-- this test guard that.
-- In this test, DECLARE cursor will spawn a write gang
-- and a read gang. And we set idle_in_transaction_session_timeout
-- to 1s, when FETCH is executed, the read gang will sleep 2s because
-- of the fault injection. However, without the fix, the write gang will be
-- terminated 1s later when FETCH is issued due to the timeout of
-- idle_in_transaction_session_timeout. So when the reader is going to read the
-- shared snapshot, ERROR will be raised.

CREATE TABLE t_idle_trx_timeout (a int) DISTRIBUTED BY(a);
INSERT INTO t_idle_trx_timeout VALUES (2),(3);
SELECT gp_segment_id, * FROM t_idle_trx_timeout;

SET idle_in_transaction_session_timeout = 1000;
SELECT gp_inject_fault('before_read_shared_snapshot_for_cursor', 'sleep', '', '', '', 1, 1, 2, dbid)
 FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
BEGIN;
DECLARE cur CURSOR FOR SELECT * FROM t_idle_trx_timeout;
FETCH cur;
FETCH cur;
END;

SELECT gp_inject_fault('before_read_shared_snapshot_for_cursor', 'reset', dbid) 
 FROM gp_segment_configuration WHERE content = 0 AND role = 'p';

DROP TABLE t_idle_trx_timeout;

