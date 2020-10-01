--
-- Test the boundaries of some built-in datatypes
--

set datestyle to 'ISO, DMY';
set intervalstyle to 'postgres';

--
-- bit varying
--
DROP TABLE IF EXISTS dml_bitvarying;
CREATE TABLE dml_bitvarying(a bit varying(2)) distributed by (a);

-- Simple DML
INSERT INTO dml_bitvarying VALUES('11');
SELECT * FROM dml_bitvarying ORDER BY 1;
UPDATE dml_bitvarying SET a = '00';
SELECT * FROM dml_bitvarying ORDER BY 1;

-- out of range values
INSERT INTO dml_bitvarying VALUES('111');
SELECT * FROM dml_bitvarying ORDER BY 1;
UPDATE dml_bitvarying SET a = '000';
SELECT * FROM dml_bitvarying ORDER BY 1;

--
-- Interval
--
DROP TABLE IF EXISTS dml_interval;
CREATE TABLE dml_interval(a interval) distributed by (a);
-- SIMPLE INSERTS
INSERT INTO dml_interval VALUES('178000000 years');
SELECT * FROM dml_interval ORDER BY 1;
INSERT INTO dml_interval VALUES('-178000000 years');
SELECT * FROM dml_interval ORDER BY 1;
UPDATE dml_interval SET a = '-178000000 years';
SELECT * FROM dml_interval ORDER BY 1;

-- out of range values
INSERT INTO dml_interval VALUES('178000000 years 1 month');
SELECT * FROM dml_interval ORDER BY 1;
INSERT INTO dml_interval VALUES('-178000000 years 1 month');
SELECT * FROM dml_interval ORDER BY 1;
UPDATE dml_interval SET a = '-178000000 years 1 month';
SELECT * FROM dml_interval ORDER BY 1;

--
-- Numeric
--
DROP TABLE IF EXISTS dml_numeric;
CREATE TABLE dml_numeric(a numeric) distributed by (a);

-- Simple DML
INSERT INTO dml_numeric VALUES (10e+1000);
SELECT * FROM dml_numeric ORDER BY 1;
INSERT INTO dml_numeric VALUES (1e-1000);
SELECT * FROM dml_numeric ORDER BY 1;

-- GPDB_94_MERGE_FIXME: after 94_stable, these are not out of range
-- out of range values
INSERT INTO dml_numeric VALUES (1e+10000);
SELECT * FROM dml_numeric ORDER BY 1;
UPDATE dml_numeric SET a = 1e+10000;
SELECT * FROM dml_numeric ORDER BY 1;

--
-- Numeric, with a typmod
--
DROP TABLE IF EXISTS dml_numeric2;
CREATE TABLE dml_numeric2(a numeric(5,2)) distributed by (a);

-- Simple DML
INSERT  INTO  dml_numeric2 VALUES (1.00e+2);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+1;
SELECT * FROM dml_numeric2 ORDER BY 1;

-- out of range values
INSERT  INTO  dml_numeric2 VALUES (1.00e+3);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+3;
SELECT * FROM dml_numeric2 ORDER BY 1;

--
-- Timestamp without timezone
--
DROP TABLE IF EXISTS dml_timestamp;
CREATE TABLE dml_timestamp( a timestamp) distributed by (a);

-- Simple DML
INSERT INTO dml_timestamp VALUES (to_date('2012-02-21', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamp ORDER BY 1;
INSERT INTO dml_timestamp VALUES (to_date('4714-01-27 AD', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamp ORDER BY 1;
UPDATE dml_timestamp SET a = to_date('2012-02-21', 'YYYY-MM-DD BC');
SELECT * FROM dml_timestamp ORDER BY 1;

-- out of range values
INSERT INTO dml_timestamp VALUES ('294277-01-27 AD'::timestamp);
SELECT * FROM dml_timestamp ORDER BY 1;
UPDATE dml_timestamp SET a = '294277-01-27 AD'::timestamp;
SELECT * FROM dml_timestamp ORDER BY 1;
INSERT INTO dml_timestamp VALUES ('2012-02-31 AD'::timestamp);
SELECT * FROM dml_timestamp ORDER BY 1;

-- Greenplum 4.3 and 5 had support for YYYYMMDDHH24MISS, which was removed in
-- v6 since it cannot be parsed without ambiguity. This test should fail.
SELECT '13081205132018'::timestamp;

--
-- Timestamp with timezone
--
DROP TABLE IF EXISTS dml_timestamptz;
CREATE TABLE dml_timestamptz( a timestamptz) distributed by (a);

-- Simple DML
INSERT INTO dml_timestamptz VALUES (to_date('4714-01-27 AD', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamptz ORDER BY 1;
UPDATE dml_timestamptz SET a = to_date('4714-01-27 AD', 'YYYY-MM-DD BC');
SELECT * FROM dml_timestamptz ORDER BY 1;

-- out of range values
INSERT INTO dml_timestamptz VALUES ('294277-01-27 AD'::timestamptz);
SELECT * FROM dml_timestamptz ORDER BY 1;
INSERT INTO dml_timestamptz VALUES ('4714-01-27 BC'::timestamptz);
SELECT * FROM dml_timestamptz ORDER BY 1;
UPDATE dml_timestamptz SET a = '4714-01-27 BC'::timestamptz;
SELECT * FROM dml_timestamptz ORDER BY 1;


--
-- Tests for implicit conversions between "unknown" and other types.
--

-- Test "unknown" from sub queries - MPP-2510
select foo || 'bar'::text from (select 'bar' as foo) a;
select foo || 'bar'::text from (select 'bar'::text as foo) a;
select * from ( select 'a' as a) x join (select 'b' as b) y on a=b;

-- Test "unknown" with typmod MPP-2658
create table unknown_test (v varchar(20), n numeric(20, 2), t timestamp(2));
insert into unknown_test select '100', '123.23', '2008-01-01 11:11:11';
select 'foo'::varchar(10) || bar from (select 'bar' as bar) moo;
select '123'::numeric(4,1) + bar from (select '123' as bar) baz;
drop table unknown_test;

-- Test nested "unknown"s from MPP-2689
select 'foo'::text || foo from ( select foo from (select 4.5, foo from ( select
1, 'foo' as foo) a ) b ) c;
select 'foo'::text || foo from ( select foo from
 (select foo || bar as foo from ( select 'bar' as bar, 'foo' as foo) a ) b ) c;
create domain u_d as text;
prepare p1 as select $1::u_d || foo from (select 'foo' as foo) a;
prepare p2 as select 'foo' || foo
from (select $1::u_d || bar as foo from (select 'bar' as bar) a ) b;

select 'a' as a, 'b' as b, 'c' as c, 1 as d union select * from (select 'a' as a, 'b' as b, 'c' as c, 1 as d)d;
select * from (select 'a' as a, 'b' as b, 'c' as c, 1 as d)d union select 'a' as a, 'b' as b, 'c' as c, 1 as d;


--
-- Float8: test if you can dump/restore subnormal (1e-323) values using COPY
--
CREATE TABLE FLOATS(a float8);
INSERT INTO FLOATS select 1e-307::float8 / 10^i FROM generate_series(1,16) i;

SELECT * FROM FLOATS ORDER BY a;

SELECT float8in(float8out(a)) FROM FLOATS ORDER BY a;

COPY FLOATS TO '/tmp/floats';
TRUNCATE FLOATS;
COPY FLOATS FROM '/tmp/floats';

SELECT * FROM FLOATS ORDER BY a;
