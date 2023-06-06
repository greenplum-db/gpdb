!\retcode rm -rf /tmp/basebackup_progress_tablespace;
!\retcode mkdir -p /tmp/basebackup_progress_tablespace;
CREATE TABLESPACE basebackuptest_space LOCATION '/tmp/basebackup_progress_tablespace';

-- Inject fault after checkpoint creation in basebackup
SELECT gp_inject_fault('basebackup_progress_tablespace_streamed', 'suspend', dbid)
FROM gp_segment_configuration WHERE content >= -1 and role='p';

-- Run pg_basebackup which should trigger and suspend at the fault
1&: SELECT pg_basebackup(hostname, 100+content, port, false, NULL,
                         '/tmp/baseback_progress_test' || content, true, 'fetch', true)
    from gp_segment_configuration where content = -1 and role = 'p';
2&: SELECT pg_basebackup(hostname, 100+content, port, false, NULL,
                         '/tmp/baseback_progress_test' || content, true, 'fetch', true)
    from gp_segment_configuration where content = 0 and role = 'p';
3&: SELECT pg_basebackup(hostname, 100+content, port, false, NULL,
                         '/tmp/baseback_progress_test' || content, true, 'fetch', true)
    from gp_segment_configuration where content = 1 and role = 'p';
4&: SELECT pg_basebackup(hostname, 100+content, port, false, NULL,
                         '/tmp/baseback_progress_test' || content, true, 'fetch', true)
    from gp_segment_configuration where content = 2 and role = 'p';

-- Wait until fault has been triggered
SELECT gp_wait_until_triggered_fault('basebackup_progress_tablespace_streamed', 1, dbid)
FROM gp_segment_configuration WHERE content >= -1 and role='p';

-- See that pg_basebackup is still running
SELECT application_name, state FROM pg_stat_replication;
SELECT gp_segment_id, pid is not null as has_pid, phase, (backup_total > backup_streamed and tablespaces_total >= tablespaces_streamed) as is_streaming_tablespaces, tablespaces_streamed = 1 FROM gp_stat_progress_basebackup ORDER BY gp_segment_id ASC;
SELECT s.pid is not null as has_pid,
       s.phase,
       (s.backup_total = (select sum(backup_total) from gp_stat_progress_basebackup)) as sum_backup_total,
       (s.backup_streamed = (select sum(backup_streamed) from gp_stat_progress_basebackup)) as sum_backup_streamed,
       (s.tablespaces_total = (select avg(tablespaces_total) from gp_stat_progress_basebackup)) as avg_tablespace_total,
       (s.tablespaces_streamed = (select avg(tablespaces_streamed) from gp_stat_progress_basebackup)) as avg_tablespace_streamed
        FROM gp_stat_progress_basebackup_summary s;

-- Resume basebackup
SELECT gp_inject_fault('basebackup_progress_end', 'suspend', dbid)
FROM gp_segment_configuration WHERE content >= -1 and role='p';
SELECT gp_inject_fault('basebackup_progress_tablespace_streamed', 'reset', dbid)
FROM gp_segment_configuration WHERE content >= -1 and role='p';

-- Wait until fault has been triggered
SELECT gp_wait_until_triggered_fault('basebackup_progress_end', 1, dbid)
FROM gp_segment_configuration WHERE content >= -1 and role='p';

-- See that pg_basebackup is still running
SELECT application_name, state FROM pg_stat_replication;
SELECT gp_segment_id, pid is not null as has_pid, phase, (backup_total = backup_streamed and tablespaces_total = tablespaces_streamed) as backup_all FROM gp_stat_progress_basebackup ORDER BY gp_segment_id ASC;
SELECT s.pid is not null as has_pid,
       s.phase,
       (s.backup_total = (select sum(backup_total) from gp_stat_progress_basebackup)) as sum_backup_total,
       (s.backup_streamed = (select sum(backup_streamed) from gp_stat_progress_basebackup)) as sum_backup_streamed,
       (s.tablespaces_total = (select avg(tablespaces_total) from gp_stat_progress_basebackup)) as avg_tablespace_total,
       (s.tablespaces_streamed = (select avg(tablespaces_streamed) from gp_stat_progress_basebackup)) as avg_tablespace_streamed
FROM gp_stat_progress_basebackup_summary s;

-- Resume basebackup
SELECT gp_inject_fault('basebackup_progress_end', 'reset', dbid)
FROM gp_segment_configuration WHERE content >= -1 and role='p';

-- Wait until basebackup finishes
--start_ignore
1<:
2<:
3<:
4<:
--end_ignore

-- The summary view should be empty after basebackup finishes
select * from gp_stat_progress_basebackup_summary;

drop tablespace basebackuptest_space;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

--start_ignore
-- cleanup
!\retcode rm -rf /tmp/baseback_progress_test-1;
!\retcode rm -rf /tmp/baseback_progress_test0;
!\retcode rm -rf /tmp/baseback_progress_test1;
!\retcode rm -rf /tmp/baseback_progress_test2;
--end_ignore
