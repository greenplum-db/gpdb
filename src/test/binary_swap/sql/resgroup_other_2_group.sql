show gp_resource_manager;

select * from pg_resgroup
  where rsgname not like 'rg_dump_test%'
  order by oid;
select avg(reslimittype)
  from pg_resgroupcapability
  where reslimittype <= 1;
select groupname from gp_toolkit.gp_resgroup_config
  where groupname not like 'rg_dump_test%'
  order by groupid;
select rsgname from gp_toolkit.gp_resgroup_status
  where rsgname not like 'rg_dump_test%'
  order by groupid;

alter resource group rg1 set cpu_hard_quota_limit 20;
alter resource group rg1 set cpu_hard_quota_limit 10;
drop resource group rg1;
create resource group rg1 with (cpu_hard_quota_limit=10, memory_limit=10);

alter resource group rg2 set cpu_hard_quota_limit 20;
alter resource group rg2 set cpu_hard_quota_limit 10;

-- TODO: change to the resgroup
