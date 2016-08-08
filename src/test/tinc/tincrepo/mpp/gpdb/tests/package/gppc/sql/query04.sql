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
