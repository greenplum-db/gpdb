-- start_matchsubs
-- m/ERROR:  permission denied: "pg_ao(seg|visimap|blkdir)_\d+" is a system catalog/
-- s/pg_ao(seg|visimap|blkdir)_\d+/pg_ao_aux_table_xxxxx/
-- end_matchsubs

-- create functions to query uao auxiliary tables through gp_dist_random instead of going through utility mode
CREATE OR REPLACE FUNCTION gp_aovisimap_dist_random(
  IN relation_name text) RETURNS setof record AS $$
DECLARE
  relid oid;
  result record;
BEGIN
  select into relid oid from pg_class where relname=quote_ident(relation_name);
  for result in
      EXECUTE 'select * from gp_dist_random(''pg_aoseg.pg_aovisimap_' || relid ||''');'
  loop
      return next result;
  end loop;
  return;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION gp_aovisimap_count(
  IN relation_name text) RETURNS int AS $$
DECLARE
  aovisimap_record record;
  result int := 0;
BEGIN
  for aovisimap_record in
      EXECUTE 'select gp_toolkit.__gp_aovisimap(''' || relation_name || '''::regclass) from gp_dist_random(''gp_id'');'
  loop
      result := result + 1;
  end loop;
  return result;
END;
$$ LANGUAGE plpgsql;

-- Verify empty visimap for uao table
create table uao_table_check_empty_visimap (i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_check_empty_visimap values(1,'test',2);
SELECT count(i.indexrelid) FROM pg_appendonly a, pg_index i WHERE a.visimaprelid = i.indrelid AND a.relid='uao_table_check_empty_visimap'::regclass;

-- Verify GUC select_invisible=true for uao tables
create table uao_table_check_select_invisible (i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_check_select_invisible values(1,'test',4);
select * from uao_table_check_select_invisible;
update uao_table_check_select_invisible set j = 'test_update' where i = 1;
select * from uao_table_check_select_invisible;
set gp_select_invisible = true;
select * from uao_table_check_select_invisible;
set gp_select_invisible = false;

-- Verify that the visimap changes when delete from uao table
create table uao_table_check_visimap_changes_after_delete (i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
select * from uao_table_check_visimap_changes_after_delete;
select count(*) from gp_aovisimap_dist_random('uao_table_check_visimap_changes_after_delete') as (segno integer, first_row_no bigint, visimap bytea);
insert into uao_table_check_visimap_changes_after_delete select i,'aa'||i,i+10 from generate_series(1,5) as i;
delete from uao_table_check_visimap_changes_after_delete where i=3;
select * from uao_table_check_visimap_changes_after_delete;
select count(*) from gp_aovisimap_dist_random('uao_table_check_visimap_changes_after_delete') as (segno integer, first_row_no bigint, visimap bytea);

-- Verify that the visimap changes when delete and truncate from uao table
create table uao_table_visimap_changes_after_trunc (i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
select * from uao_table_visimap_changes_after_trunc;
select count(*) from gp_aovisimap_dist_random('uao_table_visimap_changes_after_trunc') as (segno integer, first_row_no bigint, visimap bytea);
insert into uao_table_visimap_changes_after_trunc select i,'aa'||i,i+10 from generate_series(1,5) as i;
delete from uao_table_visimap_changes_after_trunc where i=3;
select * from uao_table_visimap_changes_after_trunc;
select count(*) from gp_aovisimap_dist_random('uao_table_visimap_changes_after_trunc') as (segno integer, first_row_no bigint, visimap bytea);
truncate table uao_table_visimap_changes_after_trunc;
select * from uao_table_visimap_changes_after_trunc;
select count(*) from gp_aovisimap_dist_random('uao_table_visimap_changes_after_trunc') as (segno integer, first_row_no bigint, visimap bytea);

-- Verify the usage of UDF gp_aovisimap for deleted tuple
create table uao_table_gp_aovisimap_after_delete(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_gp_aovisimap_after_delete select i,'aa'||i,i+10 from generate_series(1,10) as i;
select count(*) from gp_dist_random('uao_table_gp_aovisimap_after_delete');
select * from gp_aovisimap_count('uao_table_gp_aovisimap_after_delete');
delete from uao_table_gp_aovisimap_after_delete;
select count(*) from gp_dist_random('uao_table_gp_aovisimap_after_delete');
select * from gp_aovisimap_count('uao_table_gp_aovisimap_after_delete');

-- Verify the usage of UDF gp_aovisimap for update tuple
create table uao_table_gp_aovisimap_after_update(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_gp_aovisimap_after_update select i,'aa'||i,i+10 from generate_series(1,10) as i;
select count(*) from gp_dist_random('uao_table_gp_aovisimap_after_update');
select * from gp_aovisimap_count('uao_table_gp_aovisimap_after_update');
update uao_table_gp_aovisimap_after_update set j = j || '_9';
select count(*) from gp_dist_random('uao_table_gp_aovisimap_after_update');
select * from gp_aovisimap_count('uao_table_gp_aovisimap_after_update');

-- Verify the tupcount changes in pg_aoseg when deleting from uao table
create table uao_table_tupcount_changes_after_delete(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_tupcount_changes_after_delete select i,'aa'||i,i+10 from generate_series(1,10) as i;
analyze uao_table_tupcount_changes_after_delete;
select sum(tupcount) from gp_toolkit.__gp_aoseg('uao_table_tupcount_changes_after_delete');
select count(*) from uao_table_tupcount_changes_after_delete;
delete from uao_table_tupcount_changes_after_delete where i = 1;
select sum(tupcount) from gp_toolkit.__gp_aoseg('uao_table_tupcount_changes_after_delete');
select count(*) from uao_table_tupcount_changes_after_delete;
vacuum full uao_table_tupcount_changes_after_delete;
select sum(tupcount) from gp_toolkit.__gp_aoseg('uao_table_tupcount_changes_after_delete');
select count(*) from uao_table_tupcount_changes_after_delete;

-- Verify the tupcount changes in pg_aoseg when updating uao table
create table uao_table_tupcount_changes_after_update(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_tupcount_changes_after_update select i,'aa'||i,i+10 from generate_series(1,10) as i;
analyze uao_table_tupcount_changes_after_update;
select sum(tupcount) from gp_toolkit.__gp_aoseg('uao_table_tupcount_changes_after_update');
select count(*) from uao_table_tupcount_changes_after_update;
update uao_table_tupcount_changes_after_update set j=j||'test11' where i = 1;
select sum(tupcount) from gp_toolkit.__gp_aoseg('uao_table_tupcount_changes_after_update');
select count(*) from uao_table_tupcount_changes_after_update;
vacuum full uao_table_tupcount_changes_after_update;
select sum(tupcount) from gp_toolkit.__gp_aoseg('uao_table_tupcount_changes_after_update');
select count(*) from uao_table_tupcount_changes_after_update;

-- Verify the hidden tup_count using UDF gp_aovisimap_hidden_info(oid) for uao relation after delete and vacuum
create table uao_table_check_hidden_tup_count_after_delete(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_check_hidden_tup_count_after_delete select i,'aa'||i,i+10 from generate_series(1,10) as i;
select * from gp_toolkit.__gp_aovisimap_hidden_info('uao_table_check_hidden_tup_count_after_delete'::regclass);
delete from uao_table_check_hidden_tup_count_after_delete;
select * from gp_toolkit.__gp_aovisimap_hidden_info('uao_table_check_hidden_tup_count_after_delete'::regclass);
vacuum full uao_table_check_hidden_tup_count_after_delete;
select * from gp_toolkit.__gp_aovisimap_hidden_info('uao_table_check_hidden_tup_count_after_delete'::regclass);

-- Verify the hidden tup_count using UDF gp_aovisimap_hidden_info(oid) for uao relation after update and vacuum
create table uao_table_check_hidden_tup_count_after_update(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_check_hidden_tup_count_after_update select i,'aa'||i,i+10 from generate_series(1,10) as i;
select * from gp_toolkit.__gp_aovisimap_hidden_info('uao_table_check_hidden_tup_count_after_update'::regclass);
update uao_table_check_hidden_tup_count_after_update set j = 'test21';
select * from gp_toolkit.__gp_aovisimap_hidden_info('uao_table_check_hidden_tup_count_after_update'::regclass);
vacuum full uao_table_check_hidden_tup_count_after_update;
select * from gp_toolkit.__gp_aovisimap_hidden_info('uao_table_check_hidden_tup_count_after_update'::regclass);

-- Verify that we can delete from the AO auxiliary tables when allow_system_table_mods is enabled
CREATE OR REPLACE FUNCTION insert_dummy_ao_aux_entry(fqname text)
RETURNS void AS
$$
BEGIN
    EXECUTE 'INSERT INTO ' || fqname || ' VALUES(null)';
END
$$ LANGUAGE plpgsql;
CREATE OR REPLACE FUNCTION delete_dummy_ao_aux_entry(fqname text)
RETURNS void AS
$$
BEGIN
    EXECUTE 'DELETE FROM ' || fqname;
END
$$ LANGUAGE plpgsql;

CREATE TABLE uao_catalog_delete_from (a int) USING ao_row DISTRIBUTED BY (a);
CREATE INDEX uao_catalog_delete_from_idx ON uao_catalog_delete_from USING btree(a);

SELECT delete_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT segrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT visimaprelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;

SET allow_system_table_mods = on;
SELECT insert_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT segrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;
SELECT insert_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;
SELECT insert_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT visimaprelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;

SELECT delete_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT segrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT blkdirrelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;
SELECT delete_dummy_ao_aux_entry(s.interior_fqname)
FROM (
     SELECT visimaprelid::regclass::text AS interior_fqname FROM pg_appendonly WHERE relid = 'uao_catalog_delete_from'::regclass::oid
) s;

SELECT count(*) FROM gp_toolkit.__gp_aoseg('uao_catalog_delete_from');
SELECT count(*) FROM gp_toolkit.__gp_aoblkdir('uao_catalog_delete_from');
SELECT count(*) FROM gp_toolkit.__gp_aovisimap('uao_catalog_delete_from');

SET allow_system_table_mods = off;
DROP FUNCTION insert_dummy_ao_aux_entry(fqname text);
DROP FUNCTION delete_dummy_ao_aux_entry(fqname text);
DROP TABLE uao_catalog_delete_from;
