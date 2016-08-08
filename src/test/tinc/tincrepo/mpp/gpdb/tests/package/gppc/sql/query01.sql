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

