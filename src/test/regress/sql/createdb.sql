-- start_ignore
create language plpythonu;
-- end_ignore

CREATE OR REPLACE FUNCTION db_dirs(dboid oid) RETURNS setof text
  STRICT STABLE LANGUAGE plpythonu
as $$
import os
p = os.popen("find $MASTER_DATA_DIRECTORY/../.. -name %d -type d" % dboid)
return p.readlines()
$$;


-- inject fault on content0 primary to error out after copying
-- template db directory
select gp_inject_fault2('create_db_after_file_copy', 'error', dbid, hostname, port)
from gp_segment_configuration where content=0 and role='p';

-- should fail
create database db_with_leftover_files;

-- Wait until replay_location = flush_location.
do $$
declare
  behind_segs int;
  i int;
begin
  for i in 1..120 loop
    select count(1) into behind_segs from gp_stat_replication where
      sent_location != replay_location;
    if behind_segs = 0 then
      return;
    end if;
    perform pg_sleep(0.5);
  end loop;
  raise exception 'mirrors took longer than expected to catch up';
end;
$$;

set gp_select_invisible=on;
select count(*)=0 as result from
  (select db_dirs(oid) from pg_database where datname = 'db_with_leftover_files') as foo;

-- start_ignore
select db_dirs(oid) from pg_database where datname = 'db_with_leftover_files';
-- end_ignore

-- cleanup
set gp_select_invisible=off;
select gp_inject_fault2('create_db_after_file_copy', 'reset', dbid, hostname, port)
from gp_segment_configuration where content=0 and role='p';
