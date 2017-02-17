-- @author balasr3
-- @description TPC-H query10
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                c_custkey,
                c_name,
                sum(l_extendedprice * (1 - l_discount)) as revenue,
                c_acctbal,
                n_name,
                c_address,
                c_phone,
                c_comment
        from
                customer,
                orders,
                lineitem,
                nation
        where
                c_custkey = o_custkey
                and l_orderkey = o_orderkey
                and o_orderdate >= date '1993-07-01'
                and o_orderdate < date '1993-07-01' + interval '3 month'
                and l_returnflag = 'R'
                and c_nationkey = n_nationkey
        group by
                c_custkey,
                c_name,
                c_acctbal,
                c_phone,
                n_name,
                c_address,
                c_comment
        order by
                revenue desc
        LIMIT 20;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
