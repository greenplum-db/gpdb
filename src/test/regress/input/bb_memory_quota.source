
create user memquota_role1 with nosuperuser nocreatedb nocreaterole;

GRANT ALL PRIVILEGES ON PUBLIC.heap_orders TO memquota_role1;

create resource queue memquota_resqueue1 active threshold 4;

alter resource queue memquota_resqueue1 with (memory_limit = '500MB');

alter role memquota_role1 with resource queue memquota_resqueue1;

-- concurrency = statement limit of the resource queue
\! @abs_builddir@/mem_quota_util.py --complexity=1 --concurrency=4 --dbname=regress --username=memquota_role1

-- trigger query waiting
\! @abs_builddir@/mem_quota_util.py --complexity=1 --concurrency=15 --dbname=regress --username=memquota_role1

-- use 'auto' memory allocation policy
set gp_resqueue_memory_policy=auto;

(select count(*) from (select o0.o_orderkey from (heap_orders o0 left outer join heap_orders o1 on o0.o_orderkey = o1.o_orderkey left outer join heap_orders o2 on o2.o_orderkey = o1.o_orderkey left outer join heap_orders o3 on o3.o_orderkey = o2.o_orderkey left outer join heap_orders o4 on o4.o_orderkey = o3.o_orderkey) order by o0.o_orderkey) as foo);

-- eager free queries from cdbfast
set gp_resqueue_memory_policy=eager_free;

--query with deep group tree and each group having just one memory intensive operator
select count(*) from
(
select sum(FOO2.sum_qty) as sum_qty,
sum(FOO2.sum_base_price) as sum_base_price,
count(*) as count_order,
FOO2.l_returnflag,
FOO2.l_linestatus,
FOO2.l_partkey
FROM
(
select sum(FOO1.sum_qty) as sum_qty,
sum(FOO1.sum_base_price) as sum_base_price,
count(*) as count_order,
FOO1.l_returnflag,
FOO1.l_linestatus,
FOO1.l_partkey
FROM
 ( select l_returnflag,l_linestatus,l_partkey,
          sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
   from
   heap_lineitem
   where
   l_shipdate <= date '1998-12-01' - interval '106 day'
   group by
   l_returnflag, l_linestatus,l_partkey
   order by l_partkey
 ) FOO1
group by FOO1.l_partkey,FOO1.l_returnflag,FOO1.l_linestatus
order by FOO1.l_partkey
) FOO2
group by FOO2.l_partkey,FOO2.l_returnflag,FOO2.l_linestatus
order by FOO2.l_partkey, sum_qty
) FOO3;



--query with deep group tree and each group having more memory intensive operators
select count(*) FROM
(
select sum(FOO2.sum_qty) as sum_qty,
sum(FOO2.sum_base_price) as sum_base_price,
count(*) as count_order,
FOO2.l_returnflag,
FOO2.l_linestatus,
FOO2.l_partkey

FROM
(
select sum(FOO1.sum_qty) as sum_qty,
sum(FOO1.sum_base_price) as sum_base_price,
count(*) as count_order,
FOO1.l_returnflag,
FOO1.l_linestatus,
FOO1.l_partkey
FROM
 ( select l_returnflag,l_linestatus,l_partkey,
          sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
   from
   heap_lineitem,heap_part
   where
   l_shipdate <= date '1997-12-01' - interval '106 day' and p_partkey = l_partkey
   group by
   l_returnflag, l_linestatus,l_partkey
 ) FOO1,heap_part
where FOO1.l_partkey = heap_part.p_partkey
group by FOO1.l_partkey,FOO1.l_returnflag,FOO1.l_linestatus

) FOO2,heap_part
where FOO2.l_partkey = heap_part.p_partkey
group by FOO2.l_partkey,FOO2.l_returnflag,FOO2.l_linestatus
order by FOO2.l_partkey, sum_qty
) FOO3;



--same as query1. But this will be executed using merge join to trigger mark restore in eager_free
set enable_hashjoin = off;
set enable_nestloop = off;
set enable_mergejoin = on;

select count(*) FROM
(
select sum(FOO2.sum_qty) as sum_qty,
sum(FOO2.sum_base_price) as sum_base_price,
count(*) as count_order,
FOO2.l_returnflag,
FOO2.l_linestatus,
FOO2.l_partkey

FROM
(
select sum(FOO1.sum_qty) as sum_qty,
sum(FOO1.sum_base_price) as sum_base_price,
count(*) as count_order,
FOO1.l_returnflag,
FOO1.l_linestatus,
FOO1.l_partkey
FROM
 ( select l_returnflag,l_linestatus,l_partkey,
          sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
   from
   heap_lineitem,heap_part
   where
   l_shipdate <= date '1996-12-01' - interval '106 day' and p_partkey = l_partkey
   group by
   l_returnflag, l_linestatus,l_partkey
 ) FOO1,heap_part
where FOO1.l_partkey = heap_part.p_partkey
group by FOO1.l_partkey,FOO1.l_returnflag,FOO1.l_linestatus

) FOO2,heap_part
where FOO2.l_partkey = heap_part.p_partkey
group by FOO2.l_partkey,FOO2.l_returnflag,FOO2.l_linestatus
order by FOO2.l_partkey, sum_qty
) FOO3;

reset gp_resqueue_memory_policy;
reset enable_hashjoin;
reset enable_nestloop;
reset enable_mergejoin;

--query with a shallow group tree. The number of groups and memory intensive operators is same as query1
select count(*) FROM
(
(
select l_returnflag,l_linestatus,l_partkey,
        sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
from
   heap_lineitem
where
    l_shipdate <= date '1998-12-01' - interval '106 day'
group by
    l_returnflag, l_linestatus,l_partkey
order by l_partkey
)

UNION ALL

(
select l_returnflag,l_linestatus,l_partkey,
       sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
from
   heap_lineitem
where
    l_shipdate <= date '1997-12-01' - interval '106 day'
group by
    l_returnflag, l_linestatus,l_partkey
order by l_partkey
)

UNION ALL

(
select l_returnflag,l_linestatus,l_partkey,
       sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
from
   heap_lineitem
where
   l_shipdate <= date '1999-12-01' - interval '106 day'
group by
   l_returnflag, l_linestatus,l_partkey
order by l_partkey, sum_qty
)
) FOO3;

--query with blocking operators that would require rescan.bitmapindex scan on l_shipdate will require rescan and hence it should not be considered blocking

create index heap_lineitem_shipdate on heap_lineitem(l_shipdate);

with heap_lineitem_stats as
(
select l_returnflag,l_linestatus,l_partkey,l_shipdate,
          sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
from
   heap_lineitem
where
    l_shipdate <= date '1998-12-01' - interval '106 day'
group by
    l_returnflag, l_linestatus,l_partkey,l_shipdate
order by l_partkey)

select count(*) FROM
(
select l_partkey,sum_qty,sum_base_price,count_order
from
   heap_lineitem_stats,heap_part
where
    l_partkey = p_partkey and l_shipdate <=  date '1998-01-01'

UNION

select l_partkey,sum_qty,sum_base_price,count_order
from
   heap_lineitem_stats,heap_part
where
    l_partkey = p_partkey and l_shipdate <= date '1997-01-01'
order by l_partkey, sum_qty
) FOO1;

REVOKE ALL PRIVILEGES ON PUBLIC.heap_orders FROM memquota_role1;
DROP USER IF EXISTS memquota_role1;
DROP RESOURCE QUEUE memquota_resqueue1;
