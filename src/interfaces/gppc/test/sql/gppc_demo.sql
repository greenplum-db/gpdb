set timezone=UTC;
-- Data types testing
DROP FUNCTION IF EXISTS oidcheckfunc(text);
DROP FUNCTION IF EXISTS boolfunc(bool);
DROP FUNCTION IF EXISTS charfunc("char");
DROP FUNCTION IF EXISTS int2mulfunc(int2, int2);
DROP FUNCTION IF EXISTS int4func1(int);
DROP FUNCTION IF EXISTS int4func1();
DROP FUNCTION IF EXISTS int4func1(int, int);
DROP FUNCTION IF EXISTS int8plusfunc(int8, int8);
DROP FUNCTION IF EXISTS float4func1(float4);
DROP FUNCTION IF EXISTS float8func1(float8);
DROP FUNCTION IF EXISTS textdoublefunc(text);
DROP FUNCTION IF EXISTS textgenfunc();
DROP FUNCTION IF EXISTS textcopyfunc(text, bool);
DROP FUNCTION IF EXISTS varchardoublefunc(varchar);
DROP FUNCTION IF EXISTS varchargenfunc();
DROP FUNCTION IF EXISTS varcharcopyfunc(text, bool);
DROP FUNCTION IF EXISTS bpchardoublefunc(char);
DROP FUNCTION IF EXISTS bpchargenfunc();
DROP FUNCTION IF EXISTS bpcharcopyfunc(text, bool);
DROP FUNCTION IF EXISTS byteafunc1(bytea);
DROP FUNCTION IF EXISTS byteafunc2(bytea);
DROP FUNCTION IF EXISTS argisnullfunc(int);
DROP FUNCTION IF EXISTS gppc_func_text(text,bool);
DROP FUNCTION IF EXISTS gppc_func_text(text);
DROP FUNCTION IF EXISTS gppc_func_text();
DROP FUNCTION IF EXISTS gppc_func_varchar(varchar,bool);
DROP FUNCTION IF EXISTS gppc_func_varchar(varchar);
DROP FUNCTION IF EXISTS gppc_func_varchar();

CREATE OR REPLACE FUNCTION oidcheckfunc(text) RETURNS int4 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION boolfunc(bool) RETURNS bool AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION charfunc("char") RETURNS "char" AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION int2mulfunc(int2, int2) RETURNS int2 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION int4func1(int) RETURNS int AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION int4func1() RETURNS int AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION int4func1(int, int) RETURNS int AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION int8plusfunc(int8, int8) RETURNS int8 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION float4func1(float4) RETURNS float4 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION float8func1(float8) RETURNS float8 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION textdoublefunc(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION textgenfunc() RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION textcopyfunc(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION varchardoublefunc(varchar) RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION varchargenfunc() RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION varcharcopyfunc(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION bpchardoublefunc(char) RETURNS char AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION bpchargenfunc() RETURNS char AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION bpcharcopyfunc(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION byteafunc1(bytea) RETURNS bytea AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION byteafunc2(bytea) RETURNS bytea AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION argisnullfunc(int) RETURNS bool AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE;
CREATE OR REPLACE FUNCTION gppc_func_text(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION gppc_func_text(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION gppc_func_text() RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION gppc_func_varchar(varchar, bool) RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION gppc_func_varchar(varchar) RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION gppc_func_varchar() RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;

SELECT oidcheckfunc('bool'),
        oidcheckfunc('char'),
        oidcheckfunc('int2'),
        oidcheckfunc('int4'),
        oidcheckfunc('int8'),
        oidcheckfunc('float4'),
        oidcheckfunc('float8'),
        oidcheckfunc('text'),
        oidcheckfunc('varchar'),
        oidcheckfunc('bpchar'),
        oidcheckfunc('bytea'),
        oidcheckfunc('numeric'),
        oidcheckfunc('time'),
        oidcheckfunc('timetz'),
        oidcheckfunc('timestamp'),
        oidcheckfunc('timestamptz');
SELECT boolfunc(true and true);
SELECT charfunc('a');
SELECT int2mulfunc(2::int2, 3::int2);
SELECT int4func1(10);
SELECT int4func1();
SELECT int4func1(11, 12);
SELECT int8plusfunc(10000000000, 1);
SELECT float4func1(4.2);
SELECT float8func1(0.0000001);
SELECT textdoublefunc('bla');
SELECT textgenfunc();
SELECT textcopyfunc('white', true), textcopyfunc('white', false);
SELECT varchardoublefunc('bla');
SELECT varchargenfunc();
SELECT varcharcopyfunc('white', true), varcharcopyfunc('white', false);
SELECT bpchardoublefunc('bla');
SELECT bpchargenfunc();
SELECT bpcharcopyfunc('white', true), bpcharcopyfunc('white', false);
SELECT argisnullfunc(0), argisnullfunc(NULL);
SELECT gppc_func_text();
SELECT gppc_func_text('This is the first argument passed to the function.');
SELECT gppc_func_text('white', true), gppc_func_text('black', false);
SELECT gppc_func_varchar();
SELECT gppc_func_varchar('This function has one argument.');
SELECT gppc_func_text('This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.This function has one argument.');
SELECT gppc_func_varchar('white', true), gppc_func_varchar('black', false);
SELECT byteafunc1(E'\\244\\233abc');
SELECT byteafunc2(E'\\244\\233abc');

-- Numeric type

DROP FUNCTION IF EXISTS numericfunc1(numeric);
DROP FUNCTION IF EXISTS numericfunc2(numeric);
DROP FUNCTION IF EXISTS numericfunc3(float8);
DROP FUNCTION IF EXISTS numericdef1(int4);

CREATE OR REPLACE FUNCTION numericfunc1(numeric) RETURNS numeric AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION numericfunc2(numeric) RETURNS float8 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION numericfunc3(float8) RETURNS numeric AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION numericdef1(int4) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;

DROP TABLE IF EXISTS numerictable;
CREATE TABLE numerictable(
	a numeric(5, 2),
	b numeric(3),
	c numeric
);

SELECT numericfunc1(1000);
SELECT numericfunc2(1000.00001);
SELECT numericfunc3(1000.00001);
SELECT attname, numericdef1(atttypmod) FROM pg_attribute
	WHERE attrelid = 'numerictable'::regclass and atttypid = 'numeric'::regtype;
-- Encoding name
DROP FUNCTION IF EXISTS test_encoding_name(int);
CREATE OR REPLACE FUNCTION test_encoding_name(int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE;
SELECT
       test_encoding_name(0),
       test_encoding_name(1),
       test_encoding_name(2),
       test_encoding_name(3),
       test_encoding_name(4),
       test_encoding_name(5),
       test_encoding_name(6),
       test_encoding_name(7),
       test_encoding_name(8),
       test_encoding_name(9),
       test_encoding_name(10),
       test_encoding_name(11),
       test_encoding_name(12),
       test_encoding_name(13),
       test_encoding_name(14),
       test_encoding_name(15),
       test_encoding_name(16),
       test_encoding_name(17),
       test_encoding_name(18),
       test_encoding_name(19),
       test_encoding_name(20),
       test_encoding_name(21),
       test_encoding_name(22),
       test_encoding_name(23),
       test_encoding_name(24),
       test_encoding_name(25),
       test_encoding_name(26),
       test_encoding_name(27),
       test_encoding_name(28),
       test_encoding_name(29),
       test_encoding_name(30),
       test_encoding_name(31),
       test_encoding_name(32),
       test_encoding_name(33),
       test_encoding_name(34);
-- Error function and error callback
DROP FUNCTION IF EXISTS errfunc1(text);
DROP FUNCTION IF EXISTS errfunc_varchar(varchar);
DROP FUNCTION IF EXISTS errfunc_bpchar(char);
DROP FUNCTION IF EXISTS errorcallbackfunc1(text);

CREATE OR REPLACE FUNCTION errfunc1(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION errfunc_varchar(varchar) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION errfunc_bpchar(char) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION errorcallbackfunc1(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;

SELECT errfunc1('The quick brown fox jumps over the lazy dog');
SELECT errfunc_varchar('This is to test INFO message using varchar.');
SELECT errfunc_bpchar('This is to test WARNING message using bpchar.');

SELECT errorcallbackfunc1('warning');
SELECT errorcallbackfunc1('error');
SELECT errorcallbackfunc1('notice'); 
-- Date and timestamp
DROP FUNCTION IF EXISTS datefunc1_nochange(date);
DROP FUNCTION IF EXISTS datefunc1(date);
DROP FUNCTION IF EXISTS datefunc2(date);
DROP FUNCTION IF EXISTS datefunc3_year(date);
DROP FUNCTION IF EXISTS datefunc3_mon(date);
DROP FUNCTION IF EXISTS datefunc3_mday(date);
DROP FUNCTION IF EXISTS timefunc1(time);
DROP FUNCTION IF EXISTS timetzfunc1(timetz);
DROP FUNCTION IF EXISTS timestampfunc1(timestamp);
DROP FUNCTION IF EXISTS timestamptzfunc1(timestamptz);

CREATE OR REPLACE FUNCTION datefunc1_nochange(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION datefunc1(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION datefunc2(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION datefunc3_year(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION datefunc3_mon(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION datefunc3_mday(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION timefunc1(time) RETURNS time AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION timetzfunc1(timetz) RETURNS timetz AS '$libdir/gppc_test' LANGUAGE c STABLE STRICT;
CREATE OR REPLACE FUNCTION timestampfunc1(timestamp) RETURNS timestamp AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION timestamptzfunc1(timestamptz) RETURNS timestamptz AS '$libdir/gppc_test' LANGUAGE c STABLE STRICT;

SELECT datefunc1_nochange('1900-01-01');
SELECT datefunc1('1898-12-31');
SELECT datefunc1('2012-02-29');
SELECT datefunc2('2013-03-01');
SELECT datefunc3_year('1900-01-01');
SELECT datefunc3_year('00-14-37');
SELECT datefunc3_year('02-11-03');
SELECT datefunc3_mon('2012-01-29');
SELECT datefunc3_mon('2012-03-29');
SELECT datefunc3_mon('2011-03-29');
SELECT datefunc3_mday('2012-03-01');
SELECT datefunc3_mday('2013-03-01');
SELECT datefunc3_mday('1900-01-01');
SELECT timefunc1('15:00:01');
SELECT timetzfunc1('15:00:01 UTC');
SELECT timestampfunc1('2011-02-24 15:00:01');
SELECT timestamptzfunc1('2011-02-24 15:00:01 UTC');
-- SPI test
DROP FUNCTION IF EXISTS spifunc1(text, int);
DROP FUNCTION IF EXISTS spifunc2(text, text);
DROP FUNCTION IF EXISTS spifunc3(text, int);
DROP FUNCTION IF EXISTS spifunc4(text, text);
DROP FUNCTION IF EXISTS spifunc5(text, int, int);
DROP FUNCTION IF EXISTS spifunc5a(text, int, int);
DROP FUNCTION IF EXISTS spifunc6(text, int, int);

CREATE OR REPLACE FUNCTION spifunc1(text, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc2(text, text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc3(text, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc4(text, text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc5(text, int, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc5a(text, int, int) RETURNS int AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc6(text, int, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;

SELECT spifunc1($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2);
SELECT spifunc2($$select i, i * 2 as val from generate_series(1, 10)i order by 1$$, 'val');
SELECT spifunc3($$select i, 'foo' || i as val from generate_series(1, 10)i order by 1$$, 2);
SELECT spifunc4($$select i, 'foo' || i as val from generate_series(1, 10)i order by 1$$, 'val');

-- multiple queries in one query string
SELECT spifunc1($$select i, i * 2 from generate_series(1, 10)i order by 1; select 1+1; select 1+2$$, 1);

-- access table
DROP TABLE IF EXISTS spi_test CASCADE;
CREATE TABLE spi_test (a int, b text);
INSERT INTO spi_test (SELECT a, 'foo'||a FROM generate_series(1, 10) a);
SELECT spifunc1($$select * from spi_test order by a limit 5$$, 2);

-- access view
DROP VIEW IF EXISTS spi_view;
CREATE VIEW spi_view AS 
select * from spi_test order by a limit 5;
SELECT spifunc1($$select * from spi_view$$, 2);

-- join table and view
SELECT spifunc1($$select * from spi_test, spi_view where spi_test.a = spi_view.a order by spi_test.a$$, 2);

-- using sub-query
SELECT spifunc1($$select * from spi_test where spi_test.a in (select a from spi_view) order by spi_test.a$$, 2);

-- recursive SPI function call
SELECT spifunc1($$select (
	SELECT spifunc1('select * from spi_test, spi_view where spi_test.a = spi_view.a order by spi_test.a', 2)a
)$$, 1);

-- DDL: create table
SELECT spifunc1($$create table spi_test2 (a int, b text)$$, 1);
\d spi_test2

-- DDL: alter table
SELECT spifunc1($$alter table spi_test2 add column c int$$, 1);
\d spi_test2

-- DDL: drop table
SELECT spifunc1($$drop table spi_test2$$, 1);
\dt spi_test2

-- When tcount = 0, no limit
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, 0);
-- When tcount = 5, which is less than total 10 records
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, 5);
-- When tcount = 20, which is greater than total 10 records
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, 20);
-- When tcount = -1, returns null
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, -1);

-- insert a record with 400K bytes
---- insert into spi_test values(31, repeat('test',100000));
-- SPI GppcSPIGetValue makecopy = true for long text 400K bytes
---- SELECT spifunc1($$select * from spi_test order by a $$, 2);

-- SPI GppcSPIGetValue makecopy = false
SELECT spifunc5a($$SELECT * FROM spi_test ORDER BY 1$$, 2, 10);
SELECT spifunc5a($$SELECT * FROM spi_test ORDER BY 1$$, 2, 5);
SELECT spifunc5a($$SELECT * FROM spi_test ORDER BY 1$$, 2, 0);

-- SPI GppcSPIExec select into table
DROP TABLE IF EXISTS spi_test3;
SELECT spifunc6($$select i, 'foo'||i into spi_test3 from generate_series(10,15) i$$,0,0);
SELECT * FROM spi_test3 ORDER BY 1;

-- SPI GppcSPIExec CTAS
DROP TABLE IF EXISTS spi_test4;
SELECT spifunc6($$create table spi_test4 as select i, 'foo'||i from generate_series(1,10) i$$,0,0);
SELECT * FROM spi_test4 ORDER BY 1;

-- SPI truncate
SELECT spifunc1($$truncate spi_test$$, 1);
-- After truncate
SELECT * FROM spi_test order by a;

-- DML: insert
SELECT spifunc6($$insert into spi_test select i, 'foo'||i from generate_series(1, 5) i$$, 0, 0);
SELECT spifunc6($$insert into spi_test values (6, 'foo6')$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: update
SELECT spifunc6($$update spi_test set b = 'boo' ||a$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: delete
SELECT spifunc6($$delete from spi_test where a > 5$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: insert using tcount=3, notice tcount takes no effect
SELECT spifunc6($$insert into spi_test select i, 'foo'||i from generate_series(6, 10) i$$, 3, 0);
SELECT * from spi_test order by a;

-- DML: update using tcount=3, notice tcount takes no effect
SELECT spifunc6($$update spi_test set b = 'boo' ||a$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: delete using tcounti=3, notice tcount takes no effect
SELECT spifunc6($$delete from spi_test where a > 5$$, 3, 0);
SELECT * from spi_test order by a;

-- DML: create, alter, and drop index
SELECT spifunc6($$CREATE INDEX spi_idx1 ON spi_test (a, b)$$, 0, 0);
\d spi_idx1
SELECT spifunc6($$ALTER INDEX spi_idx1 RENAME TO spi_idx2$$, 0, 0);
\d spi_idx2
SELECT spifunc6($$DROP INDEX spi_idx2$$, 0 , 0);
\d spi_idx2

-- Negative: connect again when already connected
SELECT spifunc6($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2, 1);
-- Negative: try to execute without connection
SELECT spifunc6($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2, 2);
-- Negative: close connection again when it has been closed already
SELECT spifunc6($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2, 3);
