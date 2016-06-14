CREATE FUNCTION oidcheckfunc(text) RETURNS int4 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION boolfunc(bool) RETURNS bool AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION charfunc("char") RETURNS "char" AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION int2mulfunc(int2, int2) RETURNS int2 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION int4func1(int) RETURNS int AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION int8plusfunc(int8, int8) RETURNS int8 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION float4func1(float4) RETURNS float4 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION float8func1(float8) RETURNS float8 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION textdoublefunc(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION textgenfunc() RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION textcopyfunc(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION varchardoublefunc(varchar) RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION varchargenfunc() RETURNS varchar AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION varcharcopyfunc(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION bpchardoublefunc(char) RETURNS char AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION bpchargenfunc() RETURNS char AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION bpcharcopyfunc(text, bool) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION errfunc1(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION argisnullfunc(int) RETURNS bool AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE;
CREATE FUNCTION byteafunc1(bytea) RETURNS bytea AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION numericfunc1(numeric) RETURNS numeric AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION numericfunc2(numeric) RETURNS float8 AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION numericfunc3(float8) RETURNS numeric AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION numericdef1(int4) RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION datefunc1(date) RETURNS date AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION timefunc1(time) RETURNS time AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION timetzfunc1(timetz) RETURNS timetz AS '$libdir/gppc_test' LANGUAGE c STABLE STRICT;
CREATE FUNCTION timestampfunc1(timestamp) RETURNS timestamp AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION timestamptzfunc1(timestamptz) RETURNS timestamptz AS '$libdir/gppc_test' LANGUAGE c STABLE STRICT;
CREATE FUNCTION spifunc1(text, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE FUNCTION spifunc2(text, text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE FUNCTION spifunc3(text, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE FUNCTION spifunc4(text, text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE FUNCTION errorcallbackfunc1(text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE FUNCTION test_encoding_name() RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE;
CREATE FUNCTION test_reset_current_memory() RETURNS bool AS '$libdir/gppc_test' LANGUAGE c;
CREATE FUNCTION test_reset_child_memory() RETURNS bool AS '$libdir/gppc_test' LANGUAGE c;
CREATE FUNCTION test_interrupt(bool) RETURNS void AS '$libdir/gppc_test' LANGUAGE c;

CREATE TABLE numerictable(
	a numeric(5, 2),
	b numeric(3),
	c numeric
);

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
SELECT errfunc1('The quick brown fox jumps over the lazy dog');
SELECT argisnullfunc(0), argisnullfunc(NULL);
SELECT byteafunc1(E'\\244\\233abc');
SELECT numericfunc1(1000);
SELECT numericfunc2(1000.00001);
SELECT numericfunc3(1000.00001);
SELECT attname, numericdef1(atttypmod) FROM pg_attribute
	WHERE attrelid = 'numerictable'::regclass and atttypid = 'numeric'::regtype;
SELECT datefunc1('2011-02-24');
SELECT timefunc1('15:00:01');
SELECT timetzfunc1('15:00:01 UTC');
SELECT timestampfunc1('2011-02-24 15:00:01');
SELECT timestamptzfunc1('2011-02-24 15:00:01 UTC');
SELECT spifunc1($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2);
SELECT spifunc2($$select i, i * 2 as val from generate_series(1, 10)i order by 1$$, 'val');
SELECT spifunc3($$select i, 'foo' || i as val from generate_series(1, 10)i order by 1$$, 2);
SELECT spifunc4($$select i, 'foo' || i as val from generate_series(1, 10)i order by 1$$, 'val');
SELECT errorcallbackfunc1('warning');
SELECT errorcallbackfunc1('error');
SELECT test_encoding_name();
SELECT test_reset_current_memory();
SELECT test_reset_child_memory();
SELECT test_interrupt(NULL);
SELECT test_interrupt(pg_cancel_backend(pg_backend_pid()));
