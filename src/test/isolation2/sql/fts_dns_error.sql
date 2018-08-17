-- Tests FTS can handle DNS error.
create extension if not exists gp_inject_fault;

-- to make test deterministic and fast
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;

-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery. Approximately we want to wait 30 seconds.
!\retcode gpconfig -c gp_gang_creation_retry_count -v 127 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 250 --skipvalidation --masteronly;
!\retcode gpstop -u;

-- skip FTS probes firstly
select gp_inject_fault_infinite('fts_probe', 'skip', 1);
select gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_probe', 1, 1);

-- no down segment in the beginning
select count(*) from gp_segment_configuration where status = 'd';

-- inject dns error
select gp_inject_fault_infinite('get_dns_cached_address', 'skip', 1);
-- specify database name, so gdd backend will not be affected
SELECT gp_inject_fault('before_fts_notify', 'suspend', '', 'isolation2test', '', 1, -1, 1, 1);


-- trigger a DNS error in segment 0 in session 0,
-- Before error out,QD will notify the FTS to do
-- a probe, fts probe is currently skipped, so this
-- query will get stuck 
0&: create table fts_dns_error_test (c1 int, c2 int);

select gp_wait_until_triggered_fault('before_fts_notify', 1, 1);

-- enable fts probe
select gp_inject_fault_infinite('fts_probe', 'reset', 1);

-- resume fts notify for session 0 to notify fts to probe
select gp_inject_fault_infinite('before_fts_notify', 'resume', 1);
select gp_inject_fault_infinite('before_fts_notify', 'reset', 1);

-- wait until fts done the probe and this query will fail 
0<:

-- verify a fts failover happens
select count(*) from gp_segment_configuration where status = 'd';

-- fts failover happens, so the following query will success.
0: create table fts_dns_error_test (c1 int, c2 int);

-- reset dns fault so segment can be recovered
select gp_inject_fault_infinite('get_dns_cached_address', 'reset', 1);

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select mode = 's' from gp_segment_configuration where content = 0 limit 1) then /* in func */
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select mode = 's' from gp_segment_configuration where content = 0 limit 1) then /* in func */
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

-- verify no segment is down after recovery
select count(*) from gp_segment_configuration where status = 'd';

!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation --masteronly;
!\retcode gpstop -u;


