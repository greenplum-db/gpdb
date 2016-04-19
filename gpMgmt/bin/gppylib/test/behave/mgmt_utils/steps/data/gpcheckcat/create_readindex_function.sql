CREATE OR REPLACE FUNCTION gp_toolkit.readindex(oid,bool) RETURNS SETOF record AS '$libdir/indexscan', 'readindex' LANGUAGE C STRICT;

