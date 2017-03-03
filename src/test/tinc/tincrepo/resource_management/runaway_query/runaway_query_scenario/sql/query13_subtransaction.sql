-- @author balasr3
-- @description TPC-H query13
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                c_count,
                count(*) as custdist
        from
                (
                        select
                                c_custkey,
                                count(o_orderkey)
                        from
                                customer left outer join orders on
                                        c_custkey = o_custkey
                                        and o_comment not like '%express%accounts%'
                        group by
                                c_custkey
                ) as c_orders (c_custkey, c_count)
        group by
                c_count
        order by
                custdist desc,
                c_count desc;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
