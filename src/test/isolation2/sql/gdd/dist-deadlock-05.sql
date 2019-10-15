DROP TABLE IF EXISTS t05;
CREATE TABLE t05 (id int primary key);

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;

10: BEGIN;
20: BEGIN;

10: INSERT INTO t05 VALUES(segid(0,1));

20: INSERT INTO t05 VALUES(segid(1,1));
-- seg 0: con20 ==> con10, xid lock
20&: INSERT INTO t05 VALUES(segid(0,1));

-- seg 1: con10 ==> con20, xid lock
10>: INSERT INTO t05 VALUES(segid(1,1));

-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
