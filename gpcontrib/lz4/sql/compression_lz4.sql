-- Tests for lz4 compression.

-- Check that callbacks are registered
SELECT * FROM pg_compression WHERE compname = 'lz4';

CREATE TABLE lz4test (id int4, t text) WITH (appendonly=true, compresstype=lz4, orientation=column);

-- Check that the reloptions on the table shows compression type
SELECT reloptions FROM pg_class WHERE relname = 'lz4test';

INSERT INTO lz4test SELECT g, 'foo' || g FROM generate_series(1, 100000) g;
INSERT INTO lz4test SELECT g, 'bar' || g FROM generate_series(1, 100000) g;

-- Check that we actually compressed data. With liblz4-1.9.4, the ratio is 1.59.
SELECT get_ao_compression_ratio('lz4test') IN (1.59);

-- Check contents, at the beginning of the table and at the end.
SELECT * FROM lz4test ORDER BY (id, t) LIMIT 5;
SELECT * FROM lz4test ORDER BY (id, t) DESC LIMIT 5;


-- Test different compression levels:
CREATE TABLE lz4test_1 (id int4, t text) WITH (appendonly=true, compresstype=lz4, compresslevel=1);
CREATE TABLE lz4test_9 (id int4, t text) WITH (appendonly=true, compresstype=lz4, compresslevel=9);
CREATE TABLE lz4test_12 (id int4, t text) WITH (appendonly=true, compresstype=lz4, compresslevel=12);

INSERT INTO lz4test_1 SELECT g, 'foo' || g FROM generate_series(1, 10000) g;
INSERT INTO lz4test_1 SELECT g, 'bar' || g FROM generate_series(1, 10000) g;
SELECT * FROM lz4test_1 ORDER BY (id, t) LIMIT 5;
SELECT * FROM lz4test_1 ORDER BY (id, t) DESC LIMIT 5;

INSERT INTO lz4test_9 SELECT g, 'foo' || g FROM generate_series(1, 10000) g;
INSERT INTO lz4test_9 SELECT g, 'bar' || g FROM generate_series(1, 10000) g;
SELECT * FROM lz4test_9 ORDER BY (id, t) LIMIT 5;
SELECT * FROM lz4test_9 ORDER BY (id, t) DESC LIMIT 5;


-- Test the bounds of compresslevel. None of these are allowed.
CREATE TABLE lz4test_invalid (id int4) WITH (appendonly=true, compresstype=lz4, compresslevel=-1);
CREATE TABLE lz4test_invalid (id int4) WITH (appendonly=true, compresstype=lz4, compresslevel=0);
CREATE TABLE lz4test_invalid (id int4) WITH (appendonly=true, compresstype=lz4, compresslevel=13);

-- CREATE TABLE for heap table with compresstype=lz4 should fail
CREATE TABLE lz4test_heap (id int4, t text) WITH (compresstype=lz4);
