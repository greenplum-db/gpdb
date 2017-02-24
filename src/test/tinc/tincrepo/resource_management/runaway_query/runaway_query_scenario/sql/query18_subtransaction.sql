-- @author balasr3
-- @description TPC-H query18
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                c_name,
                c_custkey,
                o_orderkey,
                o_orderdate,
                o_totalprice,
                sum(l_quantity)
        from
                customer,
                orders,
                lineitem
        where
                o_orderkey in (
                        select
                                l_orderkey
                        from
                                lineitem
                        group by
                                l_orderkey having
                                        sum(l_quantity) > 312
                )
                and c_custkey = o_custkey
                and o_orderkey = l_orderkey
        group by
                c_name,
                c_custkey,
                o_orderkey,
                o_orderdate,
                o_totalprice
        order by
                o_totalprice desc,
                o_orderdate
        LIMIT 100;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
