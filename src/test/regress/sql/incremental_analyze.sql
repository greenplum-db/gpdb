-- start_ignore
DROP DATABASE IF EXISTS incrementalanalyze;
CREATE DATABASE incrementalanalyze;
\c incrementalanalyze
DROP SCHEMA IF EXISTS incremental_analyze;
CREATE SCHEMA incremental_analyze;
-- end_ignore
-- Test ANALYZE for different data types
-- Case 1: Partitions have MCVs but after merge, none of the partition MCVs 
-- qualify as a global MCV for the root and they are used to create the
-- root histogram
DROP TABLE IF EXISTS foo;
SET default_statistics_target = 4;
CREATE TABLE foo (
    a int,
    b int, 
    c03_bigint bigint, 
    c04_bigserial bigserial, 
    c05_bit bit, 
    c06_bit_varying_10 bit varying(10), 
    c07_bool bool, 
    c08_char char, 
    c09_varchar_10 varchar(10), 
    c10_cidr cidr, 
    c11_date date, 
    c12_numeric_8_2 numeric(8,2), 
    c13_float8 float8, 
    c14_inet inet, 
    c15_int4 int4, 
    c16_interval interval, 
    c17_macaddr macaddr, 
    c18_money money, 
    c19_float4 float4, 
    c20_serial4 serial4, 
    c21_smallint smallint, 
    c22_text text, 
    c23_time time, 
    c24_timetz timetz, 
    c25_timestamp timestamp, 
    c26_timestamptz timestamptz,
    c27_uuid uuid,
    c28_tsquery tsquery
) PARTITION BY RANGE (b) (START (0) END(6) EVERY(3));
INSERT INTO foo
SELECT
i,
i%6,
i%6::bigint,
i%6,
i::bit,
(i%6)::bit(10)::bit varying(10),
(i%2)::bool,
(i%6)::char,
('a'||(i%6))::varchar(10),
('192.168.100.'||(i%6))::cidr, 
('2018-4-'||(i%6)+1)::date, 
(i%6)::numeric(8,2), 
(i%6)::float8,
inet ('192.168.100.'||(i%6)),
i%6::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%6)::text)::text::macaddr,
(i%6)::text::money,
(i%6)::float4, 
i%6,
i%6,
'aa'||(i%6)::text,
((i%6)::text||':59:59')::time,
((i%6)::text||':59:00 EST')::timetz,
('2018-01-01 '||(i%6)::text||':59:00')::timestamp,
('2018-01-01 '||(i%6)::text||':59:00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%6)::text||(i%6)::text)::uuid,
('foo'||(i%6)::text||' & rat'||(i%6)::text)::tsquery
FROM generate_series(1,100) i;
ANALYZE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' and attname != 'a' ORDER BY attname,tablename;
-- Case 2: Partitions have no MCVs, only histograms, after merge root has to
-- have approximately merged histograms.
TRUNCATE foo;
INSERT INTO foo 
SELECT
i,
i%3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i%128))::cidr, 
('2018-4-'||(i%15)+1)::date, 
(i)::numeric(8,2), 
(i)::float8,
inet ('192.168.100.'||(i%128)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%50)::text)::text::macaddr,
(i)::text::money,
(i)::float4, 
i,
i,
'aa'||(i)::text,
((i%12)::text||':59:59')::time,
((i%12)::text||':59:00 EST')::timetz,
('2018-01-01 '||(i%12)::text||':59:00')::timestamp,
('2018-01-01 '||(i%12)::text||':59:00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%5)::text||(i%5)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,100) i;
INSERT INTO foo 
SELECT
i,
i%3,
i+200::bigint,
i+200,
i::bit,
(i+200)::bit(10)::bit varying(10),
(i+200)::bool,
(i+200)::char,
('a'||(i+200))::varchar(10),
('192.168.100.'||(i%128+128))::cidr, 
('2018-4-'||(i%15+16))::date, 
(i)::numeric(8,2),
(i)::float8,
inet ('192.168.100.'||(i%128+128)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%50+50)::text)::text::macaddr,
(i)::text::money,
(i)::float4,
i,
i,
'aa'||(i)::text,
((i%12+12)::text||':59:59')::time,
((i%12+12)::text||':59:00 EST')::timetz,
('2018-01-01 '||(i%12+12)::text||':59:00')::timestamp,
('2018-01-01 '||(i%12+12)::text||':59:00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%5+5)::text||(i%5+5)::text)::uuid,
('foo'||(i+200)::text||' & rat'||(i+200)::text)::tsquery
FROM generate_series(1,100) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i%128))::cidr, 
('2018-4-'||(i%15)+1)::date, 
(i)::numeric(8,2), 
(i)::float8,
inet ('192.168.100.'||(i%128)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%50)::text)::text::macaddr,
(i)::text::money,
(i)::float4, 
i,
i,
'aa'||(i)::text,
((i%12)::text||':59:59')::time,
((i%12)::text||':59:00 EST')::timetz,
('2018-01-01 '||(i%12)::text||':59:00')::timestamp,
('2018-01-01 '||(i%12)::text||':59:00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%5)::text||(i%5)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,200) i;
ANALYZE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Case 3: Partitions have MCVs but after merge, MCVs that do not qualify as 
-- global MCV for the root will be used to create root histograms
TRUNCATE foo;
INSERT INTO foo 
SELECT
i,
i%3,
i%4::bigint,
i%4,
i::bit,
(i%4)::bit(10)::bit varying(10),
(i)::bool,
(i%4)::char,
('a'||(i%4))::varchar(10),
('192.168.100.'||(i%4))::cidr, 
('2018-4-'||(i%4)+1)::date, 
(i%4)::numeric(8,2), 
(i%4)::float8,
inet ('192.168.100.'||(i%4)),
i%4::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%4)::text)::text::macaddr,
(i%4)::text::money,
(i%4)::float4, 
i%4,
i%4,
'aa'||(i%4)::text,
('12:'||(i%4)::text||':00')::time,
('12:'||(i%4)::text||':00 EST')::timetz,
('2018-01-01 '||'12:'||(i%4)::text||':00')::timestamp,
('2018-01-01 '||'12:'||(i%4)::text||':00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%4)::text||(i%4)::text)::uuid,
('foo'||(i%4)::text||' & rat'||(i%4)::text)::tsquery
FROM generate_series(1,100) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i))::cidr, 
('2018-4-'||(i%26+1))::date, 
(i)::numeric(8,2),
(i)::float8,
inet ('192.168.100.'||(i)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i)::text)::text::macaddr,
(i)::text::money,
(i)::float4,
i,
i,
'aa'||(i)::text,
('12:'||(i%60)::text||':00')::time,
('12:'||(i%60)::text||':00 EST')::timetz,
('2018-01-01 '||'12:'||(i%60)::text||':00')::timestamp,
('2018-01-01 '||'12:'||(i%60)::text||':00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%6)::text||(i%6)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,60) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i%4+61::bigint,
i%4+61,
(i%4+61)::bit,
(i%4+61)::bit(10)::bit varying(10),
(i%4+61)::bool,
(i%4+61)::char,
('a'||(i%4+61))::varchar(10),
('192.168.100.'||(i%4+61))::cidr, 
('2018-4-'||(i%4)+27)::date, 
(i%4+61)::numeric(8,2), 
(i%4+61)::float8,
inet ('192.168.100.'||(i%4+61)),
i%4+61::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%4+61)::text)::text::macaddr,
(i%4+61)::text::money,
(i%4+61)::float4, 
i%4+61,
i%4+61,
'aa'||(i%4+61)::text,
('12:59:'||(i%4+10)::text)::time,
('12:59:'||(i%4+10)::text||' EST')::timetz,
('2018-01-01 '||'12:59:'||(i%4+10)::text)::timestamp,
('2018-01-01 '||'12:59:'||(i%4+10)::text||' EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%4+6)::text||(i%4+6)::text)::uuid,
('foo'||(i%4)::text||' & rat'||(i%4)::text)::tsquery
FROM generate_series(1,40) i;
ANALYZE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Case 4: A partition has MCVs but after merge, those MCVs do not qualify as 
-- global MCV for the root will be used to create root histograms
TRUNCATE foo;
INSERT INTO foo 
SELECT
i,
i%3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i%128))::cidr, 
('2018-4-'||(i%15)+1)::date, 
(i)::numeric(8,2), 
(i)::float8,
inet ('192.168.100.'||(i%128)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%50)::text)::text::macaddr,
(i)::text::money,
(i)::float4, 
i,
i,
'aa'||(i)::text,
((i%12)::text||':59:59')::time,
((i%12)::text||':59:00 EST')::timetz,
('2018-01-01 '||(i%12)::text||':59:00')::timestamp,
('2018-01-01 '||(i%12)::text||':59:00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%5)::text||(i%5)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,100) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i%4+61::bigint,
i%4+61,
(i%4+61)::bit,
(i%4+61)::bit(10)::bit varying(10),
(i%4+61)::bool,
(i%4+61)::char,
('a'||(i%4+61))::varchar(10),
('192.168.100.'||(i%4+61))::cidr, 
('2018-4-'||(i%4+26))::date, 
(i%4+61)::numeric(8,2),
(i%4+61)::float8,
inet ('192.168.100.'||(i%4+61)),
i%4+61::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%4+61)::text)::text::macaddr,
(i%4+61)::text::money,
(i%4+61)::float4,
i%4+61,
i%4+61,
'aa'||(i%4+61)::text,
('12:'||(i%4+56)::text||':00')::time,
('12:'||(i%4+56)::text||':00 EST')::timetz,
('2018-01-01 '||'12:'||(i%4+56)::text||':00')::timestamp,
('2018-01-01 '||'12:'||(i%4+56)::text||':00 EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%4+6)::text||(i%4+6)::text)::uuid,
('foo'||(i%4+61)::text||' & rat'||(i%4+61)::text)::tsquery
FROM generate_series(1,8) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i))::cidr, 
('2018-4-'||(i%25+1))::date, 
(i)::numeric(8,2),
(i)::float8,
inet ('192.168.100.'||(i)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i)::text)::text::macaddr,
(i)::text::money,
(i)::float4,
i,
i,
'aa'||(i)::text,
('12:00:'||(i%60)::text)::time,
('12:00:'||(i%60)::text||' EST')::timetz,
('2018-01-01 '||'12:00'||(i%60)::text)::timestamp,
('2018-01-01 '||'12:00'||(i%60)::text||' EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%10)::text||(i%10)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,60) i;
ANALYZE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' and attname != 'a' ORDER BY attname,tablename;
-- Case 5: A partition has MCVs but after merge, those MCVs qualify as global
-- MCVs for the root
TRUNCATE foo;
INSERT INTO foo 
SELECT
i,
i%3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i%128))::cidr, 
('2018-'||(i%12)+1||'-'||(i%15)+1)::date,
(i)::numeric(8,2), 
(i)::float8,
inet ('192.168.100.'||(i%128)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%50)::text)::text::macaddr,
(i)::text::money,
(i)::float4, 
i,
i,
'aa'||(i)::text,
('12:'||(i%60)::text||':'||(i%30)::text)::time,
('12:'||(i%60)::text||':'||(i%30)::text||' EST')::timetz,
('2018-01-01 '||'12:'||(i%60)::text||':'||(i%30)::text)::timestamp,
('2018-01-01 '||'12:'||(i%60)::text||':'||(i%30)::text||' EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%5)::text||(i%5)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,60) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i%4+61::bigint,
i%4+61,
(i%4+61)::bit,
(i%4+61)::bit(10)::bit varying(10),
(i%4+61)::bool,
(i%4+61)::char,
('a'||(i%4+61))::varchar(10),
('192.168.100.'||(i%4+61))::cidr, 
('2018-5-'||(i%4)+16)::date,
(i%4+61)::numeric(8,2),
(i%4+61)::float8,
inet ('192.168.100.'||(i%4+61)),
i%4+61::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i%4+61)::text)::text::macaddr,
(i%4+61)::text::money,
(i%4+61)::float4,
i%4+61,
i%4+61,
'aa'||(i%4+61)::text,
('01:00:'||(i%4)::text)::time,
('01:00:'||(i%4)::text||' EST')::timetz,
('2018-01-01 '||'01:00:'||(i%4)::text)::timestamp,
('2018-01-01 '||'01:00:'||(i%4)::text||' EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%4+6)::text||(i%4+6)::text)::uuid,
('foo'||(i%4+61)::text||' & rat'||(i%4+61)::text)::tsquery
FROM generate_series(1,8) i;
INSERT INTO foo 
SELECT
i,
i%3+3,
i::bigint,
i,
i::bit,
(i)::bit(10)::bit varying(10),
(i)::bool,
(i)::char,
('a'||(i))::varchar(10),
('192.168.100.'||(i))::cidr, 
('2018-'||(i%12)+1||'-'||(i%15)+1)::date,
(i)::numeric(8,2),
(i)::float8,
inet ('192.168.100.'||(i)),
i::int4,
interval '24 hours',
macaddr ('12:34:56:78:90:'||(i)::text)::text::macaddr,
(i)::text::money,
(i)::float4,
i,
i,
'aa'||(i)::text,
('12:'||(i%60)::text||':'||(i%30)::text)::time,
('12:'||(i%60)::text||':'||(i%30)::text||' EST')::timetz,
('2018-01-01 '||'12:'||(i%60)::text||':'||(i%30)::text)::timestamp,
('2018-01-01 '||'12:'||(i%60)::text||':'||(i%30)::text||' EST')::timestamptz,
('11111111-1111-1111-1111-1111111111'||(i%10)::text||(i%10)::text)::uuid,
('foo'||(i)::text||' & rat'||(i)::text)::tsquery
FROM generate_series(1,10) i;
ANALYZE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' and attname != 'a' ORDER BY attname,tablename;
-- Test merging leaf stats where HLL is empty
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo select i, i%6, repeat('aaaa', 100000) FROM generate_series(1, 100)i;
ANALYZE foo;
SELECT * FROM pg_stats WHERE tablename like 'foo%' and attname = 'c' ORDER BY attname,tablename;
-- Test ANALYZE full scan HLL
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%3, 'text_'||i%100 FROM generate_series(1,1000)i; 
INSERT INTO foo SELECT i, i%3+3, 'text_'||i%200 FROM generate_series(1,1000)i; 
SET default_statistics_target to 3;
ANALYZE FULLSCAN foo;
SELECT tablename, n_distinct FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Test ANLYZE MERGE behavior
-- Merge stats from only one partition
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE foo_1_prt_1;
ANALYZE MERGE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge stats from both partitions
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
ANALYZE MERGE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- No stats after MERGE
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
ANALYZE MERGE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge stats from only one partition
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
SET allow_system_table_mods = 'DML';
UPDATE pg_attribute SET attstattarget=0 WHERE attrelid = 'foo_1_prt_1'::regclass and ATTNAME in ('a','b','c');
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
ANALYZE MERGE foo;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge stats from only one partition one column
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE foo_1_prt_1(c);
ANALYZE MERGE foo(c);
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge fails as all partitions are not analyzed
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE MERGE foo;

-- Merge fails as all partitions are empty and are not analyzed
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
ANALYZE MERGE foo;

-- Merge errors on partition of a table
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo select i, i%6, i%100 FROM generate_series(1,2000)i;
ANALYZE MERGE foo_1_prt_1;

-- Test merging of leaf stats when one partition has
-- FULL SCAN HLL and the other has HLL from sample
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO FOO SELECT i,i%6 FROM generate_series(1,1000)i;
ANALYZE foo_1_prt_1;
ANALYZE FULLSCAN foo_1_prt_2;
ANALYZE MERGE foo;
-- Test merging of stats for a newly added partition
-- Do not collect samples while merging stats
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO FOO SELECT i,i%6 FROM generate_series(1,1000)i;
SET default_statistics_target = 4;
ANALYZE foo;
ALTER TABLE foo ADD partition new_part START (6) INCLUSIVE END (9) EXCLUSIVE;
INSERT INTO foo SELECT i, i%3+6 FROM generate_series(1,500)i;
ANALYZE foo_1_prt_new_part;
SET client_min_messages = 'log';
ANALYZE MERGE foo;
-- Insert a new column that is not analyzed in the leaf partitions.
-- Analyzing root partition will use merging statistics for the first 2 columns, 
-- will create a sample for the root to analyze the newly added columns since 
-- the leaf partitions does not have any stats for it, yet
ALTER TABLE foo ADD COLUMN c int;
INSERT INTO foo SELECT i, i%9, i%100 FROM generate_series(1,500)i;
ANALYZE rootpartition foo;
