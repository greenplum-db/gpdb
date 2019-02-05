set optimizer_enable_master_only_queries = on;
select count(*)/1000 from 
(select
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='priority') as "Priority",
(select count(*) from pg_resqueue x,pg_roles y
where x.oid=y.rolresqueue and a.rsqname=x.rsqname) as "RQAssignedUsers"
from ( select distinct rsqname from pg_resqueue_attributes ) a)
as foo;

select count(*)/1000 from
(select a.rsqname as "RQname",
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='active_statements') as "ActiveStatment",
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='max_cost') as "MaxCost",
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='min_cost') as "MinCost",
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='cost_overcommit') as "CostOvercommit",
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='memory_limit') as "MemoryLimit",
(select ressetting from pg_resqueue_attributes b
where a.rsqname=b.rsqname and resname='priority') as "Priority",
(select count(*) from pg_resqueue x,pg_roles y
where x.oid=y.rolresqueue and a.rsqname=x.rsqname) as "RQAssignedUsers"
from ( select distinct rsqname from pg_resqueue_attributes ) a)
as foo;