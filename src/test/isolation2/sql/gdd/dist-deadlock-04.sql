DROP TABLE IF EXISTS t04a;
CREATE TABLE t04a (id int, val int);
INSERT INTO t04a (id, val) SELECT i, i FROM generate_series(1, 100) i;

DROP TABLE IF EXISTS t04b;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;

10: BEGIN;
20: BEGIN;

10: UPDATE t04a SET val=val WHERE id=segid(0,1);
10: CREATE TABLE t04b (id int);

20: UPDATE t04a SET val=val WHERE id=segid(1,1);

-- seg 1: con10 ==> con20, xid lock
10&: UPDATE t04a SET val=val WHERE id=segid(1,1);

-- seg -1: con20 ==> con10, xid lock
20>: CREATE TABLE t04b (id int);

-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
