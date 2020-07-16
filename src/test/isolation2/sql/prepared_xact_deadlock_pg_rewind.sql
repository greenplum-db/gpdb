-- Test a recovered (in startup) prepared transaction does not block
-- pg_rewind due to lock conflict of database template1 when it runs the single
-- mode instance to ensure clean shutdown on the target postgres instance.
include: helpers/server_helpers.sql;

-- set GUCs to speed-up the test
1: alter system set gp_fts_probe_retries to 2;
1: alter system set gp_fts_probe_timeout to 5;
1: select pg_reload_conf();

1: select gp_inject_fault('after_xlog_xact_prepare_flushed', 'suspend', dbid) from gp_segment_configuration where role='p' and content = 0;
2&: create database db_orphan_prepare;
1: select gp_wait_until_triggered_fault('after_xlog_xact_prepare_flushed', 1, dbid) from gp_segment_configuration where role='p' and content = 0;

-- immediate shutdown the primary and then promote the mirror.
1: select pg_ctl((select datadir from gp_segment_configuration c where c.role='p' and c.content=0), 'stop');
1: select gp_request_fts_probe_scan();
1: select content, preferred_role, role, status, mode from gp_segment_configuration where content = 0;

-- wait until promote is finished.
0U: select 1;
0Uq:
2<:

-- restore the cluster. Previously there is a bug the incremental recovery
-- hangs in pg_rewind due to lock conflict. pg_rewinds runs a single-mode
-- postgres to ensure clean shutdown of the postgres. That will recover the
-- unhandled prepared transactions into memory which will hold locks. For
-- example, "create database" will hold the lock of template1 on pg_database
-- with mode 5, but that conflicts with the mode 3 lock which is needed during
-- postgres starting in InitPostgres() and thus pg_rewind hangs forever.
!\retcode gprecoverseg -a;
!\retcode gprecoverseg -ar;

-- reset fts GUCs.
3: alter system reset gp_fts_probe_retries;
3: alter system reset gp_fts_probe_timeout;
3: select pg_reload_conf();
