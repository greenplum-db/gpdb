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

--
-- Bitmap Index AM supports all the same operations as the B-tree index, it
-- just stores the entries differently. Make copies of all the built-in B-tree
-- operator classes and families for the bitmap AM, too.
--
do $$
declare
  btree_amoid oid;
  bitmap_amoid oid;
  btree_opfam_oid oid;
  bitmap_opfam_oid oid;
  opfam_row pg_opfamily;
  opclass_row pg_opclass;
  amop_row pg_amop;
  amproc_row pg_amproc;
begin
  -- Fetch OIDs of the B-tree and bitmap AMs.
  btree_amoid = (select oid from pg_am where amname = 'btree');
  bitmap_amoid = (select oid from pg_am where amname = 'bitmap');
  -- Loop through all B-tree opfamilies.
  FOR btree_opfam_oid IN select oid as r from pg_opfamily WHERE opfmethod = btree_amoid LOOP
    -- Copy this B-tree opfamily for the Bitmap AM.
    SELECT * INTO opfam_row FROM pg_opfamily WHERE oid = btree_opfam_oid;
    opfam_row.opfmethod = bitmap_amoid;
    INSERT INTO pg_opfamily SELECT opfam_row.*;
    GET DIAGNOSTICS bitmap_opfam_oid = RESULT_OID;
    -- Also copy all operator classes belonging to this opfamily.
    FOR opclass_row IN select * from pg_opclass where opcfamily = btree_opfam_oid LOOP
      opclass_row.opcfamily = bitmap_opfam_oid;
      opclass_row.opcmethod = bitmap_amoid;
      -- Reverse of the "ugly little hack" to store 'name' columns as
      -- 'cstrings' in b-tree indexes (see pg_opclass.h). The hack will kick
      -- in for the LOV index, anyway, but we mustn't try to use 'cstring' in
      -- the LOV heap.
      IF opclass_row.opcname='name_ops' THEN
        opclass_row.opckeytype = 0;
      END IF;
      INSERT INTO pg_opclass SELECT opclass_row.*;
    END LOOP;
    -- And all pg_amop and pg_amproc rows.
    FOR amop_row IN select * from pg_amop where amopfamily = btree_opfam_oid LOOP
      amop_row.amopfamily = bitmap_opfam_oid;
      amop_row.amopmethod = bitmap_amoid;
      INSERT INTO pg_amop SELECT amop_row.*;
    END LOOP;
    FOR amproc_row IN select * from pg_amproc where amprocfamily = btree_opfam_oid LOOP
      amproc_row.amprocfamily = bitmap_opfam_oid;
      INSERT INTO pg_amproc SELECT amproc_row.*;
    END LOOP;
  END LOOP;
end;
$$;

CREATE SCHEMA gp_toolkit;

RESET log_min_messages;
