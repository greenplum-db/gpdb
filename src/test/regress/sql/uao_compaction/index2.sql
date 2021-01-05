-- Test index usage after vacuuming.
-- Bug verification for MPP-24913

CREATE TABLE table_index2 (a BIGINT, b BIGINT) WITH (appendonly=true);
CREATE INDEX table_index2_index_a ON table_index2(a);
CREATE INDEX table_index2_index_b ON table_index2(b);

\set QUIET off
INSERT INTO table_index2 SELECT *,* FROM generate_series(1, 347305);
INSERT INTO table_index2 SELECT *,* FROM generate_series(1, 347305);
ANALYZE table_index2;
UPDATE table_index2 SET b=100 WHERE a < 347305;
INSERT INTO table_index2 SELECT *,* FROM generate_series(1, 347305);
UPDATE table_index2 SET b=100 WHERE a < 347305;
INSERT INTO table_index2 SELECT *,* FROM generate_series(1, 347305);
INSERT INTO table_index2 SELECT *,* FROM generate_series(1, 347305);
UPDATE table_index2 SET b=101 WHERE b = 100;
VACUUM table_index2;
UPDATE table_index2 SET b=102 WHERE b = 101;
VACUUM table_index2;

SELECT COUNT(*) FROM table_index2;
SET enable_seqscan=OFF;
SELECT COUNT(*) FROM table_index2 WHERE a > 0;
