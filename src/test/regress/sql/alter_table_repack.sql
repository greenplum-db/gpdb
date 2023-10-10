--
-- Test ALTER TABLE REPACK BY COLUMNS (...) 
--

--------------------------------------------------
-- AORO test basic functionality, will sort table data on disk
CREATE TABLE repack_aoro(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
INSERT INTO repack_aoro SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE TEMP TABLE aoro_relfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname IN ('repack_aoro')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname IN ('repack_aoro') ORDER BY segid
    DISTRIBUTED BY (segid);

ALTER TABLE repack_aoro REPACK BY COLUMNS (i DESC, j ASC);
SELECT * FROM repack_aoro WHERE gp_segment_id = 0 LIMIT 5; -- a table scan now returns sorted data
SELECT COUNT(*) FROM repack_aoro; -- expected number of rows

CREATE TEMP TABLE aoro_relfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('repack_aoro')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('repack_aoro') ORDER BY segid
    DISTRIBUTED BY (segid);

-- show that relfilenodes have all changed, as table has been rewritten
SELECT * FROM aoro_relfilebefore INTERSECT SELECT * FROM aoro_relfileafter;
DROP TABLE aoro_relfilebefore;
DROP TABLE aoro_relfileafter;

-- show that repack can be combined with other AT commands
ALTER TABLE repack_aoro
    REPACK BY COLUMNS (i),
    DROP COLUMN k,
    SET (compresslevel=5);
SELECT * FROM repack_aoro WHERE gp_segment_id = 0 LIMIT 5;
SELECT COUNT(*) FROM repack_aoro;
--------------------------------------------------

--------------------------------------------------
-- AOCO test basic functionality, will sort table data on disk
CREATE TABLE repack_aoco(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
INSERT INTO repack_aoco SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE TEMP TABLE aoco_relfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname IN ('repack_aoro')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname IN ('repack_aoco') ORDER BY segid
    DISTRIBUTED BY (segid);

ALTER TABLE repack_aoco REPACK BY COLUMNS (i);
SELECT * FROM repack_aoco WHERE gp_segment_id = 0 LIMIT 5; -- a table scan now returns sorted data
SELECT COUNT(*) FROM repack_aoco; -- expected number of rows

CREATE TEMP TABLE aoco_relfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname IN ('repack_aoco')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname IN ('repack_aoco') ORDER BY segid
    DISTRIBUTED BY (segid);

-- show that relfilenodes have all changed, as table has been rewritten
SELECT * FROM aoco_relfilebefore INTERSECT SELECT * FROM aoco_relfileafter;
DROP TABLE aoco_relfilebefore;
DROP TABLE aoco_relfileafter;

-- show that repack can be combined with other AT commands
ALTER TABLE repack_aoco
    REPACK BY COLUMNS (i),
    DROP COLUMN k,
    SET (compresslevel=5);
SELECT * FROM repack_aoro WHERE gp_segment_id = 0 LIMIT 5;
SELECT COUNT(*) FROM repack_aoro;
--------------------------------------------------

--------------------------------------------------
-- AORO test that indexes are rebuilt
CREATE TABLE repack_aoro_idx(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
INSERT INTO repack_aoro_idx SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE INDEX repack_aoro_idx_i ON repack_aoro_idx(i);
CREATE INDEX repack_aoro_idx_j ON repack_aoro_idx(j);

CREATE TEMP TABLE aoro_relfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'repack_aoro_idx%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'repack_aoro_idx%' ORDER BY segid
    DISTRIBUTED BY (segid);

ALTER TABLE repack_aoro_idx REPACK BY COLUMNS (i);

CREATE TEMP TABLE aoro_relfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'repack_aoro_idx%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'repack_aoro_idx%' ORDER BY segid
    DISTRIBUTED BY (segid);

-- show that relfilenodes have all changed, as table has been rewritten
SELECT * FROM aoro_relfilebefore INTERSECT SELECT * FROM aoro_relfileafter;
DROP TABLE aoro_relfilebefore;
DROP TABLE aoro_relfileafter;
--------------------------------------------------

--------------------------------------------------
-- AOCO test that indexes are rebuilt
CREATE TABLE repack_aoco_idx(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
INSERT INTO repack_aoco_idx SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE INDEX repack_aoco_idx_i ON repack_aoco_idx(i);
CREATE INDEX repack_aoco_idx_j ON repack_aoco_idx(j);

CREATE TEMP TABLE aoco_relfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'repack_aoco_idx%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'repack_aoco_idx%' ORDER BY segid
    DISTRIBUTED BY (segid);

ALTER TABLE repack_aoco_idx REPACK BY COLUMNS (i);

CREATE TEMP TABLE aoco_relfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'repack_aoco_idx%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'repack_aoco_idx%' ORDER BY segid
    DISTRIBUTED BY (segid);

-- show that relfilenodes have all changed, as table has been rewritten
SELECT * FROM aoco_relfilebefore INTERSECT SELECT * FROM aoco_relfileafter;
DROP TABLE aoco_relfilebefore;
DROP TABLE aoco_relfileafter;
--------------------------------------------------

--------------------------------------------------
-- AORO test partition recursion
CREATE TABLE repack_aoro_part_root(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i)
    PARTITION BY RANGE (i);

CREATE TABLE repack_aoro_part_1(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoro_part_root ATTACH PARTITION repack_aoro_part_1 FOR VALUES FROM (1) TO (500);

CREATE TABLE repack_aoro_part_2(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i) 
    PARTITION BY RANGE (i);
ALTER TABLE repack_aoro_part_root ATTACH PARTITION repack_aoro_part_2 FOR VALUES FROM (500) TO (2000);

CREATE TABLE repack_aoro_part_def(i int, j int, k text)
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoro_part_root ATTACH PARTITION repack_aoro_part_def DEFAULT;

CREATE TABLE repack_aoro_part_2_1(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoro_part_2 ATTACH PARTITION repack_aoro_part_2_1 FOR VALUES FROM (1000) TO (1500);

CREATE TABLE repack_aoro_part_2_def(i int, j int, k text)
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoro_part_2 ATTACH PARTITION repack_aoro_part_2_def DEFAULT;

INSERT INTO repack_aoro_part_root SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE TEMP TABLE aoro_relfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relfilenode != 0 AND relname LIKE 'repack_aoro_part%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relfilenode != 0 AND relname LIKE 'repack_aoro_part%' 
    ORDER BY segid DISTRIBUTED BY (segid);

ALTER TABLE repack_aoro_part_root REPACK BY COLUMNS (i);

CREATE TEMP TABLE aoro_relfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relfilenode != 0 AND relname LIKE 'repack_aoro_part%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relfilenode != 0 AND relname LIKE 'repack_aoro_part%' 
    ORDER BY segid DISTRIBUTED BY (segid);

-- show that relfilenodes have all changed, as table has been rewritten
SELECT * FROM aoro_relfilebefore INTERSECT SELECT * FROM aoro_relfileafter;
DROP TABLE aoro_relfilebefore;
DROP TABLE aoro_relfileafter;
--------------------------------------------------

--------------------------------------------------
-- AOCO test partition recursion
CREATE TABLE repack_aoco_part_root(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i)
    PARTITION BY RANGE (i);

CREATE TABLE repack_aoco_part_1(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoco_part_root ATTACH PARTITION repack_aoco_part_1 FOR VALUES FROM (1) TO (500);

CREATE TABLE repack_aoco_part_2(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i) 
    PARTITION BY RANGE (i);
ALTER TABLE repack_aoco_part_root ATTACH PARTITION repack_aoco_part_2 FOR VALUES FROM (500) TO (2000);

CREATE TABLE repack_aoco_part_def(i int, j int, k text)
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoco_part_root ATTACH PARTITION repack_aoco_part_def DEFAULT;

CREATE TABLE repack_aoco_part_2_1(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoco_part_2 ATTACH PARTITION repack_aoco_part_2_1 FOR VALUES FROM (1000) TO (1500);

CREATE TABLE repack_aoco_part_2_def(i int, j int, k text)
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoco_part_2 ATTACH PARTITION repack_aoco_part_2_def DEFAULT;

INSERT INTO repack_aoco_part_root SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE TEMP TABLE aoco_relfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relfilenode != 0 AND relname LIKE 'repack_aoco_part%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relfilenode != 0 AND relname LIKE 'repack_aoco_part%' 
    ORDER BY segid DISTRIBUTED BY (segid);

ALTER TABLE repack_aoco_part_root REPACK BY COLUMNS (i);

CREATE TEMP TABLE aoco_relfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relfilenode != 0 AND relname LIKE 'repack_aoco_part%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relfilenode != 0 AND relname LIKE 'repack_aoco_part%' 
    ORDER BY segid DISTRIBUTED BY (segid);

-- show that relfilenodes have all changed, as table has been rewritten
SELECT * FROM aoco_relfilebefore INTERSECT SELECT * FROM aoco_relfileafter;
DROP TABLE aoco_relfilebefore;
DROP TABLE aoco_relfileafter;
--------------------------------------------------


--------------------------------------------------
-- test with some more complex types, will sort table data on disk
CREATE TABLE repack_aoco_timestamp(i int, j int, k timestamp) 
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (k);
INSERT INTO repack_aoco_timestamp VALUES (1, 2, to_date('2014-02-21', 'YYYY-MM-DD BC'));
INSERT INTO repack_aoco_timestamp VALUES (1, 2, to_date('2012-04-21', 'YYYY-MM-DD BC'));
INSERT INTO repack_aoco_timestamp VALUES (1, 2, to_date('2013-03-21', 'YYYY-MM-DD BC'));
ALTER TABLE repack_aoco_timestamp REPACK BY COLUMNS (k);
SELECT * FROM repack_aoco_timestamp; -- a table scan now returns sorted data
SELECT COUNT(*) FROM repack_aoco_timestamp; -- expected number of rows

CREATE TABLE repack_aoro_float(i int, k float8, j int) 
    WITH (appendoptimized=true, orientation=row, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (k);
INSERT INTO repack_aoro_float select 1, 1e-307::float8 / 10^i, 2 FROM generate_series(1,16) i order by random();
ALTER TABLE repack_aoro_float REPACK BY COLUMNS (k DESC);
SELECT * FROM repack_aoro_float WHERE gp_segment_id = 0 LIMIT 5; -- a table scan now returns sorted data
SELECT COUNT(*) FROM repack_aoro_float; -- expected number of rows
--------------------------------------------------

--------------------------------------------------
-- test that expected warnings/errors are printed
CREATE TABLE err_heap(i int, j int, k text) DISTRIBUTED BY (i);
ALTER TABLE err_heap REPACK BY COLUMNS (i);

CREATE TABLE repack_aoco_nocol(i int, j int, k text)
    WITH (appendoptimized=true, orientation=column, compresstype=zstd, compresslevel=3)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoco_nocol REPACK BY COLUMNS (z);
ALTER TABLE repack_aoco_nocol REPACK BY COLUMNS (1);
ALTER TABLE repack_aoco_nocol REPACK BY COLUMNS (*);

ALTER TABLE repack_tbl_dne REPACK BY COLUMNS (z);

CREATE TABLE repack_aoco_combo(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=column, compresstype=none, compresslevel=0)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoco_combo 
    REPACK BY COLUMNS (i),
    DROP COLUMN i; 
ALTER TABLE repack_aoco_combo 
    REPACK BY COLUMNS (i),
    ADD COLUMN z int; 
ALTER TABLE repack_aoco_combo 
    REPACK BY COLUMNS (i),
    SET ACCESS METHOD heap; 

CREATE TABLE repack_aoro_combo(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=none, compresslevel=0)
    DISTRIBUTED BY (i);
ALTER TABLE repack_aoro_combo 
    REPACK BY COLUMNS (i),
    DROP COLUMN i; 
ALTER TABLE repack_aoro_combo 
    REPACK BY COLUMNS (i),
    ADD COLUMN z int; 
ALTER TABLE repack_aoro_combo 
    REPACK BY COLUMNS (i),
    SET ACCESS METHOD heap; 
--------------------------------------------------

--------------------------------------------------
-- AORO test with altering table-level relopts showing compression use case
CREATE TABLE repack_aoro_relopt(i int, j int, k text) 
    WITH (appendoptimized=true, orientation=row, compresstype=none, compresslevel=0)
    DISTRIBUTED BY (i);

INSERT INTO repack_aoro_relopt SELECT j, j%3, 'tryme' FROM generate_series(1, 100000)j
    ORDER BY random();

CREATE TEMP TABLE aoro_reloptbefore AS select pg_relation_size('repack_aoro_relopt')
    DISTRIBUTED BY (pg_relation_size);

ALTER TABLE repack_aoro_relopt 
    REPACK BY COLUMNS (i),
    SET (compresstype=zstd, compresslevel=3);

CREATE TEMP TABLE aoro_reloptafter AS select pg_relation_size('repack_aoro_relopt')
    DISTRIBUTED BY (pg_relation_size);

SELECT b.pg_relation_size > a.pg_relation_size AS shrunk
FROM
    aoro_reloptbefore b,
    aoro_reloptafter a;    
DROP TABLE aoro_reloptbefore;
DROP TABLE aoro_reloptafter;
--------------------------------------------------

