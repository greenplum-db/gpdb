------------------------------------------------------------------
-- fixedwidth Formatters
------------------------------------------------------------------

CREATE OR REPLACE FUNCTION fixedwidth_in() RETURNS record
AS '$libdir/fixedwidth.so', 'fixedwidth_in'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION fixedwidth_out(record) RETURNS bytea
AS '$libdir/fixedwidth.so', 'fixedwidth_out'
LANGUAGE C STABLE;
