-- --------------------------------------------------------------------
--
-- cdb_schema.sql
--
-- Define mpp administrative schema and several SQL functions to aid 
-- in maintaining the mpp administrative schema.  
--
-- This is version 2 of the schema.
--
-- TODO Error checking is rudimentary and needs improvment.
--
--
-- --------------------------------------------------------------------
SET log_min_messages = WARNING;

-------------------------------------------------------------------
-- database
-------------------------------------------------------------------
CREATE OR REPLACE VIEW pg_catalog.gp_pgdatabase AS 
    SELECT *
      FROM gp_pgdatabase() AS L(dbid smallint, isprimary boolean, content smallint, valid boolean, definedprimary boolean);

GRANT SELECT ON pg_catalog.gp_pgdatabase TO PUBLIC;

------------------------------------------------------------------
-- distributed transaction related
------------------------------------------------------------------
CREATE OR REPLACE VIEW pg_catalog.gp_distributed_xacts AS 
    SELECT *
      FROM gp_distributed_xacts() AS L(distributed_xid xid, state text, gp_session_id int, xmin_distributed_snapshot xid);

GRANT SELECT ON pg_catalog.gp_distributed_xacts TO PUBLIC;


CREATE OR REPLACE VIEW pg_catalog.gp_transaction_log AS 
    SELECT *
      FROM gp_transaction_log() AS L(segment_id smallint, dbid smallint, transaction xid, status text);

GRANT SELECT ON pg_catalog.gp_transaction_log TO PUBLIC;

CREATE OR REPLACE VIEW pg_catalog.gp_distributed_log AS 
    SELECT *
      FROM gp_distributed_log() AS L(segment_id smallint, dbid smallint, distributed_xid xid, status text, local_transaction xid);

GRANT SELECT ON pg_catalog.gp_distributed_log TO PUBLIC;

ALTER RESOURCE QUEUE pg_default WITH (priority=medium, memory_limit='-1');


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

RESET log_min_messages;
