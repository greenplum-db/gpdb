-- start_matchsubs
-- m/ERROR:  permission denied: "pg_ao(csseg|visimap|blkdir)_\d+" is a system catalog/
-- s/pg_ao(csseg|visimap|blkdir)_\d+/pg_aocs_aux_table_xxxxx/
-- end_matchsubs

-- create functions to query uaocs auxiliary tables through gp_dist_random instead of going through utility mode
CREATE OR REPLACE FUNCTION gp_aocsseg_dist_random(
  IN relation_name text) RETURNS setof record AS $$
DECLARE
  record_text text;
  result record;
BEGIN
  for record_text in
      EXECUTE 'select gp_toolkit.__gp_aocsseg(''' || relation_name || '''::regclass)::text from gp_dist_random(''gp_id'');'
  loop
      EXECUTE 'select a[3], a[4], a[5], a[6], a[7], a[8] from
              (select regexp_split_to_array(''' || record_text ||''', '','')) as dt(a);' into result;
      return next result;
  end loop;
  return;
END;
$$ LANGUAGE plpgsql;

-- Verify empty visimap for uaocs table
create table uaocs_table_check_empty_visimap (i int, j varchar(20), k int ) with (appendonly=true, orientation=column) DISTRIBUTED BY (i);
insert into uaocs_table_check_empty_visimap values(1,'test',2);
select 1 from pg_appendonly where visimapidxid is not null and visimapidxid is not NULL and relid='uaocs_table_check_empty_visimap'::regclass;

-- Verify the hidden tup_count using UDF gp_aovisimap_hidden_info(oid) for uaocs relation after delete and vacuum
create table uaocs_table_check_hidden_tup_count_after_delete(i int, j varchar(20), k int ) with (appendonly=true, orientation=column) DISTRIBUTED BY (i);
insert into uaocs_table_check_hidden_tup_count_after_delete select i,'aa'||i,i+10 from generate_series(1,10) as i;
select gp_toolkit.__gp_aovisimap_hidden_info('uaocs_table_check_hidden_tup_count_after_delete'::regclass) from gp_dist_random('gp_id');
delete from uaocs_table_check_hidden_tup_count_after_delete where i = 1;
select gp_toolkit.__gp_aovisimap_hidden_info('uaocs_table_check_hidden_tup_count_after_delete'::regclass) from gp_dist_random('gp_id');
vacuum full uaocs_table_check_hidden_tup_count_after_delete;
select gp_toolkit.__gp_aovisimap_hidden_info('uaocs_table_check_hidden_tup_count_after_delete'::regclass) from gp_dist_random('gp_id');

-- Verify the hidden tup_count using UDF gp_aovisimap_hidden_info(oid) for uaocs relation after update and vacuum
create table uaocs_table_check_hidden_tup_count_after_update(i int, j varchar(20), k int ) with (appendonly=true, orientation=column) DISTRIBUTED BY (i);
insert into uaocs_table_check_hidden_tup_count_after_update select i,'aa'||i,i+10 from generate_series(1,10) as i;
select gp_toolkit.__gp_aovisimap_hidden_info('uaocs_table_check_hidden_tup_count_after_update'::regclass) from gp_dist_random('gp_id');
update uaocs_table_check_hidden_tup_count_after_update set j = 'test_update';
select gp_toolkit.__gp_aovisimap_hidden_info('uaocs_table_check_hidden_tup_count_after_update'::regclass) from gp_dist_random('gp_id');
vacuum full uaocs_table_check_hidden_tup_count_after_update;
select gp_toolkit.__gp_aovisimap_hidden_info('uaocs_table_check_hidden_tup_count_after_update'::regclass) from gp_dist_random('gp_id');

-- Verify that we can delete from the AOCO auxiliary tables when allow_system_table_mods is enabled
CREATE OR REPLACE FUNCTION insert_dummy_aoco_aux_entry(fqname text)
RETURNS void AS
$$
BEGIN
    EXECUTE 'INSERT INTO ' || fqname || ' VALUES(null)';
END
$$ LANGUAGE plpgsql;
CREATE OR REPLACE FUNCTION delete_dummy_aoco_aux_entry(fqname text)
RETURNS void AS
$$
BEGIN
    EXECUTE 'DELETE FROM ' || fqname;
END
$$ LANGUAGE plpgsql;
CREATE OR REPLACE FUNCTION select_dummy_aoco_aux_entry(fqname text)
RETURNS int AS
$$
DECLARE result int;
BEGIN
    EXECUTE 'SELECT count(*) FROM ' || fqname INTO result;
    RETURN result;
END
$$ LANGUAGE plpgsql;

CREATE TABLE uaocs_catalog_delete_from (a int) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (a);
CREATE INDEX uaocs_catalog_delete_from_idx ON uaocs_catalog_delete_from USING btree(a);

SELECT delete_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT segrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT visimaprelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;

SET allow_system_table_mods = on;
SELECT insert_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT segrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;
SELECT insert_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;
SELECT insert_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT visimaprelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;

SELECT delete_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT segrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT visimaprelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;

SELECT count(*) FROM gp_toolkit.__gp_aocsseg('uaocs_catalog_delete_from');
SELECT count(*) FROM gp_toolkit.__gp_aovisimap('uaocs_catalog_delete_from');
SELECT select_dummy_aoco_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uaocs_catalog_delete_from'::regclass::oid
) s;

SET allow_system_table_mods = off;
DROP FUNCTION insert_dummy_aoco_aux_entry(fqname text);
DROP FUNCTION delete_dummy_aoco_aux_entry(fqname text);
DROP FUNCTION select_dummy_aoco_aux_entry(fqname text);
DROP TABLE uaocs_catalog_delete_from;
