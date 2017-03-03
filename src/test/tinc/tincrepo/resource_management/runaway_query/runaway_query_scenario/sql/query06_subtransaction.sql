-- @author balasr3
-- @description TPC-H query06
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                sum(l_extendedprice * l_discount) as revenue
from
                lineitem
where
                l_shipdate >= date '1993-01-01'
                and l_shipdate < date '1993-01-01' + interval '1 year'
                and l_discount between 0.03 - 0.01 and 0.03 + 0.01
                and l_quantity < 24;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
