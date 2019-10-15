DROP TABLE IF EXISTS t104;
CREATE TABLE t104 (id int, val int);
INSERT INTO t104 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;
40: RESET optimizer;
50: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;
40: BEGIN;
50: BEGIN;

10: UPDATE t104 SET val=val WHERE id=segid(0,1);

40: UPDATE t104 SET val=val WHERE id=segid(1,4);

30: UPDATE t104 SET val=val WHERE id IN (segid(0,3), segid(1,3));
-- seg 1: con30 ==> con40, xid lock
30&: UPDATE t104 SET val=val WHERE id=segid(1,4);

50: UPDATE t104 SET val=val WHERE id=segid(1,5);
-- seg 1: con50 ==> con30, xid lock
50&: UPDATE t104 SET val=val WHERE id=segid(1,3);

20: UPDATE t104 SET val=val WHERE id=segid(1,2);
-- seg 0: con20 ==> con10, xid lock
-- seg 1: con20 ==> con50, xid lock
20&: UPDATE t104 SET val=val WHERE id IN (segid(0,1), segid(1,5));

-- below two queries will cause two connected small deadlocks which form a big
-- deadlock, to break this big one at least two transactions should be
-- cancelled, but current logic will cancel the youngest transactions in turn,
-- so one extra transaction will be cancelled.
--
-- this might be improved in the future.

-- seg 0: con40 ==> con30, xid lock
40>: UPDATE t104 SET val=val WHERE id=segid(0,3);

-- seg 1: con10 ==> con20, xid lock
10>: UPDATE t104 SET val=val WHERE id=segid(1,2);

-- con50, con40, con20 will be cancelled by gdd
50<:
50q:
40<:
40q:
20<:
20q:

-- no more deadlock
30<:
30q:
10<:
10q:
