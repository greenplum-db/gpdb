DROP TABLE IF EXISTS t07a;
DROP TABLE IF EXISTS t07b;
CREATE TABLE t07a (c1 int, c2 int);
CREATE TABLE t07b (c1 int, c2 int);
INSERT INTO t07a (c1, c2) SELECT i, i FROM generate_series(1, 100) i;
INSERT INTO t07b (c1, c2) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;
40: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;
40: BEGIN;

10: UPDATE t07b SET c2 = 11 WHERE c1 = segid(0,1);

20: UPDATE t07b SET c2 = 22 WHERE c1 = segid(1,1);

30: LOCK t07a;
-- seg 0: con30 ==> con10, xid lock
30&: UPDATE t07b SET c2 = 21 WHERE c1 = segid(0,1);

-- seg 1: con10 ==> con20, xid lock
10&: UPDATE t07b SET c2 = 12 WHERE c1 = segid(1,1);

40: UPDATE t07b SET c2 = 13 WHERE c1 = segid(0,2);
-- seg -1: con40 ==> con30, relation lock
40&: LOCK t07a;

-- set 0: con20 ==> con40, xid lock
20>: UPDATE t07b SET c2 = 23 WHERE c1 = segid(0,2);

-- con40 will be cancelled by gdd
40<:
40q:

-- no more deadlock
20<:
20q:
10<:
10q:
30<:
30q:
