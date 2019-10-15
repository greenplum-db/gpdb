DROP TABLE IF EXISTS t06;
CREATE TABLE t06 (id int, val int);
INSERT INTO t06 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;

10: BEGIN;
20: BEGIN;

10: UPDATE t06 SET val=7 WHERE val=segid(0,1);

20: UPDATE t06 SET val=8 WHERE val=segid(1,1);

-- seg 1: con10 ==> con20, xid lock
10&: UPDATE t06 SET val=8 WHERE val=segid(1,1);

-- seg 0: con20 ==> con10, xid lock
20>: UPDATE t06 SET val=7 WHERE val=segid(0,1);

-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
