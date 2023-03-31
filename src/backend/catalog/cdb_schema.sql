-- --------------------------------------------------------------------
--
-- cdb_schema.sql
--
-- Define mpp administrative schema and several SQL functions to aid 
-- in maintaining the mpp administrative schema.  
--
-- XXX: over the years most of the view/functions have been removed from 
-- this file. Right now it does not really serve its original purpose,
-- but just contains the functions that need plpgsql support (and their
-- related ones). At some point we can consider either rename this
-- file (to something like 'system_plpgsql_func.sql'), or simply rewrite
-- the plpgsql function in C.
--
-- --------------------------------------------------------------------

-- pg_tablespace_location wrapper functions to see Greenplum cluster-wide tablespace locations
CREATE FUNCTION gp_tablespace_segment_location (IN tblspc_oid oid, OUT gp_segment_id int, OUT tblspc_loc text)
RETURNS SETOF RECORD AS
$$
DECLARE
  seg_id int;
BEGIN
  EXECUTE 'select pg_catalog.gp_execution_segment()' INTO seg_id;
  -- check if execute in entrydb QE to prevent giving wrong results
  IF seg_id = -1 THEN
    RAISE EXCEPTION 'Cannot execute in entrydb, this query is not currently supported by GPDB.';
  END IF;
  RETURN QUERY SELECT pg_catalog.gp_execution_segment() as gp_segment_id, *
    FROM pg_catalog.pg_tablespace_location($1);
END;
$$ LANGUAGE plpgsql EXECUTE ON ALL SEGMENTS;


CREATE FUNCTION gp_tablespace_location (IN tblspc_oid oid, OUT gp_segment_id int, OUT tblspc_loc text)
RETURNS SETOF RECORD
AS
  'SELECT * FROM pg_catalog.gp_tablespace_segment_location($1)
   UNION ALL
   SELECT pg_catalog.gp_execution_segment() as gp_segment_id, * FROM pg_catalog.pg_tablespace_location($1)'
LANGUAGE SQL EXECUTE ON COORDINATOR;
