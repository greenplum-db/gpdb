-- @author balasr3
-- @description TPC-H query04
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                o_orderpriority,
                count(*) as order_count
from
                orders
where
                o_orderdate >= date '1996-03-01'
                and o_orderdate < date '1996-03-01' + interval '3 month'
                and exists (
                        select
                                *
                        from
                                lineitem
                        where
                                l_orderkey = o_orderkey
                                and l_commitdate < l_receiptdate
                )
group by
                o_orderpriority
order by
                o_orderpriority;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
