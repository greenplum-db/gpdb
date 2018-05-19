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

-- Make sure that we can update to a TOASTed value.
UPDATE update_test SET c = repeat('x', 10000) WHERE c = 'car';
SELECT a, b, char_length(c) FROM update_test;

DROP TABLE update_test;
