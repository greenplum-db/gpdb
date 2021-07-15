--
-- test use after free in GetSnapshotData()
-- needs a CTAS query with two slices a SQL call in both slices
--
BEGIN;
CREATE TABLE cmbt (a text, b int) DISTRIBUTED BY (a);
INSERT INTO cmbt select 1, 1 from generate_series(1, 3000)i;

CREATE OR REPLACE FUNCTION yolo (tm TEXT, s TEXT) RETURNS TEXT LANGUAGE sql VOLATILE STRICT
AS $$
	SELECT '10'::text;
$$
EXECUTE ON ANY;

-- EXPLAIN VERBOSE
CREATE TABLE ytbl AS SELECT a, yolo(a, 'writer')
	FROM (SELECT a FROM cmbt WHERE '10' = yolo('1', 'reader')) u;

ROLLBACK;
