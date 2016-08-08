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
