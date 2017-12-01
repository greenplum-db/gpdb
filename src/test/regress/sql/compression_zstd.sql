-- Tests for zstd compression.

CREATE TABLE zstdtest (id int4, t text) WITH (appendonly=true, compresstype=zstd, orientation=column);

INSERT INTO zstdtest SELECT g, 'foo' || g FROM generate_series(1, 100000) g;
INSERT INTO zstdtest SELECT g, 'bar' || g FROM generate_series(1, 100000) g;

-- Check contents, at the beginning of the table and at the end.
SELECT * FROM zstdtest ORDER BY id LIMIT 5;
SELECT * FROM zstdtest ORDER BY id DESC LIMIT 5;


-- Test different compression levels:
CREATE TABLE zstdtest_1 (id int4, t text) WITH (appendonly=true, compresstype=zstd, compresslevel=1);
CREATE TABLE zstdtest_10 (id int4, t text) WITH (appendonly=true, compresstype=zstd, compresslevel=10);
CREATE TABLE zstdtest_19 (id int4, t text) WITH (appendonly=true, compresstype=zstd, compresslevel=19);

INSERT INTO zstdtest_1 SELECT g, 'foo' || g FROM generate_series(1, 10000) g;
INSERT INTO zstdtest_1 SELECT g, 'bar' || g FROM generate_series(1, 10000) g;
SELECT * FROM zstdtest_1 ORDER BY id LIMIT 5;
SELECT * FROM zstdtest_1 ORDER BY id DESC LIMIT 5;

INSERT INTO zstdtest_19 SELECT g, 'foo' || g FROM generate_series(1, 10000) g;
INSERT INTO zstdtest_19 SELECT g, 'bar' || g FROM generate_series(1, 10000) g;
SELECT * FROM zstdtest_19 ORDER BY id LIMIT 5;
SELECT * FROM zstdtest_19 ORDER BY id DESC LIMIT 5;


-- Test the bounds of compresslevel. None of these are allowed.
CREATE TABLE zstdtest_invalid (id int4) WITH (appendonly=true, compresstype=zstd, compresslevel=-1);
CREATE TABLE zstdtest_invalid (id int4) WITH (appendonly=true, compresstype=zstd, compresslevel=0);
CREATE TABLE zstdtest_invalid (id int4) WITH (appendonly=true, compresstype=zstd, compresslevel=20);
