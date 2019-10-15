DROP TABLE IF EXISTS t106;
CREATE TABLE t106 (id int, val int);
INSERT INTO t106 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;

30: UPDATE t106 SET val=30 WHERE id=segid(1,1);

10: UPDATE t106 SET val=10 WHERE val=segid(0,2);

-- seg 1: con20 ==> con30, xid lock
20&: UPDATE t106 SET val=20 WHERE val=segid(1,1);

SELECT barrier();

-- seg 1: con10 ~~> con20, tuple lock
10&: UPDATE t106 SET val=10 WHERE val=segid(1,1);

-- seg 0: con30 ==> con10, xid lock
30>: UPDATE t106 SET val=30 WHERE id=segid(0,2);

-- con30 will be cancelled by gdd
30<:
30q:

-- no more deadlock
20<:
20q:
10<:
10q:
