DROP TABLE IF EXISTS t102;
CREATE TABLE t102 (id int, val int);
INSERT INTO t102 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;

30: UPDATE t102 SET val=30 WHERE id=segid(1,1);

10: UPDATE t102 SET val=10 WHERE id=segid(0,1);

20: UPDATE t102 SET val=10 WHERE id=segid(1,2);
-- seg 0: con20 ==> con10, xid lock
-- seg 1: con20 ==> con30, xid lock
20&: UPDATE t102 SET val=20 WHERE id IN (segid(0,1), segid(1,1));

-- seg 1: con10 ==> con20, xid lock
10>: UPDATE t102 SET val=10 WHERE id=segid(1,2);

-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
