-- @author balasr3
-- @description TPC-H query11
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

select
                ps_partkey,
                sum(ps_supplycost * ps_availqty) as value
        from
                partsupp,
                supplier,
                nation
        where
                ps_suppkey = s_suppkey
                and s_nationkey = n_nationkey
                and n_name = 'BRAZIL'
        group by
                ps_partkey having
                        sum(ps_supplycost * ps_availqty) > (
                                select
                                        sum(ps_supplycost * ps_availqty) * 0.0000015625
                                from
                                        partsupp,
                                        supplier,
                                        nation
                                where
                                        ps_suppkey = s_suppkey
                                        and s_nationkey = n_nationkey
                                        and n_name = 'BRAZIL'
                        )
        order by
                value desc;
