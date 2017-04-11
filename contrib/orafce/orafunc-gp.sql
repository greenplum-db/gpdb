-- Adjust this setting to control where the objects get created.
\set ORA_SCHEMA oracompat

CREATE SCHEMA :ORA_SCHEMA;

SET search_path = :ORA_SCHEMA;
BEGIN;

-- NVL
CREATE OR REPLACE FUNCTION nvl(anyelement, anyelement)
RETURNS anyelement
AS 'MODULE_PATHNAME','ora_nvl'
LANGUAGE C IMMUTABLE;

-- ADD_MONTHS
CREATE OR REPLACE FUNCTION add_months(day date, value int)
RETURNS date
AS 'MODULE_PATHNAME'
LANGUAGE 'C' IMMUTABLE STRICT;

-- LAST_DAY
CREATE OR REPLACE FUNCTION last_day(value date)
RETURNS date
AS 'MODULE_PATHNAME'
LANGUAGE 'C' IMMUTABLE STRICT;

-- NEXT_DAY
CREATE OR REPLACE FUNCTION next_day(value date, weekday text)
RETURNS date
AS 'MODULE_PATHNAME'
LANGUAGE 'C' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION next_day(value date, weekday integer)
RETURNS date
AS 'MODULE_PATHNAME', 'next_day_by_index'
LANGUAGE 'C' IMMUTABLE STRICT;

-- MONTHS_BETWEEN
CREATE OR REPLACE FUNCTION months_between(date1 date, date2 date)
RETURNS numeric
AS 'MODULE_PATHNAME'
LANGUAGE 'C' IMMUTABLE STRICT;

-- TRUNC
CREATE OR REPLACE FUNCTION trunc(value timestamp with time zone, fmt text)
RETURNS timestamp with time zone
AS 'MODULE_PATHNAME', 'ora_timestamptz_trunc'
LANGUAGE 'C' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION trunc(value timestamp with time zone)
RETURNS timestamp with time zone
AS $$ SELECT trunc($1, 'DDD'); $$
LANGUAGE 'SQL' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION trunc(value date, fmt text)
RETURNS date
AS 'MODULE_PATHNAME', 'ora_date_trunc'
LANGUAGE 'C' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION trunc(value date)
RETURNS date
AS $$ SELECT $1; $$
LANGUAGE 'SQL' IMMUTABLE STRICT;

-- ROUND
CREATE OR REPLACE FUNCTION round(value timestamp with time zone, fmt text)
RETURNS timestamp with time zone
AS 'MODULE_PATHNAME', 'ora_timestamptz_round'
LANGUAGE 'C' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION round(value timestamp with time zone)
RETURNS timestamp with time zone
AS $$ SELECT round($1, 'DDD'); $$
LANGUAGE 'SQL' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION round(value date, fmt text)
RETURNS date
AS 'MODULE_PATHNAME','ora_date_round'
LANGUAGE 'C' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION round(value date)
RETURNS date
AS $$ SELECT $1; $$
LANGUAGE 'SQL' IMMUTABLE STRICT;

-- INSTR
CREATE OR REPLACE FUNCTION instr(str text, patt text, start int, nth int)
RETURNS int
AS 'MODULE_PATHNAME','plvstr_instr4'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION instr(str text, patt text, start int)
RETURNS int
AS 'MODULE_PATHNAME','plvstr_instr3'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION instr(str text, patt text)
RETURNS int
AS 'MODULE_PATHNAME','plvstr_instr2'
LANGUAGE C IMMUTABLE STRICT;

-- REVERSE
CREATE OR REPLACE FUNCTION reverse(str text, start int, _end int)
RETURNS text
AS 'MODULE_PATHNAME','plvstr_rvrs'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION reverse(str text, start int)
RETURNS text
AS $$ SELECT reverse($1,$2,NULL);$$
LANGUAGE SQL IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION reverse(str text)
RETURNS text
AS $$ SELECT reverse($1,1,NULL);$$
LANGUAGE SQL IMMUTABLE STRICT;

-- CONCAT
CREATE OR REPLACE FUNCTION concat(text, text)
RETURNS text
AS 'MODULE_PATHNAME','ora_concat'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION concat(text, anyarray)
RETURNS text
AS 'SELECT concat($1, $2::text)'
LANGUAGE sql IMMUTABLE;

CREATE OR REPLACE FUNCTION concat(anyarray, text)
RETURNS text
AS 'SELECT concat($1::text, $2)'
LANGUAGE sql IMMUTABLE;

CREATE OR REPLACE FUNCTION concat(anyarray, anyarray)
RETURNS text
AS 'SELECT concat($1::text, $2::text)'
LANGUAGE sql IMMUTABLE;

-- NANVL
CREATE OR REPLACE FUNCTION nanvl(float4, float4)
RETURNS float4 AS
$$ SELECT CASE WHEN $1 = 'NaN' THEN $2 ELSE $1 END; $$
LANGUAGE sql IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION nanvl(float8, float8)
RETURNS float8 AS
$$ SELECT CASE WHEN $1 = 'NaN' THEN $2 ELSE $1 END; $$
LANGUAGE sql IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION nanvl(numeric, numeric)
RETURNS numeric AS
$$ SELECT CASE WHEN $1 = 'NaN' THEN $2 ELSE $1 END; $$
LANGUAGE sql IMMUTABLE STRICT;

-- BITAND
CREATE OR REPLACE FUNCTION bitand(bigint, bigint)
RETURNS bigint
AS $$ SELECT $1 & $2; $$
LANGUAGE sql IMMUTABLE STRICT;

-- LISTAGG
CREATE OR REPLACE FUNCTION listagg1_transfn(text, text)
RETURNS text
AS 'MODULE_PATHNAME','orafce_listagg1_transfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION listagg2_transfn(text, text, text)
RETURNS text
AS 'MODULE_PATHNAME','orafce_listagg2_transfn'
LANGUAGE C IMMUTABLE;

DROP AGGREGATE IF EXISTS :ORA_SCHEMA.listagg(text);
CREATE  AGGREGATE :ORA_SCHEMA.listagg(text) (
  SFUNC=:ORA_SCHEMA.listagg1_transfn,
  STYPE=text
);

DROP AGGREGATE IF EXISTS :ORA_SCHEMA.listagg(text, text);
CREATE AGGREGATE :ORA_SCHEMA.listagg(text, text) (
  SFUNC=:ORA_SCHEMA.listagg2_transfn,
  STYPE=text
);

-- NVL2
CREATE OR REPLACE FUNCTION nvl2(anyelement, anyelement, anyelement)
RETURNS anyelement
AS 'MODULE_PATHNAME','ora_nvl2'
LANGUAGE C IMMUTABLE;

-- LNNVL
CREATE OR REPLACE FUNCTION lnnvl(bool)
RETURNS bool
AS 'MODULE_PATHNAME','ora_lnnvl'
LANGUAGE C IMMUTABLE;

-- DUMP
CREATE OR REPLACE FUNCTION dump("any") 
RETURNS varchar
AS 'MODULE_PATHNAME', 'orafce_dump'
LANGUAGE C;

CREATE OR REPLACE FUNCTION dump("any", integer)
RETURNS varchar
AS 'MODULE_PATHNAME', 'orafce_dump'
LANGUAGE C;

-- NLSSORT
CREATE OR REPLACE FUNCTION nlssort(text, text)
RETURNS bytea
AS 'MODULE_PATHNAME', 'ora_nlssort'
LANGUAGE 'C' IMMUTABLE;

-- SUBSTR
CREATE OR REPLACE FUNCTION substr(str text, start int)
RETURNS text
AS 'MODULE_PATHNAME','oracle_substr2'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION substr(str text, start int, len int)
RETURNS text
AS 'MODULE_PATHNAME','oracle_substr3'
LANGUAGE C IMMUTABLE STRICT;

COMMIT;
