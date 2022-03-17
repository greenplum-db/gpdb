-- Test resgroup oid larger than int32.
select gp_inject_fault('bump_oid', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = -1;

create resource group rg_large_oid with (cpu_rate_limit=20, memory_limit=10);

select gp_inject_fault('bump_oid', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

select max(oid)::bigint > (power(2,31) + 1)::bigint from pg_resgroup;

-- count(*) > 0 to run the SQL but do not display the result
select count(*) > 0 from pg_resgroup_get_status(NULL);

drop resource group rg_large_oid;
