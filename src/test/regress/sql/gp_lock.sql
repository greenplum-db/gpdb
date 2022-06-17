CREATE TABLE gp_lock_test (a int);

BEGIN;
LOCK TABLE gp_lock_test IN ACCESS SHARE MODE;

SELECT l.gp_segment_id, c.relname, l.mode, l.granted
FROM pg_locks l, pg_class c WHERE l.relation = c.oid and c.relname = 'gp_lock_test';
ROLLBACK;

BEGIN;
LOCK TABLE gp_lock_test IN ACCESS SHARE MODE MASTER ONLY;

SELECT l.gp_segment_id, c.relname, l.mode, l.granted
FROM pg_locks l, pg_class c WHERE l.relation = c.oid and c.relname = 'gp_lock_test';
ROLLBACK;

-- other modes are not supported
BEGIN;
LOCK TABLE gp_lock_test IN EXCLUSIVE MODE MASTER ONLY;
ROLLBACK;

DROP TABLE gp_lock_test;
