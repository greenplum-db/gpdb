DROP TABLE IF EXISTS t108;
CREATE TABLE t108 (id int, val int);
INSERT INTO t108 (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;
30: RESET optimizer;

10: BEGIN;
20: BEGIN;
30: BEGIN;

10: INSERT INTO t108 VALUES(segid(1,1),segid(1,1));

20: INSERT INTO t108 VALUES(segid(1,1),segid(1,1));

30: INSERT INTO t108 VALUES(segid(1,1),segid(1,1));

-- ANALYZE holds ShareUpdateExclusiveLock, they conflict with each other.
-- But they are not conflict with INSERT, So GDD graph on seg1 should be
-- 20 -> 10 and 30 -> 10 and no cycle in this case.
10: ANALYZE t108;

20&: ANALYZE t108;

30&: ANALYZE t108;

SELECT pg_sleep(20);

-- con10/20/30 should finish normally.
10q:

20<:
20q:

30<:
30q:

