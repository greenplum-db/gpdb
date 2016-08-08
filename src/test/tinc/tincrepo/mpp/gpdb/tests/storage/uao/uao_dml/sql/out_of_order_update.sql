-- @Description Tests that out-of-order update works fine.
-- 

-- MPP-23546.  The following update statements ensure that
-- appendonly_udpate() sees tuples out of order with respect to their
-- row numbers / tuple ids and also that the tuples seen by update
-- span more than one visimap entry.

set enable_nestloop=false;
UPDATE foo SET a = 0 FROM bar WHERE foo.b = bar.b AND foo.a = bar.a AND
(bar.a = 10 OR bar.a = 40000 OR bar.a = 20000);

-- Ensure that tuples to be updated are all from the same GPDB segment.
SELECT distinct(a) FROM foo WHERE gp_segment_id = 0 AND
foo.a IN (12, 80001, 1001) ORDER BY a;

UPDATE foo SET a = 1 FROM bar WHERE
foo.b = bar.b AND foo.a = bar.a AND
(bar.a = 12 OR bar.a = 80001 OR bar.a = 1001);
