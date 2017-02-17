-- @author balasr3
-- @description TPC-H query08
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

select
                o_year,
                sum(case
                        when nation = 'RUSSIA' then volume
                        else 0
                end) / sum(volume) as mkt_share
        from
                (
                        select
                                extract(year from o_orderdate) as o_year,
                                l_extendedprice * (1 - l_discount) as volume,
                                n2.n_name as nation
                        from
                                part,
                                supplier,
                                lineitem,
                                orders,
                                customer,
                                nation n1,
                                nation n2,
                                region
                        where
                                p_partkey = l_partkey
                                and s_suppkey = l_suppkey
                                and l_orderkey = o_orderkey
                                and o_custkey = c_custkey
                                and c_nationkey = n1.n_nationkey
                                and n1.n_regionkey = r_regionkey
                                and r_name = 'EUROPE'
                                and s_nationkey = n2.n_nationkey
                                and o_orderdate between date '1995-01-01' and date '1996-12-31'
                                and p_type = 'LARGE BRUSHED STEEL'
                ) as all_nations
        group by
                o_year
        order by
                o_year;
