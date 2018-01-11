--
-- Tests for updatable cursors
-- 
-- Deterministic test runs will be a little tricky in the case of updatable
-- cursors. By definition, these cursors cannot be ordered, yet we will be
-- issuing UPDATE and DELETE based exclusively on cursor position.
--

CREATE TEMP TABLE uctest(f1 int, f2 int, f3 text) DISTRIBUTED BY (f1);
CREATE TEMP TABLE uctest2(f1 int, f2 int, f3 text) DISTRIBUTED BY (f1);
INSERT INTO uctest VALUES (1, 1, 'one'), (1, 1, 'one'), (2, 2, 'two'), (3, 3, 'three');
SELECT * FROM uctest ORDER BY 1, 2, 3;

--
-- DELETE ... WHERE CURRENT
---
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM uctest WHERE f1 = 1;
FETCH 1 FROM c1;
DELETE FROM uctest WHERE CURRENT OF c1;
-- should show deletion
SELECT * FROM uctest ORDER BY 1, 2, 3;
-- cursor did not move
FETCH ALL FROM c1;
COMMIT;
-- should still see deletion
SELECT * FROM uctest ORDER BY 1, 2, 3;

--
-- UPDATE ... WHERE CURRENT against SELECT ... FOR UPDATE
--
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM uctest WHERE f1 = 2 FOR UPDATE;
FETCH 1 FROM c1;
UPDATE uctest SET f2 = 8 WHERE CURRENT OF c1;
SELECT * FROM uctest ORDER BY 1, 2, 3;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

-- 
-- UPDATE ... WHERE CURRENT against SELECT ... FOR SHARE
--
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM uctest WHERE f1 = 3 FOR SHARE;
FETCH 1 FROM c1;
UPDATE uctest SET f2 = 9 WHERE CURRENT OF c1;
SELECT * FROM uctest ORDER BY 1, 2, 3;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

--
-- Scan paths
--
CREATE INDEX uctest_index ON uctest USING btree(f1);

SET enable_tidscan=on; SET enable_seqscan=off; SET enable_indexscan=off;
BEGIN;
DECLARE a CURSOR FOR SELECT * FROM uctest WHERE f1 = 3;
FETCH 1 FROM a;
UPDATE uctest SET f2 = 10 WHERE CURRENT OF a;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

SET enable_tidscan=off; SET enable_seqscan=on; SET enable_indexscan=off;
BEGIN;
DECLARE a CURSOR FOR SELECT * FROM uctest WHERE f1 = 3;
FETCH 1 FROM a;
UPDATE uctest SET f2 = 11 WHERE CURRENT OF a;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

SET enable_tidscan=off; SET enable_seqscan=off; SET enable_indexscan=on;
BEGIN;
DECLARE a CURSOR FOR SELECT * FROM uctest WHERE f1 = 3;
FETCH 1 FROM a;
UPDATE uctest SET f2 = 12 WHERE CURRENT OF a;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

RESET enable_tidscan; RESET enable_seqscan; RESET enable_indexscan;

--
-- System attributes already residing in targetlist
--
BEGIN;
DECLARE c1 CURSOR FOR SELECT f1 as gp_segment_id, * FROM uctest WHERE f1 = 3; 
FETCH 1 FROM c1;
UPDATE uctest SET f2 = -1 * f2 WHERE CURRENT OF c1;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

BEGIN;
DECLARE c1 CURSOR FOR SELECT 100 as ctid, * FROM uctest WHERE f1 = 3;
FETCH 1 FROM c1;
UPDATE uctest SET f2 = -1 * f2 WHERE CURRENT OF c1;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

--
-- Repeated updates
--
BEGIN;
DECLARE c CURSOR FOR SELECT * from uctest WHERE f1 = 3;
FETCH 1 from c;
UPDATE uctest SET f2 = f2 + 10 WHERE CURRENT of c;
SELECT * from uctest ORDER BY 1, 2, 3;
UPDATE uctest SET f2 = f2 + 10 WHERE CURRENT of c;
SELECT * from uctest ORDER BY 1, 2, 3;
COMMIT;
SELECT * from uctest ORDER BY 1, 2, 3;

--
-- UPDATE with FROM and subqueries
--
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest WHERE f1 = 3;
FETCH 1 FROM c;
UPDATE uctest SET f2 = other.f2 + 10 FROM
    (SELECT * FROM uctest WHERE f1 = 3) AS other
    WHERE CURRENT OF c;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest WHERE f1 = 3;
FETCH 1 FROM c;
UPDATE uctest SET f2 = (SELECT f2 - 10 FROM uctest WHERE f1 = 3) WHERE CURRENT OF c;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

--
-- Partitioning
--

-- Partitioning: ensure each part has overlapping gp_segment_id/ctid, to test the 
--				 use of tableoid as qual
CREATE TABLE portals_updatable_rank(id int, rank int, f int)
	DISTRIBUTED BY (id)
	PARTITION BY RANGE (rank)
		(START (0) END (10) EVERY (1),
		 DEFAULT PARTITION extra );
INSERT INTO portals_updatable_rank (SELECT x, x, x FROM generate_series(0, 10) x);
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank WHERE id = 1;
FETCH 1 FROM c;
UPDATE portals_updatable_rank SET f = f * -1 WHERE CURRENT OF c;
SELECT * FROM portals_updatable_rank ORDER BY 1, 2, 3;
COMMIT;
SELECT * FROM portals_updatable_rank ORDER BY 1, 2, 3;

-- Partitioning: what if range tables of DECLARE CURSOR and CURRENT OF differ? with one
--				 pointing at logical table and the other pointing merely at a part
-- Partitioning: DECLARE CURSOR against portals_updatable_rank_1_prt_extra, CURRENT OF against portals_updatable_rank_1_prt_extra
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank_1_prt_extra;
FETCH 1 FROM c;
UPDATE portals_updatable_rank_1_prt_extra set f = f * -1 WHERE CURRENT OF c;
COMMIT;
SELECT * FROM portals_updatable_rank_1_prt_extra ORDER BY 1, 2, 3;

-- Partitioning: DECLARE CURSOR against portals_updatable_rank, CURRENT OF against portals_updatable_rank_1_prt_extra
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank WHERE rank = 10; 
DELETE FROM portals_updatable_rank_1_prt_extra WHERE CURRENT OF c;	-- error out on wrong table
ROLLBACK;

-- Partitioning: DECLARE CURSOR against portals_updatable_rank_1_prt_extra, CURRENT OF against portals_updatable_rank
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank_1_prt_extra WHERE rank = 10; 
DELETE FROM portals_updatable_rank WHERE CURRENT OF c;	-- error out on wrong table
ROLLBACK;

-- Partitioning, negative, cursor-agnostic: cannot update distribution key
UPDATE portals_updatable_rank SET id = id + 1 WHERE CURRENT OF c;

-- Partitioning, negative, cursor-agnostic: cannot move tuple across partitions
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank WHERE rank = 1;
FETCH 1 FROM c;
UPDATE portals_updatable_rank SET rank = rank + 1 WHERE CURRENT OF c;
ROLLBACK;

-- Partitioning: AO part
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_2;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (0) WITH TABLE aopart;
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank WHERE rank = 0;
FETCH 1 FROM c;
DELETE FROM portals_updatable_rank WHERE CURRENT OF c;
ROLLBACK;

-- Partitioning: AO/CO part
CREATE TABLE aocopart (LIKE portals_updatable_rank) WITH (appendonly=true, orientation=column)
DISTRIBUTED BY (id);
INSERT INTO aocopart SELECT * FROM portals_updatable_rank_1_prt_2;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (0) WITH TABLE aocopart;
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank WHERE rank = 0;
FETCH 1 FROM c;
DELETE FROM portals_updatable_rank WHERE CURRENT OF c;
ROLLBACK;

-- Partitioning: mostly AO parts
--  Despite some non-determinism, this should cover the case in which AO parts
--  are scanned by the segments. Despite our grand attempts to disallow AO/CO tables,
--  the executor will need to cope with AO tuples, for cases just like this.
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_2;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (0) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_3;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (1) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_4;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (2) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_5;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (3) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_6;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (4) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_7;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (5) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_8;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (6) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_9;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (7) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_10;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (8) WITH TABLE aopart;
DROP TABLE aopart;
CREATE TABLE aopart (LIKE portals_updatable_rank) WITH (appendonly=true) DISTRIBUTED BY (id);
INSERT INTO aopart SELECT * FROM portals_updatable_rank_1_prt_11;
ALTER TABLE portals_updatable_rank EXCHANGE PARTITION FOR (9) WITH TABLE aopart;
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM portals_updatable_rank WHERE rank = 10;    -- isolate the remaining heap part
FETCH 1 FROM c;
UPDATE portals_updatable_rank SET f = f * -1 WHERE CURRENT OF c;
COMMIT;
SELECT * FROM portals_updatable_rank ORDER BY 1, 2, 3;

-- Partitioning, sub-partitioning
CREATE TABLE bar (a int, b int, c int, d int)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE(b)
    SUBPARTITION BY LIST( c)
        SUBPARTITION TEMPLATE (
            DEFAULT SUBPARTITION subothers,
            SUBPARTITION s1 VALUES(0,1,2),
            SUBPARTITION s2 VALUES(3,4,5)
        )
    ( DEFAULT PARTITION others, START(0) END(6) EVERY(1) );
INSERT INTO bar (SELECT x, x % 6, x % 6, x FROM generate_series(0, 11) x);
BEGIN;
DECLARE a CURSOR FOR SELECT * FROM bar WHERE a = 0;
FETCH 1 FROM a;
UPDATE bar SET d = -1000 WHERE CURRENT OF a;
COMMIT;
SELECT * FROM bar ORDER BY 1, 2, 3, 4;

-- 
-- Expected Failure
--
-- WHERE CURRENT OF against SELECT ... READ ONLY
-- UPDATE succeeds despite READ ONLY designation
--

BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest WHERE f1 = 3 FOR READ ONLY;
FETCH 1 FROM c;
UPDATE uctest SET f2 = f2 + 10 WHERE CURRENT OF c; 
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

-- gp_dist_random, edge case treated as special RTE_RELATION
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM gp_dist_random('uctest') WHERE f1 = 3;
FETCH 1 FROM c;
UPDATE uctest SET f2 = f2 + 10 WHERE CURRENT OF c;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

-- naive PREPARE usage
PREPARE ucplan AS UPDATE uctest SET f2 = f2 * -1 WHERE CURRENT OF a;
BEGIN;
DECLARE a CURSOR FOR SELECT * FROM uctest WHERE f1 = 3;
FETCH 1 FROM a;
EXECUTE ucplan;
COMMIT;
SELECT * FROM uctest ORDER BY 1, 2, 3;

--
-- Negative
--

-- Negative: no such cursor
DELETE FROM uctest WHERE CURRENT OF c1; 

-- Negative: wrong UPDATE syntax
UPDATE uctest SET f2 = 5 WHERE CURRENT OF c1 AND f3 = 10;

-- Negative: can't use held cursor
DECLARE cx CURSOR WITH HOLD FOR SELECT * FROM uctest;
DELETE FROM uctest WHERE CURRENT OF cx;  

-- Negative: cursor on wrong table
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest where f1 = 3;
FETCH 1 from c;
DELETE FROM uctest2 WHERE CURRENT OF c; 
ROLLBACK;

-- Negative: cursor is ordered
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest ORDER BY f1;
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;

-- Negative: cursor is on a join
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest JOIN uctest2 USING (f1);
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;

-- Negative: cursor with subquery
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest WHERE f1 in (SELECT f1 FROM uctest);
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;

-- Negative: cursor is on aggregation
BEGIN;
DECLARE c CURSOR FOR SELECT count(*) FROM uctest;
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;
BEGIN;
DECLARE c CURSOR FOR SELECT f1,count(*) FROM uctest GROUP BY f1;
DELETE FROM uctest WHERE CURRENT OF c; 
ROLLBACK;
BEGIN;
DECLARE c CURSOR FOR SELECT f1,count(*) FROM uctest GROUP BY f1 HAVING count(*) = 1;
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;

-- Negative: cursor with distinct clause
BEGIN;
DECLARE c CURSOR FOR SELECT DISTINCT f1, f2 FROM uctest;
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;

-- Negative: cursor with limit/offset
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest LIMIT 1;
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM uctest OFFSET 1;
DELETE FROM uctest WHERE CURRENT OF c;
ROLLBACK;

-- Negative: cursor with set operations
-- MPP-16559 - DECLARE CURSOR with set operations will hit assertion
-- BEGIN;
-- DECLARE c CURSOR FOR SELECT f1 FROM uctest UNION SELECT f2 FROM uctest2;
-- DELETE FROM uctest WHERE CURRENT OF c;
-- ROLLBACK;
-- BEGIN;
-- DECLARE c CURSOR FOR SELECT f1 FROM uctest UNION ALL SELECT f2 FROM uctest2;
-- DELETE FROM uctest WHERE CURRENT OF c;
-- ROLLBACK;
-- BEGIN;
-- DECLARE c CURSOR FOR SELECT f1 FROM uctest EXCEPT SELECT f2 FROM uctest2;
-- DELETE FROM uctest WHERE CURRENT OF c;
-- ROLLBACK;

-- Negative: cursor with window clauses
BEGIN;
DECLARE c1 CURSOR FOR SELECT f1, rank() OVER (ORDER BY f1 DESC) FROM uctest ORDER BY f1;
DELETE FROM uctest WHERE CURRENT OF c1;
ROLLBACK;

-- Negative: cursor is not positioned
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM uctest;
DELETE FROM uctest WHERE CURRENT OF c1; 
ROLLBACK;
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM uctest;
FETCH ALL FROM c1;
FETCH 1 FROM c1;
DELETE FROM uctest WHERE CURRENT OF c1;
ROLLBACK;

-- Negative: cursor on views
CREATE TEMP VIEW ucview AS SELECT * FROM uctest;
CREATE RULE ucrule AS ON DELETE TO ucview DO INSTEAD
  DELETE FROM uctest WHERE f2 = OLD.f2;
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM ucview WHERE f1 = 3;
FETCH 1 FROM c1;
DELETE FROM ucview WHERE CURRENT OF c1;
ROLLBACK;

-- Negative, cursor-agnostic: cannot update distribution key
UPDATE uctest SET f1 = f1 + 10 WHERE CURRENT OF a;

-- Negative, cursor-agnostic: cannot update external tables
CREATE EXTERNAL WEB TABLE ucexttest (x text) EXECUTE 'echo "foo";' FORMAT 'TEXT';
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM ucexttest;
FETCH 1 from c;
UPDATE ucexttest SET x = 'bar' WHERE CURRENT OF c;
ROLLBACK;

DROP EXTERNAL TABLE ucexttest;

-- Negative, cursor-agnostic: cannot update AO
CREATE TEMP TABLE aotest (a int, b text)
	WITH (appendonly=true);
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM aotest;
DELETE FROM aotest WHERE CURRENT OF c;
ROLLBACK;

-- Negative, cursor-agnostic: cannot update AO/CO
CREATE TEMP TABLE aocotest (a int, b text)
	WITH (appendonly=true, orientation=column);
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM aocotest;
DELETE FROM aocotest WHERE CURRENT OF c;
ROLLBACK;


--
-- PL/pgSQL cursors
--

-- Test that cursors opened in PL/pgSQL can also be updated.
-- (Not supported by ORCA, as of this writing.)
create temp table uctest3 as
  select n as i, n as j from generate_series(1, 5) n distributed randomly;

create or replace function plpgsql_uc_test() returns void as $$
declare
  c cursor for select * from uctest3 where i = 3;
  r record;
begin
  open c;
  fetch c into r;
  raise notice '%, %', r.i, r.j;
  update uctest3 set i = i * 100, j = r.j * 2 where current of c;
end;
$$ language plpgsql;

select plpgsql_uc_test();
select * from uctest3;
