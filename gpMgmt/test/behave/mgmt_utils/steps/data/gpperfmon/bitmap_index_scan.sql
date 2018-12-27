DROP TABLE IF EXISTS bitmapIndexScanTest;
CREATE TABLE bitmapIndexScanTest(id int) distributed by (id);
CREATE INDEX idx_bitmapIndexScanTest ON bitmapIndexScanTest USING bitmap (id);
INSERT INTO bitmapIndexScanTest SELECT * FROM generate_series(1,1000);
SET enable_seqscan=off;
SET enable_indexscan=off;
SET enable_bitmapscan=on;

EXPLAIN SELECT * FROM bitmapIndexScanTest WHERE id>4;
