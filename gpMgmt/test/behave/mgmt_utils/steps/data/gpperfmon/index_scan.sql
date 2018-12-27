DROP TABLE IF EXISTS indexScanTest;
CREATE TABLE indexScanTest(id int) distributed by (id);
CREATE INDEX idx_indexScanTest ON indexScanTest(id);
INSERT INTO indexScanTest SELECT * FROM generate_series(1,1000);
SET enable_seqscan=off;
SET enable_bitmapscan=off;
SET enable_indexscan=on;
EXPLAIN SELECT * FROM indexScanTest WHERE id=4;
