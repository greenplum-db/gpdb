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

-- no down segment in the beginning
select count(*) from gp_segment_configuration where status = 'd';

-- probe to make sure when we call gp_request_fts_probe_scan() next
-- time below, don't overlap with auto-trigger of FTS scans by FTS
-- process. As if that happens, due to race condition will not trigger
-- the fault and fail the test.
select gp_request_fts_probe_scan();
-- trigger a DNS error
select gp_inject_fault_infinite('get_dns_cached_address', 'skip', 1);
select gp_request_fts_probe_scan();
select gp_inject_fault_infinite('get_dns_cached_address', 'reset', 1);

-- verify a fts failover happens
select count(*) from gp_segment_configuration where status = 'd';

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF --no-progress;

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


