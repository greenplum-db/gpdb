DROP TABLE IF EXISTS t103;
CREATE TABLE t103 (id int, val int);
INSERT INTO t103 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;

10: UPDATE t103 SET val=val WHERE id=segid(0,1);
10: UPDATE t103 SET val=val WHERE id=segid(1,2);

20: UPDATE t103 SET val=val WHERE id=segid(1,1);
-- seg 0: con20 ==> con10, xid lock
20&: UPDATE t103 SET val=val WHERE id=segid(0,1);

-- seg 1: con30 ==> con10, xid lock
30>: UPDATE t103 SET val=val WHERE id=segid(1,2);

-- seg 1: con10 ==> con20, xid lock
10>: UPDATE t103 SET val=val WHERE id=segid(1,1);

-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
30<:
30q:
