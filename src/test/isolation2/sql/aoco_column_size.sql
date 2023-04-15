-- Tests to validate AOCO column size and compression reporting functions and views

-- start_matchignore
-- m/.*compression_ratio .*[02-9]+[.]?(\d+)?/
-- end_matchignore

CREATE SCHEMA aoco_size;

-- Test gp_toolkit.get_column_size(oid)
CREATE TABLE aoco_size.heap(i int, j int) distributed by (i);
CREATE TABLE aoco_size.aorow(i int, j int) using ao_row distributed by (i);
CREATE TABLE aoco_size.aocol
(
  i int,
  j int ENCODING (compresstype=zstd, compresslevel=5),
  k bigint ENCODING (compresstype=rle_type, compresslevel=1),
  dropped int -- doesn't report dropped columns
)
USING ao_column
DISTRIBUTED BY (i)
PARTITION BY RANGE (j);

ALTER TABLE aoco_size.aocol DROP COLUMN dropped;

-- Attach 2 partitions with different column encodings
CREATE TABLE aoco_size.aocol_part_t1
(
  i int,
  j int ENCODING (compresstype=zlib, compresslevel=1),
  k bigint ENCODING (compresstype=zlib, compresslevel=1)
)
USING ao_column
DISTRIBUTED BY (i);

CREATE TABLE aoco_size.aocol_part_t2
(
  i int,
  j int ENCODING (compresstype=zstd, compresslevel=3),
  k bigint ENCODING (compresstype=zstd, compresslevel=1)
)
USING ao_column
DISTRIBUTED BY (i);

ALTER TABLE aoco_size.aocol ATTACH PARTITION aoco_size.aocol_part_t1 FOR VALUES FROM (1) TO (500);
ALTER TABLE aoco_size.aocol ATTACH PARTITION aoco_size.aocol_part_t2 FOR VALUES FROM (500) TO (1001);

-- Non-existent name/OID.
SELECT * FROM gp_toolkit.get_column_size('aoco_size.foo'::regclass);
-- Non AOCO relations
SELECT * FROM  gp_toolkit.get_column_size('aoco_size.heap'::regclass);
SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aorow'::regclass);
-- Partition parent
SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol'::regclass) ORDER BY 1,2;

INSERT INTO aoco_size.aocol SELECT i, i, i FROM generate_series(1, 1000) i;

SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol_part_t1'::regclass) ORDER BY 1,2;
-1U: SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol_part_t1'::regclass) ORDER BY 1,2;
0U: SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol_part_t1'::regclass) ORDER BY 1,2;

SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol_part_t2'::regclass) ORDER BY 1,2;
-1U: SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol_part_t2'::regclass) ORDER BY 1,2;
0U: SELECT * FROM  gp_toolkit.get_column_size('aoco_size.aocol_part_t2'::regclass) ORDER BY 1,2;

-- Test gp_toolkit.gp_column_size view
SELECT relname, gp_segment_id, attnum, attname, size, size_uncompressed, compression_ratio FROM gp_toolkit.gp_column_size where schema='aoco_size';
-1U: SELECT relname, gp_segment_id, attnum, attname, size, size_uncompressed, compression_ratio FROM gp_toolkit.gp_column_size where schema='aoco_size';
0U:  SELECT relname, gp_segment_id, attnum, attname, size, size_uncompressed, compression_ratio FROM gp_toolkit.gp_column_size where schema='aoco_size';

-- Test gp_toolkit.gp_column_size_summary view
SELECT relname, attnum, size, size_uncompressed, compression_ratio FROM gp_toolkit.gp_column_size_summary WHERE schema='aoco_size';

DROP SCHEMA aoco_size CASCADE;
