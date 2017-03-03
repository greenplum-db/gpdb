-- @author balasr3
-- @description TPC-H query05
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select  n_name,
       		sum(l_extendedprice * (1 - l_discount)) as revenue
from
                customer,
                orders,
                lineitem,
                supplier,
                nation,
                region
where
                c_custkey = o_custkey
                and l_orderkey = o_orderkey
                and l_suppkey = s_suppkey
                and c_nationkey = s_nationkey
                and s_nationkey = n_nationkey
                and n_regionkey = r_regionkey
                and r_name = 'ASIA'
                and o_orderdate >= date '1994-01-01'
                and o_orderdate < date '1994-01-01' + interval '1 year'
group by 
	        n_name
order by
	        revenue desc;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
