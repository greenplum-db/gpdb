\c mpph_heap

-- start_ignore                                                                                                                                              
REVOKE ALL PRIVILEGES ON PUBLIC.orders FROM memquota_role1;

DROP USER IF EXISTS memquota_role1;

DROP RESOURCE QUEUE memquota_resqueue1;

-- end_ignore

create user memquota_role1 with nosuperuser nocreatedb nocreaterole;

GRANT ALL PRIVILEGES ON PUBLIC.orders TO memquota_role1;

create resource queue memquota_resqueue1 active threshold 4;

alter resource queue memquota_resqueue1 with (memory_limit = '500MB');

alter role memquota_role1 with resource queue memquota_resqueue1;

-- concurrency = statement limit of the resource queue
\! python bugbuster/mem_quota_util.py --complexity=1 --concurrency=4 --dbname=mpph_heap --username=memquota_role1

-- trigger query waiting
\! python bugbuster/mem_quota_util.py --complexity=1 --concurrency=15 --dbname=mpph_heap --username=memquota_role1

\c mpph_heap

-- use 'auto' memory allocation policy
set gp_resqueue_memory_policy=auto;

(select count(*) from (select o0.o_orderkey from (orders o0 left outer join orders o1 on o0.o_orderkey = o1.o_orderkey left outer join orders o2 on o2.o_orderkey = o1.o_orderkey left outer join orders o3 on o3.o_orderkey = o2.o_orderkey left outer join orders o4 on o4.o_orderkey = o3.o_orderkey) order by o0.o_orderkey) as foo);

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
   lineitem
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
   lineitem,part
   where
   l_shipdate <= date '1997-12-01' - interval '106 day' and p_partkey = l_partkey
   group by
   l_returnflag, l_linestatus,l_partkey
 ) FOO1,part
where FOO1.l_partkey = part.p_partkey
group by FOO1.l_partkey,FOO1.l_returnflag,FOO1.l_linestatus

) FOO2,part
where FOO2.l_partkey = part.p_partkey
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
   lineitem,part
   where
   l_shipdate <= date '1996-12-01' - interval '106 day' and p_partkey = l_partkey
   group by
   l_returnflag, l_linestatus,l_partkey
 ) FOO1,part
where FOO1.l_partkey = part.p_partkey
group by FOO1.l_partkey,FOO1.l_returnflag,FOO1.l_linestatus

) FOO2,part
where FOO2.l_partkey = part.p_partkey
group by FOO2.l_partkey,FOO2.l_returnflag,FOO2.l_linestatus
order by FOO2.l_partkey, sum_qty
) FOO3;

-- reset connection to reset gucs
\c mpph_heap


--query with a shallow group tree. The number of groups and memory intensive operators is same as query1                                                     
select count(*) FROM
(
(
select l_returnflag,l_linestatus,l_partkey,
        sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
from
   lineitem
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
   lineitem
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
   lineitem
where
   l_shipdate <= date '1999-12-01' - interval '106 day'
group by
   l_returnflag, l_linestatus,l_partkey
order by l_partkey, sum_qty
)
) FOO3;



--query with blocking operators that would require rescan.bitmapindex scan on l_shipdate will require rescan and hence it should not be considered blocking  

-- start_ignore
drop index if exists lineitem_shipdate;
-- end_ignore

create index lineitem_shipdate on lineitem(l_shipdate);

with lineitem_stats as
(
select l_returnflag,l_linestatus,l_partkey,l_shipdate,
          sum(l_quantity) as sum_qty,sum(l_extendedprice) as sum_base_price,count(*) as count_order
from
   lineitem
where
    l_shipdate <= date '1998-12-01' - interval '106 day'
group by
    l_returnflag, l_linestatus,l_partkey,l_shipdate
order by l_partkey)

select count(*) FROM
(
select l_partkey,sum_qty,sum_base_price,count_order
from
   lineitem_stats,part
where
    l_partkey = p_partkey and l_shipdate <=  date '1998-01-01'

UNION

select l_partkey,sum_qty,sum_base_price,count_order
from
   lineitem_stats,part
where
    l_partkey = p_partkey and l_shipdate <= date '1997-01-01'
order by l_partkey, sum_qty
) FOO1;

-- start_ignore
REVOKE ALL PRIVILEGES ON PUBLIC.orders FROM memquota_role1;
DROP USER IF EXISTS memquota_role1;
DROP RESOURCE QUEUE memquota_resqueue1;
-- end_ignore
