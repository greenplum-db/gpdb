-- Tests for quicklz compression.

-- Test for appendonly row oriented
CREATE TABLE quicklztest_row (id int4, t text) WITH (appendonly=true, compresstype=quicklz, orientation=row);

INSERT INTO quicklztest_row SELECT g, 'foo' || g FROM generate_series(1, 100) g;
INSERT INTO quicklztest_row SELECT g, 'bar' || g FROM generate_series(1, 100) g;

-- Check contents, at the beginning of the table and at the end.
SELECT * FROM quicklztest_row ORDER BY id LIMIT 5;
SELECT * FROM quicklztest_row ORDER BY id DESC LIMIT 5;

-- Check that we actually compressed data
SELECT get_ao_compression_ratio('quicklztest_row') > 1;

-- Test for appendonly column oriented
CREATE TABLE quicklztest_column (id int4, t text) WITH (appendonly=true, compresstype=quicklz, orientation=column);

INSERT INTO quicklztest_column SELECT g, 'foo' || g FROM generate_series(1, 100) g;
INSERT INTO quicklztest_column SELECT g, 'bar' || g FROM generate_series(1, 100) g;

-- Check contents, at the beginning of the table and at the end.
SELECT * FROM quicklztest_column ORDER BY id LIMIT 5;
SELECT * FROM quicklztest_column ORDER BY id DESC LIMIT 5;

-- Check that we actually compressed data
SELECT get_ao_compression_ratio('quicklztest_column') > 1;

-- Test the bounds of compresslevel. QuickLZ compresslevel 1 is the only one that should work.
CREATE TABLE quicklztest_invalid (id int4) WITH (appendonly=true, compresstype=quicklz, compresslevel=-1);
CREATE TABLE quicklztest_invalid (id int4) WITH (appendonly=true, compresstype=quicklz, compresslevel=0);
CREATE TABLE quicklztest_invalid (id int4) WITH (appendonly=true, compresstype=quicklz, compresslevel=3);
