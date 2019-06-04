-- Scenario for validating mirror PANIC immediately on truncate record replay
-- if file is missing.
-- start_ignore
create language plpythonu;
-- end_ignore

-- helper functions
CREATE FUNCTION rel_filepaths_on_segments(regclass) RETURNS TABLE(gp_segment_id int, filepath text)
STRICT STABLE LANGUAGE SQL AS
  $fn$
  SELECT gp_execution_segment(), pg_relation_filepath($1) AS filepath
  $fn$
EXECUTE ON ALL SEGMENTS;

CREATE or REPLACE FUNCTION change_file_permission_readonly(path text)
  RETURNS void as
$$
  import os, stat

  if not os.path.isfile(path):
    plpy.info('skipping non-existent file %s' % (path))
  else:
    plpy.info('changing file permission to readonly')
  os.chmod(path, stat.S_IRUSR);
$$ LANGUAGE plpythonu;

CREATE or REPLACE FUNCTION wait_for_replication_replay (retries int) RETURNS bool AS
$$
declare
  i int;
  result bool;
begin
  i := 0;
  -- Wait until the mirror (content 0) has replayed up to flush location
  loop
    SELECT flush_location = replay_location INTO result from gp_stat_replication where gp_segment_id = 0;
    if result then
      return true;
    end if;

    if i >= retries then
      return false;
    end if;
    perform pg_sleep(0.1);
    i := i + 1;
  end loop;
end;
$$ language plpgsql;

CREATE TABLE mirror_reply_test_table (a INT, b INT, c CHAR(20)) WITH (appendonly=true);
INSERT INTO mirror_reply_test_table SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
-- need to wait till mirror creates the file on disk
SELECT wait_for_replication_replay(5000);
-- step to inject fault to make sure truncate xlog replay cannot open the file in write mode
SELECT change_file_permission_readonly(datadir || '/' ||
  (SELECT filepath FROM rel_filepaths_on_segments('mirror_reply_test_table')
  WHERE gp_segment_id = 0) || '.1')
  FROM gp_segment_configuration WHERE content = 0 AND role = 'm';
-- generate truncate xlog record with non-zero EOF
VACUUM mirror_reply_test_table;

-- expecting mirror to be marked down
do $$
begin
  for i in 1..120 loop
    if (select status = 'd' from gp_segment_configuration where content=0 and role = 'm') then
      return;
    end if;
    perform gp_request_fts_probe_scan();
  end loop;
end;
$$;
SELECT role, preferred_role, mode, status FROM gp_segment_configuration WHERE content=0;

-- post test cleanup
-- start_ignore
\! gprecoverseg -aF --no-progress;
-- end_ignore

-- loop while segments come in sync
do $$
begin
  for i in 1..120 loop
    if (select mode = 's' from gp_segment_configuration where content=0 and role = 'p') then
      return;
    end if;
    perform gp_request_fts_probe_scan();
  end loop;
end;
$$;
SELECT role, preferred_role, mode, status FROM gp_segment_configuration WHERE content=0;
