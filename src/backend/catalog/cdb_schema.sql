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
CREATE OR REPLACE VIEW gp_pgdatabase AS 
    SELECT *
      FROM gp_pgdatabase() AS L(dbid smallint, isprimary boolean, content smallint, valid boolean, definedprimary boolean);

GRANT SELECT ON gp_pgdatabase TO PUBLIC;

------------------------------------------------------------------
-- distributed transaction related
------------------------------------------------------------------
CREATE OR REPLACE VIEW gp_distributed_xacts AS 
    SELECT *
      FROM gp_distributed_xacts() AS L(distributed_xid xid, distributed_id text, state text, gp_session_id int, xmin_distributed_snapshot xid);

GRANT SELECT ON gp_distributed_xacts TO PUBLIC;


CREATE OR REPLACE VIEW gp_transaction_log AS 
    SELECT *
      FROM gp_transaction_log() AS L(segment_id smallint, dbid smallint, transaction xid, status text);

GRANT SELECT ON gp_transaction_log TO PUBLIC;

CREATE OR REPLACE VIEW gp_distributed_log AS 
    SELECT *
      FROM gp_distributed_log() AS L(segment_id smallint, dbid smallint, distributed_xid xid, distributed_id text, status text, local_transaction xid);

GRANT SELECT ON gp_distributed_log TO PUBLIC;

ALTER RESOURCE QUEUE pg_default WITH (priority=medium, memory_limit='-1');

CREATE SCHEMA gp_toolkit;

RESET log_min_messages;
