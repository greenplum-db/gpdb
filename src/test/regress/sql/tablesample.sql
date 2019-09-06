CREATE TABLE test_tablesample (dist int, id int, name text) WITH (fillfactor=10) DISTRIBUTED BY (dist); -- force smaller pages so we don't have to load too much data to get multiple pages

-- Changed the column length in order to match the expected results based on relation's blocksz
INSERT INTO test_tablesample SELECT 0, i, repeat(i::text, 875) FROM generate_series(0, 9) s(i) ORDER BY i;
INSERT INTO test_tablesample SELECT 3, i, repeat(i::text, 875) FROM generate_series(10, 19) s(i) ORDER BY i;
INSERT INTO test_tablesample SELECT 5, i, repeat(i::text, 875) FROM generate_series(20, 29) s(i) ORDER BY i;

-- Verify that each segment has the same amount of rows;
SELECT gp_segment_id, count(dist) FROM test_tablesample GROUP BY 1 ORDER BY 1;

SELECT t.id FROM test_tablesample AS t TABLESAMPLE SYSTEM (50) REPEATABLE (10);
SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (100.0/11) REPEATABLE (9999);
SELECT count(*) FROM test_tablesample TABLESAMPLE SYSTEM (100);
SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (50) REPEATABLE (100);
SELECT id FROM test_tablesample TABLESAMPLE BERNOULLI (50) REPEATABLE (100);
SELECT id FROM test_tablesample TABLESAMPLE BERNOULLI (5.5) REPEATABLE (1);

CREATE VIEW test_tablesample_v1 AS SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (10*2) REPEATABLE (2);
CREATE VIEW test_tablesample_v2 AS SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (99);
SELECT pg_get_viewdef('test_tablesample_v1'::regclass);
SELECT pg_get_viewdef('test_tablesample_v2'::regclass);

BEGIN;
DECLARE tablesample_cur CURSOR FOR SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (50) REPEATABLE (100) ORDER BY id;
FETCH FIRST FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;

SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (50) REPEATABLE (10);

FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;

-- Greenplum: Going backwards on cursors is not supported. By closing the
-- cursor and starting again we pass the tests and keep the file closer to
-- upstream. We do test the rescan methods of tablesample afterwards.
CLOSE tablesample_cur;
DECLARE tablesample_cur CURSOR FOR SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (50) REPEATABLE (100) ORDER BY id;
FETCH FIRST FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;
FETCH NEXT FROM tablesample_cur;

CLOSE tablesample_cur;
END;

-- Greenplum: Test rescan paths by forcing a nested loop
CREATE TABLE ttr1 (a int, b int) DISTRIBUTED BY (a);
CREATE TABLE ttr2 (a int, b int) DISTRIBUTED BY (a);
INSERT INTO ttr1 VALUES (1, 1), (NULL, NULL);
INSERT INTO ttr2 VALUES (1, 5), (NULL, 6);
SET enable_hashjoin TO OFF;
SET enable_mergejoin TO OFF;
SET enable_nestloop TO ON;

EXPLAIN SELECT * FROM ttr1 TABLESAMPLE BERNOULLI (50) REPEATABLE (100), ttr2 TABLESAMPLE BERNOULLI (50) REPEATABLE (100) WHERE ttr1.a = ttr2.a;
SELECT * FROM ttr1 TABLESAMPLE BERNOULLI (50) REPEATABLE (100), ttr2 TABLESAMPLE BERNOULLI (50) REPEATABLE (100) WHERE ttr1.a = ttr2.a;
EXPLAIN SELECT * FROM ttr1 TABLESAMPLE SYSTEM (50) REPEATABLE (100), ttr2 TABLESAMPLE SYSTEM (50) REPEATABLE (100) WHERE ttr1.a = ttr2.a;
SELECT * FROM ttr1 TABLESAMPLE SYSTEM (50) REPEATABLE (100), ttr2 TABLESAMPLE SYSTEM (50) REPEATABLE (100) WHERE ttr1.a = ttr2.a;

RESET enable_hashjoin;
RESET enable_mergejoin;
RESET enable_nestloop;
DROP TABLE ttr1;
DROP TABLE ttr2;

EXPLAIN SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (50) REPEATABLE (10);
EXPLAIN SELECT * FROM test_tablesample_v1;

-- errors
SELECT id FROM test_tablesample TABLESAMPLE FOOBAR (1);

SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (50) REPEATABLE (NULL);

SELECT id FROM test_tablesample TABLESAMPLE BERNOULLI (-1);
SELECT id FROM test_tablesample TABLESAMPLE BERNOULLI (200);
SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (-1);
SELECT id FROM test_tablesample TABLESAMPLE SYSTEM (200);

SELECT id FROM test_tablesample_v1 TABLESAMPLE BERNOULLI (1);
INSERT INTO test_tablesample_v1 VALUES(1);

WITH query_select AS (SELECT * FROM test_tablesample)
SELECT * FROM query_select TABLESAMPLE BERNOULLI (5.5) REPEATABLE (1);

SELECT q.* FROM (SELECT * FROM test_tablesample) as q TABLESAMPLE BERNOULLI (5);

-- catalog sanity

SELECT *
FROM pg_tablesample_method
WHERE tsminit IS NULL
   OR tsmseqscan IS NULL
   OR tsmpagemode IS NULL
   OR tsmnextblock IS NULL
   OR tsmnexttuple IS NULL
   OR tsmend IS NULL
   OR tsmreset IS NULL
   OR tsmcost IS NULL;

-- done
DROP TABLE test_tablesample CASCADE;
