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
