-- @author balasr3
-- @description TPC-H query07
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                supp_nation,
                cust_nation,
                l_year,
                sum(volume) as revenue
        from
                (
                        select
                                n1.n_name as supp_nation,
                                n2.n_name as cust_nation,
                                extract(year from l_shipdate) as l_year,
                                l_extendedprice * (1 - l_discount) as volume
                        from
                                supplier,
                                lineitem,
                                orders,
                                customer,
                                nation n1,
                                nation n2
                        where
                                s_suppkey = l_suppkey
                                and o_orderkey = l_orderkey
                                and c_custkey = o_custkey
                                and s_nationkey = n1.n_nationkey
                                and c_nationkey = n2.n_nationkey
                                and (
                                        (n1.n_name = 'JORDAN' and n2.n_name = 'INDIA')
                                        or (n1.n_name = 'INDIA' and n2.n_name = 'JORDAN')
                                )
                                and l_shipdate between date '1995-01-01' and date '1996-12-31'
                ) as shipping
        group by
                supp_nation,
                cust_nation,
                l_year
        order by
                supp_nation,
                cust_nation,
                l_year;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
