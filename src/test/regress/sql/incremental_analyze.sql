-- start_matchsubs
-- m/^LOG.*PartitionSelector/
-- s/^LOG.*PartitionSelector/PartitionSelector/
-- m/^LOG.*Feature not supported/
-- s/^LOG.*Feature not supported/Feature not supported/
-- end_matchsubs
-- start_ignore
DROP DATABASE IF EXISTS incrementalanalyze;
CREATE DATABASE incrementalanalyze;
ALTER DATABASE incrementalanalyze SET lc_monetary TO 'C';
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
CREATE TYPE inc_analyze_composite AS
(
    a numeric,
    b numeric
);
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
    c28_tsquery tsquery,
    c29_varchararray character varying(2)[],
    c30_intarray int[],
    c31_bigintarray bigint[],
    c33_bitarray bit(1)[],
    c34_bitvaryingarray bit varying(10)[],
    c35_boolarray boolean[],
    c36_chararray character(1)[],
    c37_cidrarray cidr[],
    c38_datearray date[],
    c39_numericarray numeric[],
    c40_float8array double precision[],
    c41_inetarray inet[],
    c42_int4array integer[],
    c43_intervalarray interval[],
    c44_macaddrarray macaddr[],
    c45_moneyarray money[],
    c46_float4array real[],
    c47_smallintarrayn smallint[],
    c48_textarray text[],
    c49_timearray time without time zone[],
    c50_timetzarray time with time zone[],
    c51_timestamparray timestamp without time zone[],
    c52_timestamptzarray timestamp with time zone[],
    c53_uuidzarray uuid[],
    c54_tsqueryarray tsquery[],
    c55_jsonarray json[],
    c57_pointarray point[],
    c58_linesegarray lseg[],
    c59_patharray path[],
    c60_boxarray box[],
    c61_polygonarray polygon[],
    c62_circlearray circle[],
    c63_inc_analyze_composite_array inc_analyze_composite[]
) PARTITION BY RANGE (b) (START (0) END(6) EVERY(3));
CREATE TYPE input_fields AS
(
    varchararray character varying(2)[],
    intarray int[],
    _bigintarray bigint[],
    bitarray bit(1)[],
    bitvaryingarray bit varying(10)[],
    boolarray boolean[],
    chararray character(1)[],
    cidrarray cidr[],
    datearray date[],
    numericarray numeric[],
    float8array double precision[],
    inetarray inet[],
    int4array integer[],
    intervalarray interval[],
    macaddrarray macaddr[],
    moneyarray money[],
    float4array real[],
    smallintarrayn smallint[],
    textarray text[],
    timearray time without time zone[],
    timetzarray time with time zone[],
    timestamparray timestamp without time zone[],
    timestamptzarray timestamp with time zone[],
    uuidzarray uuid[],
    tsqueryarray tsquery[],
    jsonarray json[],
    pointarray point[],
    linesegarray lseg[],
    patharray path[],
    boxarray box[],
    polygonarray polygon[],
    circlearray circle[],
    inc_analyze_composite_array inc_analyze_composite[]
);

CREATE OR REPLACE FUNCTION get_input_fields(i INT) RETURNS input_fields AS $$
DECLARE
    fields input_fields;
BEGIN
    SELECT
        ARRAY['a' || (i % 6), 'b' || (i % 6)]::character varying(2)[],
        ARRAY[i, i % 6]::int[],
        ARRAY[(i % 6), ((i + 1) % 6)]::bigint[],
        ARRAY[i, (i + 1)]::bit[],
        ARRAY[(i % 6)::bit(10), ((i + 1) % 6)::bit(10)]::bit varying(10)[],
        ARRAY[(i % 2), ((i + 1) % 2)]::bool[],
        ARRAY['a' || i, 'b' || i]::char[],
        ARRAY['192.168.100.' || (i % 6), '192.168.101.' || (i % 6)]::cidr[],
        ARRAY['2018-01-' || ((i % 6) + 1), '2018-01-' || ((i % 6) + 1)]::date[],
        ARRAY[(i % 6), ((i + 1) % 6)]::numeric[],
        ARRAY[(i % 6), ((i + 1) % 6)]::float8[],
        ARRAY['192.168.100.' || (i % 6), '192.168.100.' || ((i + 1) % 6)]::inet[],
        ARRAY[(i % 6), ((i + 1) % 6)]::int4[],
        '{"1 hour", "1 hour"}'::interval[],
        ARRAY['08:00:2b:01:02:' || (i % 6), '08:00:2b:01:02:' || (i % 6)]::macaddr[],
        ARRAY['123.4', '234.5']::money[],
        ARRAY[(i % 6), ((i + 1) % 6)]::float4[],
        ARRAY[(i % 6), ((i + 1) % 6)]::smallint[],
        ARRAY['abcd' || (i % 6), 'def' || ((i + 1) % 6)]::text[],
        ARRAY[(i % 6) || ':00:00', ((i + 1) % 6) || ':00:00']::time[],
        ARRAY[(i % 6) || ':00:59 PST', ((i + 1) % 6) || ':00:59 EST']::timetz[],
        ARRAY['2018-01-01 ' || (i % 6) || ':59:00', '2018-01-01 ' || ((i + 1) % 6) || ':59:00']::timestamp[],
        ARRAY['2018-01-01 ' || (i % 6) || ':59:00 PST', '2018-01-01 ' || ((i + 1) % 6) || ':59:00 EST']::timestamptz[],
        ARRAY['11111111-1111-1111-1111-1111111111' || (i % 6) || (i % 6), '11111111-1111-1111-1111-1111111111' || ((i + 1) % 6) || ((i + 1) % 6)]::uuid[],
        ARRAY['foo' || (i % 6) || ' & rat', 'rat' || (i % 6) || ' & foo']::tsquery[],
        ARRAY['{"a": "b"}', '{"c": "d"}']::json[],
        ARRAY[point(i, i + 1), point(i + 2, i + 3)],
        ARRAY[lseg(point(i, i + 1), point(i + 2, i + 3)), lseg(point(i + 4, i + 5), point(i + 6, i + 7))],
        ARRAY[path(polygon(box(point(i + 1, i + 2), point(i + 3, i + 4)))), path(polygon(box(point(i + 1, i + 2), point(i + 3, i + 4))))],
        ARRAY[box(point(i + 1, i + 2), point(i + 3, i + 4)), box(point(i + 1, i + 2), point(i + 3, i + 4))],
        ARRAY[polygon(box(point(i + 1, i + 2), point(i + 3, i + 4))), polygon(box(point(i + 1, i + 2), point(i + 3, i + 4)))], 
        ARRAY[circle(point(i + 1, i + 2), i + 3), circle(point(i + 4, i + 5), i + 6)],
        ARRAY[ROW(i % 6, (i + 1) % 6), ROW(i % 6, (i + 1) % 6)]::inc_analyze_composite[]
    INTO fields;

    RETURN fields;
END;
$$ LANGUAGE PLPGSQL;
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
('foo'||(i%6)::text||' & rat'||(i%6)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i+200)::text||' & rat'||(i+200)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i%4)::text||' & rat'||(i%4)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i%4)::text||' & rat'||(i%4)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i%4+61)::text||' & rat'||(i%4+61)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i%4+61)::text||' & rat'||(i%4+61)::text)::tsquery,
(get_input_fields(i)).*
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
('foo'||(i)::text||' & rat'||(i)::text)::tsquery,
(get_input_fields(i)).*
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
-- Test ANALYZE auto merge behavior
-- Do not merge stats from only one partition while other partitions have not been analyzed yet
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE foo_1_prt_1;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge stats from both partitions
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- No stats after merging 
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge stats from only one partition
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
SET allow_system_table_mods=true;
UPDATE pg_attribute SET attstattarget=0 WHERE attrelid = 'foo_1_prt_1'::regclass and ATTNAME in ('a','b','c');
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Merge stats from only one partition one column
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i, i%6, i%6 FROM generate_series(1,100)i; 
ANALYZE foo_1_prt_1(c);
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;

-- Test merging of leaf stats when one partition has
-- FULL SCAN HLL and the other has HLL from sample
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO FOO SELECT i,i%6 FROM generate_series(1,1000)i;
ANALYZE foo_1_prt_1;
ANALYZE FULLSCAN foo_1_prt_2;
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
SET log_statement='none';
SET client_min_messages = 'log';
-- Insert a new column that is not analyzed in the leaf partitions.
-- Analyzing root partition will use merging statistics for the first 2 columns, 
-- will create a sample for the root to analyze the newly added columns since 
-- the leaf partitions does not have any stats for it, yet
ALTER TABLE foo ADD COLUMN c int;
INSERT INTO foo SELECT i, i%9, i%100 FROM generate_series(1,500)i;
-- start_matchsubs
-- m/gp_acquire_sample_rows([^,]+, [^,]+, .+)/
-- s/gp_acquire_sample_rows([^,]+, [^,]+, .+)/gp_acquire_sample_rows()/
-- end_matchsubs
ANALYZE VERBOSE rootpartition foo;
-- Testing auto merging root statistics for all columns
-- where column attnums are differents due to dropped columns
-- and split partitions.
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text, d int)
	DISTRIBUTED BY (d) 
	PARTITION BY RANGE (a) 
		(START (0) END (8) EVERY (4), 
		DEFAULT PARTITION def_part);
INSERT INTO foo SELECT i%13, i, 'something'||i::text, i%121 FROM generate_series(1,1000)i;
ALTER TABLE foo DROP COLUMN b;
ALTER TABLE foo SPLIT DEFAULT PARTITION START (8) END (12) INTO (PARTITION new_part, default PARTITION);
set client_min_messages to 'log';
ANALYZE foo_1_prt_2;
ANALYZE foo_1_prt_3;
ANALYZE foo_1_prt_new_part;
ANALYZE foo_1_prt_def_part;
reset client_min_messages;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Testing auto merging root statistics for a column whose attnum
-- is aligned and the same in every partition due to dropped columns
-- and split partitions.
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text, d int)
	DISTRIBUTED BY (d) 
	PARTITION BY RANGE (a) 
		(START (0) END (8) EVERY (4), 
		DEFAULT PARTITION def_part);
INSERT INTO foo SELECT i%13, i, 'something'||i::text, i%121 FROM generate_series(1,1000)i;
ALTER TABLE foo DROP COLUMN b;
ALTER TABLE foo SPLIT DEFAULT PARTITION START (8) END (12) INTO (PARTITION new_part, default PARTITION);
set client_min_messages to 'log';
ANALYZE foo_1_prt_2(a);
ANALYZE foo_1_prt_3(a);
ANALYZE foo_1_prt_new_part(a);
ANALYZE foo_1_prt_def_part(a);
reset client_min_messages;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Testing auto merging root statistics for a column whose attnum
-- is not aligned and different in partitions due to dropped columns
-- and split partitions.
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text, d int)
	DISTRIBUTED BY (d) 
	PARTITION BY RANGE (a) 
		(START (0) END (8) EVERY (4), 
		DEFAULT PARTITION def_part);
INSERT INTO foo SELECT i%13, i, 'something'||i::text, i%121 FROM generate_series(1,1000)i;
ALTER TABLE foo DROP COLUMN b;
ALTER TABLE foo SPLIT DEFAULT PARTITION START (8) END (12) INTO (PARTITION new_part, default PARTITION);
set client_min_messages to 'log';
ANALYZE foo_1_prt_2(d);
ANALYZE foo_1_prt_3(d);
ANALYZE foo_1_prt_new_part(d);
ANALYZE foo_1_prt_def_part(d);
reset client_min_messages;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Testing ANALYZE ROOTPARTITION and when optimizer_analyze_root_partition is off
-- for incremental analyze.
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text)
	PARTITION BY RANGE (b) 
		(START (0) END (8) EVERY (4));
INSERT INTO foo SELECT i%130, i%8, 'something'||i::text FROM generate_series(1,1000)i;
set optimizer_analyze_root_partition=off;
set client_min_messages to 'log';
-- ANALYZE ROOTPARTITION will sample the table and compute statistics since there
-- is not stats to be merged in the leaf partitions
ANALYZE ROOTPARTITION foo;
reset client_min_messages;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
set client_min_messages to 'log';
-- ANALYZE ROOT PARTITION will piggyback on the stats collected from the leaf and merge them
ANALYZE ROOTPARTITION foo;
reset client_min_messages;
reset optimizer_analyze_root_partition;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
-- Testing that auto merge will be disabled when optimizer_analyze_root_partition
-- is off for incremental analyze. 
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c text)
	PARTITION BY RANGE (b) 
		(START (0) END (8) EVERY (4));
INSERT INTO foo SELECT i%130, i%8, 'something'||i::text FROM generate_series(1,1000)i;
set optimizer_analyze_root_partition=off;
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
reset optimizer_analyze_root_partition;
-- Test incremental analyze on a partitioned table with different storage type and compression algorithm
DROP TABLE IF EXISTS incr_analyze_test;
CREATE TABLE incr_analyze_test (
    a integer,
    b character varying,
    c date
)
WITH (appendonly=true, orientation=row) DISTRIBUTED BY (a) PARTITION BY RANGE(c)
          (
          START ('2018-01-01'::date) END ('2018-01-02'::date) EVERY ('1 day'::interval) WITH (tablename='incr_analyze_test_1_prt_1', appendonly=true, compresslevel=3, orientation=column, compresstype=ZLIB ),
          START ('2018-01-02'::date) END ('2018-01-03'::date) EVERY ('1 day'::interval) WITH (tablename='incr_analyze_test_1_prt_2', appendonly=true, compresslevel=1, orientation=column, compresstype=RLE_TYPE ),
          START ('2018-01-03'::date) END ('2018-01-04'::date) EVERY ('1 day'::interval) WITH (tablename='incr_analyze_test_1_prt_3', appendonly=true, compresslevel=1, orientation=column, compresstype=ZLIB ),
          START ('2018-01-04'::date) END ('2018-01-05'::date) EVERY ('1 day'::interval) WITH (tablename='incr_analyze_test_1_prt_4', appendonly=true, compresslevel=1, orientation=row, compresstype=ZLIB ),
          START ('2018-01-05'::date) END ('2018-01-06'::date) EVERY ('1 day'::interval) WITH (tablename='incr_analyze_test_1_prt_5', appendonly=true, compresslevel=1, orientation=row, compresstype=ZLIB ),
          START ('2018-01-06'::date) END ('2018-01-07'::date) EVERY ('1 day'::interval) WITH (tablename='incr_analyze_test_1_prt_6', appendonly=false)
          );

INSERT INTO incr_analyze_test VALUES (1, 'a', '2018-01-01');
ANALYZE incr_analyze_test;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'incr_analyze_test%' ORDER BY attname,tablename;
INSERT INTO incr_analyze_test SELECT s, md5(s::varchar), '2018-01-02' FROM generate_series(1, 1000) AS s;
ANALYZE incr_analyze_test_1_prt_2;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'incr_analyze_test%' ORDER BY attname,tablename;
SELECT relname, relpages, reltuples FROM pg_class WHERE relname LIKE 'incr_analyze_test%' ORDER BY relname;
-- Test merging of stats if an empty partition contains relpages > 0
-- Do not collect samples while merging stats
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) PARTITION BY RANGE (b) (START (0) END (6) EVERY (3));
INSERT INTO foo SELECT i,i%3 FROM generate_series(1,10)i;
ANALYZE foo_1_prt_1;
ANALYZE foo_1_prt_2;
SET allow_system_table_mods = on;
UPDATE pg_class set relpages=3 WHERE relname='foo_1_prt_2';
RESET allow_system_table_mods;
analyze verbose rootpartition foo;
-- ensure relpages is correctly set after analyzing
analyze foo_1_prt_2;
select reltuples, relpages from pg_class where relname ='foo_1_prt_2';
-- Test application of column-wise statistics setting to the number of MCVs and histogram bounds on partitioned table
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int) PARTITION BY RANGE (a) (START (0) END (10) EVERY (5));
-- fill foo with even numbers twice as large than odd ones to avoid fully even distribution of 'a' attribute and hence empty MCV/MCF
INSERT INTO foo SELECT i%10 FROM generate_series(0, 100) i;
INSERT INTO foo SELECT i%10 FROM generate_series(0, 100) i WHERE i%2 = 0;
-- default_statistics_target is 4
ALTER TABLE foo ALTER COLUMN a SET STATISTICS 5;
ANALYZE foo;
SELECT array_length(most_common_vals, 1), array_length(most_common_freqs, 1), array_length(histogram_bounds, 1) FROM pg_stats WHERE tablename = 'foo' AND attname = 'a';

-- Make sure a simple heap table does not store HLL values
CREATE TABLE simple_table_no_hll (a int);
INSERT INTO simple_table_no_hll SELECT generate_series(1,10);
ANALYZE simple_table_no_hll;
SELECT staattnum, stakind1, stakind2, stakind3, stakind4, stakind5,
       stavalues1, stavalues2, stavalues3, stavalues4, stavalues5
FROM pg_statistic WHERE starelid = 'simple_table_no_hll'::regclass;

-- Make sure analyze rootpartition option works in an option list
set optimizer_analyze_root_partition to off;
DROP TABLE IF EXISTS foo;
CREATE TABLE foo(a int) PARTITION BY RANGE(a) (start (0) INCLUSIVE END (20) EVERY (10));
INSERT INTO foo values (5),(15);
ANALYZE (verbose, rootpartition off) foo;

-- root should not have stats
SELECT count(*) from pg_statistic where starelid='foo'::regclass;

-- root should have stats
ANALYZE (verbose, rootpartition on) foo;
SELECT count(*) from pg_statistic where starelid='foo'::regclass;

-- Make sure analyze hll fullscan option works in an option list
ANALYZE (verbose, fullscan on) foo;
ANALYZE (verbose, fullscan off) foo;
reset optimizer_analyze_root_partition;

-- Test merging of stats after the last partition is analyzed. Merging should
-- be done for root without taking a sample from root if one of the column
-- statistics collection is turned off
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int, c gp_hyperloglog_estimator) PARTITION BY RANGE (b) (START (1) END (3) EVERY (1));
SET gp_autostats_mode=none;
ALTER TABLE foo ALTER COLUMN c SET STATISTICS 0;
INSERT INTO foo SELECT i,i%2+1, NULL FROM generate_series(1,100)i;
ANALYZE VERBOSE foo_1_prt_1;
ANALYZE VERBOSE foo_1_prt_2;
SELECT tablename, attname, null_frac, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename like 'foo%' ORDER BY attname,tablename;
RESET gp_autostats_mode;

-- analyze in transaction should merge leaves instead of resampling
drop table if exists foo;
create table foo (a int, b date) distributed by (a) partition by range(b) (partition "20210101" start ('20210101'::date) end ('20210201'::date), partition "20210201" start ('20210201'::date) end ('20210301'::date), partition "20210301" start ('20210301'::date) end ('20210401'::date));
insert into foo select a, '20210101'::date+a from (select generate_series(1,80) a) t1;
analyze verbose foo;

-- we should see "analyzing "public.foo" inheritance tree" in the output below
begin;
truncate foo_1_prt_20210201;
insert into foo select a, '20210101'::date+a from (select generate_series(31,40) a) t1;
analyze verbose foo_1_prt_20210201;
rollback;

-- test behavior of "analyze_hll_non_part_table" create table option
create table hll_part (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
insert into hll_part select i, i%9+1 from generate_series(1,100)i;
analyze hll_part;

create table hll_default_heap (i int, j int) distributed by (i);
select reloptions from pg_class where relname='hll_default_heap';
insert into hll_default_heap values (777,6);
analyze hll_default_heap; -- hll stats should not be collected in non-partition heap table by default
select 1 from pg_statistic where starelid='hll_default_heap'::regclass and stavalues5 is not null;

create table hll_default_ao (i int, j int) with (appendonly=true) distributed by (i);
select reloptions from pg_class where relname='hll_default_ao';
insert into hll_default_ao values (777,6);
analyze hll_default_ao;
-- hll stats should not be collected in non-partition ao table by default
select 1 from pg_statistic where starelid='hll_default_ao'::regclass and stavalues5 is not null;

create table hll_off (i int, j int) with (analyze_hll_non_part_table=false) distributed by (i);
select reloptions from pg_class where relname='hll_off';
insert into hll_off values (777,6);
analyze hll_off;
-- hll stats should not be collected in non-partition table when disabled
select 1 from pg_statistic where starelid='hll_off'::regclass and stavalues5 is not null;

-- alter table, set analyze_hll_non_part_table on, ensure hll stats collected
alter table hll_off set (analyze_hll_non_part_table=true);
select reloptions from pg_class where relname='hll_off';
analyze hll_off;
select 1 from pg_statistic where starelid='hll_off'::regclass and stavalues5 is not null;

-- alter table, set analyze_hll_non_part_table off, ensure hll stats not collected
alter table hll_off set (analyze_hll_non_part_table=false);
select reloptions from pg_class where relname='hll_off';
analyze hll_off;
select 1 from pg_statistic where starelid='hll_off'::regclass and stavalues5 is not null;

create table hll_on_ao (i int, j int) with (analyze_hll_non_part_table=true) distributed by (i);
select reloptions from pg_class where relname='hll_on_ao';
insert into hll_on_ao values (777,6);
analyze hll_on_ao;
-- hll stats should be collected in non-partition AO table when enabled
select 1 from pg_statistic where starelid='hll_on_ao'::regclass and stavalues5 is not null;

create table hll_on_heap (i int, j int) with (analyze_hll_non_part_table=true) distributed by (i);
select reloptions from pg_class where relname='hll_on_heap';
insert into hll_on_heap values (777,6);
analyze hll_on_heap;
-- hll stats should be collected in non-partition heap table when enabled
select 1 from pg_statistic where starelid='hll_on_heap'::regclass and stavalues5 is not null;

alter table hll_part exchange partition for (6)  with table hll_on_heap;
select reloptions from pg_class where relname='hll_part_1_prt_6';

-- hll stats should be present after partition exchange of table with "analyze_hll_non_part_table" enabled
select 1 from pg_statistic where starelid='hll_part_1_prt_6'::regclass and stavalues5 is not null;

-- verify that reloption is correctly set for partitioned table
create table hll_part_def (i int, j int) with (analyze_hll_non_part_table=false) distributed by (i)
partition by range(j)
(start(1) end(3) every(1));

select reloptions from pg_class where relname='hll_part_def';
select reloptions from pg_class where relname='hll_part_def_1_prt_2';

-- see if altering the reloption at the partition root with the ONLY keyword
-- only modifies the reloption for the root and future children (should NOT
-- touch existing children)
alter table only hll_part_def set (analyze_hll_non_part_table=true);
select reloptions from pg_class where relname='hll_part_def';
select reloptions from pg_class where relname='hll_part_def_1_prt_2';

-- see if altering the reloption at the partition root without the ONLY keyword
-- modifies the reloption for all existing children AND all future children
alter table hll_part_def set (analyze_hll_non_part_table=true);
select reloptions from pg_class where relname='hll_part_def';
select reloptions from pg_class where relname='hll_part_def_1_prt_2';

