-- Test gprecoverseg from config file uses the correct dbid.
--
-- In github issue 9837 dbid in gp_segment_configuration is not
-- consistent with dbid in file internal.auto.conf.
-- This is caused by gprecoverseg fetch the smallest dbid in
-- gp_segment_configuration which is not occupied by others when
-- adding a new mirror. When dbid in gp_segment_configuration is not
-- continous, the inconsistent issue will happen

include: helpers/server_helpers.sql;

SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
-- stop a primary in order to trigger a mirror promotion
select pg_ctl((select datadir from gp_segment_configuration c
where c.role='p' and c.content=1), 'stop');

-- trigger failover
select gp_request_fts_probe_scan();

-- wait for content 0 (earlier mirror, now primary) to finish the promotion
0U: select 1;
-- Quit this utility mode session, as need to start fresh one below
0Uq:

-- make the dbid in gp_segment_configuration not continuous
set allow_system_table_mods to true;
update gp_segment_configuration set dbid=9 where dbid=2;

-- generate recover config file
select generate_recover_config_file(
	(select datadir from gp_segment_configuration c where c.role='m' and c.content=1),
	(select port from gp_segment_configuration c where c.role='m' and c.content=1)::text);

-- recover from config file
!\retcode gprecoverseg -a -i /tmp/recover_config_file;

-- recover should reuse the old dbid and not occupy dbid=2
select dbid from gp_segment_configuration where dbid=2;

update gp_segment_configuration set dbid=2 where dbid=9;
set allow_system_table_mods to false;

-- fully recover the cluster
!\retcode gprecoverseg -aF;

-- rebalance the cluster
!\retcode gprecoverseg -ar;

