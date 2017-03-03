-- @author ramans2
-- @created 2014-03-17 12:00:00
-- @modified 2014-03-17 12:00:00
-- @description Single query with multiple slices that runs OOM

show gp_vmem_protect_limit;

select 4 as oom_test;

select
        l0.l_returnflag,
        l0.l_linestatus,
        sum(l0.l_quantity) as sum_qty,
        sum(l0.l_extendedprice) as sum_base_price,
        sum(l0.l_extendedprice * (1 - l0.l_discount)) as sum_disc_price,
        sum(l0.l_extendedprice * (1 - l0.l_discount) * (1 + l0.l_tax)) as sum_charge,
        avg(l0.l_quantity) as avg_qty,
        avg(l0.l_extendedprice) as avg_price,
        avg(l0.l_discount) as avg_disc,
        count(*) as count_order
from
        lineitem l0
left outer join lineitem l1 on l0.l_orderkey = l1.l_orderkey
left outer join lineitem l2 on l1.l_orderkey = l2.l_orderkey
left outer join lineitem l3 on l2.l_orderkey = l3.l_orderkey
left outer join lineitem l4 on l3.l_orderkey = l4.l_orderkey
left outer join lineitem l5 on l4.l_orderkey = l5.l_orderkey
where
        l0.l_shipdate <= date '1998-12-01' - interval '108 day'
group by
        l0.l_returnflag,
        l0.l_linestatus
union all
select
        l0.l_returnflag,
        l0.l_linestatus,
        sum(l0.l_quantity) as sum_qty,
        sum(l0.l_extendedprice) as sum_base_price,
        sum(l0.l_extendedprice * (1 - l0.l_discount)) as sum_disc_price,
        sum(l0.l_extendedprice * (1 - l0.l_discount) * (1 + l0.l_tax)) as sum_charge,
        avg(l0.l_quantity) as avg_qty,
        avg(l0.l_extendedprice) as avg_price,
        avg(l0.l_discount) as avg_disc,
        count(*) as count_order
from
        lineitem l0
left outer join lineitem l1 on l0.l_orderkey = l1.l_orderkey
left outer join lineitem l2 on l1.l_orderkey = l2.l_orderkey
left outer join lineitem l3 on l2.l_orderkey = l3.l_orderkey
left outer join lineitem l4 on l3.l_orderkey = l4.l_orderkey
left outer join lineitem l5 on l4.l_orderkey = l5.l_orderkey
where
        l0.l_shipdate <= date '1998-12-01' - interval '108 day'
group by
        l0.l_returnflag,
        l0.l_linestatus
union all
select
        l0.l_returnflag,
        l0.l_linestatus,
        sum(l0.l_quantity) as sum_qty,
        sum(l0.l_extendedprice) as sum_base_price,
        sum(l0.l_extendedprice * (1 - l0.l_discount)) as sum_disc_price,
        sum(l0.l_extendedprice * (1 - l0.l_discount) * (1 + l0.l_tax)) as sum_charge,
        avg(l0.l_quantity) as avg_qty,
        avg(l0.l_extendedprice) as avg_price,
        avg(l0.l_discount) as avg_disc,
        count(*) as count_order
from
        lineitem l0
left outer join lineitem l1 on l0.l_orderkey = l1.l_orderkey
left outer join lineitem l2 on l1.l_orderkey = l2.l_orderkey
left outer join lineitem l3 on l2.l_orderkey = l3.l_orderkey
left outer join lineitem l4 on l3.l_orderkey = l4.l_orderkey
left outer join lineitem l5 on l4.l_orderkey = l5.l_orderkey
where
        l0.l_shipdate <= date '1998-12-01' - interval '108 day'
group by
        l0.l_returnflag,
        l0.l_linestatus
order by
        l_returnflag,
        l_linestatus;
