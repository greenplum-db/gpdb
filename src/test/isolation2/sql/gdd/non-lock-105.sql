DROP TABLE IF EXISTS t105;
CREATE TABLE t105 (id int, val int);
INSERT INTO t105 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;

30: UPDATE t105 SET val=30 WHERE id=segid(1,1);

10: UPDATE t105 SET val=10 WHERE val=segid(0,2);

-- seg 0: con20 ==> con10, xid lock
-- seg 1: con20 ==> con30, xid lock
20&: UPDATE t105 SET val=20 WHERE val=segid(0,2) OR id=segid(1,1);

SELECT barrier();

-- seg 1: con10 ~~> con20, tuple lock
10&: UPDATE t105 SET val=10 WHERE val=segid(1,1);

-- so far there is no deadlock yet as con30 is idle in transaction
SELECT pg_sleep(20);

-- when con30 quits the edge from con10 to con20 on seg 1 will change:
-- seg 1: con10 ==> con20, xid lock
-- then a global deadlock is formed
30q:

-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
