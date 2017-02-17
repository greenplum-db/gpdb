-- @author balasr3
-- @description TPC-H query14
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                100.00 * sum(case
                        when p_type like 'PROMO%'
                                then l_extendedprice * (1 - l_discount)
                        else 0
                end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
        from
                lineitem,
                part
        where
                l_partkey = p_partkey
                and l_shipdate >= date '1997-01-01'
                and l_shipdate < date '1997-01-01' + interval '1 month';

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
