--
-- UPDATE ... SET <col> = DEFAULT;
--

CREATE TABLE update_test (
	e   INT DEFAULT 1,
    a   INT DEFAULT 10,
    b   INT,
    c   TEXT
);

INSERT INTO update_test(a,b,c) VALUES (5, 10, 'foo');
INSERT INTO update_test(b,a) VALUES (15, 10);

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

--
-- Test multiple-set-clause syntax
--

UPDATE update_test SET (c,b,a) = ('bugle', b+11, DEFAULT) WHERE c = 'foo';
SELECT a,b,c FROM update_test ORDER BY a,b,c;
UPDATE update_test SET (c,b) = ('car', a+b), a = a + 1 WHERE a = 10;
SELECT a,b,c FROM update_test ORDER BY a,b,c;
-- fail, multi assignment to same column:
UPDATE update_test SET (c,b) = ('car', a+b), b = a + 1 WHERE a = 10;

-- XXX this should work, but doesn't yet:
UPDATE update_test SET (a,b) = (select a,b FROM update_test where c = 'foo')
  WHERE a = 10;

-- if an alias for the target table is specified, don't allow references
-- to the original table name
UPDATE update_test AS t SET b = update_test.b + 10 WHERE t.a = 10;

DROP TABLE update_test;

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

DROP TABLE IF EXISTS update_distr_key; 

CREATE TABLE update_distr_key (a int, b int) DISTRIBUTED BY (a); 
INSERT INTO update_distr_key select i, i* 10 from generate_series(0, 9) i; 

UPDATE update_distr_key SET a = 5 WHERE b = 10; 

SELECT * from update_distr_key; 

DROP TABLE update_distr_key; 
