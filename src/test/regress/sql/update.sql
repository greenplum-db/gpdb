--
-- UPDATE ... SET <col> = DEFAULT;
--

CREATE TABLE update_test (
    a   INT DEFAULT 10,
    b   INT,
    c   TEXT
);

CREATE TABLE upsert_test (
    a   INT PRIMARY KEY,
    b   TEXT
);

INSERT INTO update_test VALUES (5, 10, 'foo');
INSERT INTO update_test(b, a) VALUES (15, 10);

SELECT a,b,c FROM update_test ORDER BY a,b,c;

UPDATE update_test SET a = DEFAULT, b = DEFAULT;

SELECT a,b,c FROM update_test ORDER BY a,b,c;

-- aliases for the UPDATE target table
UPDATE update_test AS t SET b = 10 WHERE t.a = 10;

SELECT a,b,c FROM update_test ORDER BY a,b,c;

UPDATE update_test t SET b = t.b + 10 WHERE t.a = 10;

SELECT a,b,c FROM update_test ORDER BY a,b,c;

--
-- Test VALUES in FROM
--

UPDATE update_test SET a=v.i FROM (VALUES(100, 20)) AS v(i, j)
  WHERE update_test.b = v.j;

SELECT a,b,c FROM update_test ORDER BY a,b,c;

-- fail, wrong data type:
UPDATE update_test SET a = v.* FROM (VALUES(100, 20)) AS v(i, j)
  WHERE update_test.b = v.j;

--
-- Test multiple-set-clause syntax
--

INSERT INTO update_test SELECT a,b+1,c FROM update_test;
SELECT * FROM update_test;

UPDATE update_test SET (c,b,a) = ('bugle', b+11, DEFAULT) WHERE c = 'foo';
SELECT a,b,c FROM update_test ORDER BY a,b,c;
UPDATE update_test SET (c,b) = ('car', a+b), a = a + 1 WHERE a = 10;
SELECT a,b,c FROM update_test ORDER BY a,b,c;
-- fail, multi assignment to same column:
UPDATE update_test SET (c,b) = ('car', a+b), b = a + 1 WHERE a = 10;

-- uncorrelated sub-select:
UPDATE update_test
  SET (b,a) = (select a,b from update_test where b = 41 and c = 'car')
  WHERE a = 100 AND b = 20;
SELECT * FROM update_test;
-- correlated sub-select:
UPDATE update_test o
  SET (b,a) = (select a+1,b from update_test i
               where i.a=o.a and i.b=o.b and i.c is not distinct from o.c);
SELECT * FROM update_test;
-- fail, multiple rows supplied:
UPDATE update_test SET (b,a) = (select a+1,b from update_test);
-- set to null if no rows supplied:
UPDATE update_test SET (b,a) = (select a+1,b from update_test where a = 1000)
  WHERE a = 11;
SELECT * FROM update_test;

-- if an alias for the target table is specified, don't allow references
-- to the original table name
UPDATE update_test AS t SET b = update_test.b + 10 WHERE t.a = 10;

-- Make sure that we can update to a TOASTed value.
UPDATE update_test SET c = repeat('x', 10000) WHERE c = 'car';
SELECT a, b, char_length(c) FROM update_test;

-- Test ON CONFLICT DO UPDATE
INSERT INTO upsert_test VALUES(1, 'Boo');
-- uncorrelated  sub-select:
WITH aaa AS (SELECT 1 AS a, 'Foo' AS b) INSERT INTO upsert_test
  VALUES (1, 'Bar') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b, a FROM aaa) RETURNING *;
-- correlated sub-select:
INSERT INTO upsert_test VALUES (1, 'Baz') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b || ', Correlated', a from upsert_test i WHERE i.a = upsert_test.a)
  RETURNING *;
-- correlated sub-select (EXCLUDED.* alias):
INSERT INTO upsert_test VALUES (1, 'Bat') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b || ', Excluded', a from upsert_test i WHERE i.a = excluded.a)
  RETURNING *;

DROP TABLE update_test;
DROP TABLE upsert_test;

--
-- text types. We should support the following updates.
--
drop table tab1;
drop table tab2;

CREATE TABLE tab1 (a varchar(15), b integer) DISTRIBUTED BY (a);
CREATE TABLE tab2 (a varchar(15), b integer) DISTRIBUTED BY (a);

UPDATE tab1 SET b = tab2.b FROM tab2 WHERE tab1.a = tab2.a;

drop table tab1;
drop table tab2;

CREATE TABLE tab1 (a text, b integer) DISTRIBUTED BY (a);
CREATE TABLE tab2 (a text, b integer) DISTRIBUTED BY (a);

UPDATE tab1 SET b = tab2.b FROM tab2 WHERE tab1.a = tab2.a;

drop table tab1;
drop table tab2;

CREATE TABLE tab1 (a varchar, b integer) DISTRIBUTED BY (a);
CREATE TABLE tab2 (a varchar, b integer) DISTRIBUTED BY (a);

UPDATE tab1 SET b = tab2.b FROM tab2 WHERE tab1.a = tab2.a;

drop table tab1;
drop table tab2;

CREATE TABLE tab1 (a char(15), b integer) DISTRIBUTED BY (a);
CREATE TABLE tab2 (a char(15), b integer) DISTRIBUTED BY (a);

UPDATE tab1 SET b = tab2.b FROM tab2 WHERE tab1.a = tab2.a;

drop table tab1;
drop table tab2;

DROP TABLE IF EXISTS update_distr_key; 
CREATE TABLE update_distr_key (a int, b int) DISTRIBUTED BY (a); 
INSERT INTO update_distr_key select i, i* 10 from generate_series(0, 9) i; 

UPDATE update_distr_key SET a = 5 WHERE b = 10; 

SELECT * from update_distr_key; 

DROP TABLE update_distr_key;

-- below cases is to test multi-hash-cols
CREATE TABLE tab3(c1 int, c2 int, c3 int, c4 int, c5 int) DISTRIBUTED BY (c1, c2, c3);
CREATE TABLE tab5(c1 int, c2 int, c3 int, c4 int, c5 int) DISTRIBUTED BY (c1, c2, c3, c4, c5);

INSERT INTO tab3 SELECT i, i, i, i, i FROM generate_series(1, 10)i;
INSERT INTO tab5 SELECT i, i, i, i, i FROM generate_series(1, 10)i;

-- test tab3
SELECT gp_segment_id, * FROM tab3;
UPDATE tab3 set c1 = 9 where c4 = 1;
SELECT gp_segment_id, * FROM tab3;
UPDATE tab3 set (c1,c2) = (5,6) where c4 = 1;
SELECT gp_segment_id, * FROM tab3;
UPDATE tab3 set (c1,c2,c3) = (3,2,1) where c4 = 1;
SELECT gp_segment_id, * FROM tab3;
UPDATE tab3 set c1 = 11 where c2 = 10 and c2 < 1;
SELECT gp_segment_id, * FROM tab3;

-- test tab5
SELECT gp_segment_id, * FROM tab5;
UPDATE tab5 set c1 = 1000 where c4 = 1;
SELECT gp_segment_id, * FROM tab5;
UPDATE tab5 set (c1,c2) = (9,10) where c4 = 1;
SELECT gp_segment_id, * FROM tab5;
UPDATE tab5 set (c1,c2,c4) = (5,8,6) where c4 = 1;
SELECT gp_segment_id, * FROM tab5;
UPDATE tab5 set (c1,c2,c3,c4,c5) = (1,2,3,0,6) where c5 = 1;
SELECT gp_segment_id, * FROM tab5;
UPDATE tab5 set c1 = 11 where c3 = 10 and c3 < 1;
SELECT gp_segment_id, * FROM tab5;

EXPLAIN (COSTS OFF ) UPDATE tab3 SET C1 = C1 + 1, C5 = C5+1;

-- clean up
drop table tab3;
drop table tab5;
