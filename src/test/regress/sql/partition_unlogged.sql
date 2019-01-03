-- Given an unlogged partition table with two leaf partitions
CREATE UNLOGGED TABLE unlogged_pt1(a int) PARTITION BY RANGE(a) (START(1) END(4) EVERY (2));
SELECT relpersistence, relname FROM pg_class WHERE relname IN (SELECT partitiontablename FROM pg_partitions WHERE tablename = 'unlogged_pt1');
-- When I split the first partition
ALTER TABLE unlogged_pt1 SPLIT PARTITION FOR(2) AT(2) INTO (PARTITION p1, PARTITION p2);
-- Then the resulting two new relations have relation persistence type 'u' for unlogged
SELECT relpersistence, relname FROM pg_class WHERE relname IN (SELECT partitiontablename FROM pg_partitions WHERE tablename = 'unlogged_pt1');
--start_ignore
DROP TABLE IF EXISTS unlogged_pt1;
--end_ignore
