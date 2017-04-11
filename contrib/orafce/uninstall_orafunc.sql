-- Adjust the following if this is not the schema you installed into
\set ORA_SCHEMA oracompat
set search_path = :ORA_SCHEMA, pg_catalog;

BEGIN;
DROP FUNCTION IF EXISTS nvl(anyelement, anyelement);

DROP FUNCTION IF EXISTS add_months(day date, value int);

DROP FUNCTION IF EXISTS last_day(value date);

DROP FUNCTION IF EXISTS next_day(value date, weekday text);
DROP FUNCTION IF EXISTS next_day(value date, weekday integer);

DROP FUNCTION IF EXISTS months_between(date1 date, date2 date);

DROP FUNCTION IF EXISTS trunc(value date, fmt text);
DROP FUNCTION IF EXISTS trunc(value timestamp with time zone, fmt text);
DROP FUNCTION IF EXISTS trunc(value timestamp with time zone);
DROP FUNCTION IF EXISTS trunc(value date);

DROP FUNCTION IF EXISTS round(value timestamp with time zone, fmt text);
DROP FUNCTION IF EXISTS round(value timestamp with time zone);
DROP FUNCTION IF EXISTS round(value date);
DROP FUNCTION IF EXISTS round(value date, fmt text);

DROP FUNCTION IF EXISTS instr(str text, patt text, start int, nth int);
DROP FUNCTION IF EXISTS instr(str text, patt text, start int);
DROP FUNCTION IF EXISTS instr(str text, patt text);

DROP FUNCTION IF EXISTS reverse(text, int, int);
DROP FUNCTION IF EXISTS reverse(text, int);
DROP FUNCTION IF EXISTS reverse(text);

DROP FUNCTION IF EXISTS concat(text, text);
DROP FUNCTION IF EXISTS concat(anyarray, text);
DROP FUNCTION IF EXISTS concat(text, anyarray);
DROP FUNCTION IF EXISTS concat(anyarray, anyarray);

DROP FUNCTION IF EXISTS nanvl(float4, float4);
DROP FUNCTION IF EXISTS nanvl(float8, float8);
DROP FUNCTION IF EXISTS nanvl(numeric, numeric);

DROP FUNCTION IF EXISTS bitand(bigint, bigint);

DROP AGGREGATE IF EXISTS listagg(text);
DROP AGGREGATE IF EXISTS listagg(text, text);
DROP FUNCTION IF EXISTS listagg1_transfn(text, text);
DROP FUNCTION IF EXISTS listagg2_transfn(text, text, text);

DROP FUNCTION IF EXISTS nvl2(anyelement, anyelement, anyelement);

DROP FUNCTION IF EXISTS lnnvl(bool);

DROP FUNCTION IF EXISTS dump("any");
DROP FUNCTION IF EXISTS dump("any", integer);

DROP FUNCTION IF EXISTS nlssort(text, text);

DROP FUNCTION IF EXISTS substr(text, int, int);
DROP FUNCTION IF EXISTS substr(text, int);

COMMIT;
