CREATE SCHEMA temp_relation;
SET search_path TO temp_relation;

-- Table relfilenodenames is used to get refilenodename after temp table has been droped.
CREATE TABLE relfilenodenames(segid int, relfilenodename text) distributed replicated;
create function get_temp_rel_relfilename(rel text) returns text as $$
relfilenodename = plpy.execute("SELECT \'t_\' || relfilenode AS relfilenodename FROM pg_class WHERE relname=\'" + rel + "\';")[0]['relfilenodename']
return relfilenodename
$$ language plpython3u;

-- Functions to assert physical existence of a temp relfilenode
CREATE FUNCTION temp_relfilenode_coordinator() RETURNS TABLE(gp_segment_id int)
STRICT STABLE LANGUAGE SQL AS
  $fn$
  WITH
  db_relfilenodenames AS
    (SELECT gp_execution_segment(),pg_ls_dir('./base/' || t.dboid || '/') AS relfilenodename
      FROM (SELECT oid AS dboid FROM pg_database WHERE datname = 'regression') t)
  SELECT d.gp_execution_segment
  FROM relfilenodenames r, db_relfilenodenames d
  WHERE r.relfilenodename = d.relfilenodename and r.segid = d.gp_execution_segment;
  $fn$
EXECUTE ON COORDINATOR;

CREATE FUNCTION temp_relfilenode_segments() RETURNS TABLE(gp_segment_id int)
STRICT STABLE LANGUAGE SQL AS
  $fn$
  WITH
  db_relfilenodenames AS
    (SELECT gp_execution_segment(),pg_ls_dir('./base/' || t.dboid || '/') AS relfilenodename
      FROM (SELECT oid AS dboid FROM pg_database WHERE datname = 'regression') t)
  SELECT d.gp_execution_segment
  FROM relfilenodenames r, db_relfilenodenames d
  WHERE r.relfilenodename = d.relfilenodename and r.segid = d.gp_execution_segment;
  $fn$
EXECUTE ON ALL SEGMENTS;

-- When we create a temporary table
CREATE TEMPORARY TABLE temp_table(i int);

-- Relfilenodename may be different between coordinator and segments.
insert into relfilenodenames
select -1, get_temp_rel_relfilename('temp_table') 
union 
select gp_segment_id,get_temp_rel_relfilename('temp_table') from gp_dist_random('gp_id');

-- The table's relfilenode physically exists on the coordinator and the segment primaries
SELECT temp_relfilenode_coordinator();
SELECT temp_relfilenode_segments();

DROP TABLE temp_table;
-- The table's file is cleaned up on the coordinator and the segment primaries
SELECT temp_relfilenode_coordinator();
SELECT temp_relfilenode_segments();

DROP SCHEMA temp_relation CASCADE;