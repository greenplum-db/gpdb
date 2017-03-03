-- @author balasr3
-- @description TPC-H query19
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select
                sum(l_extendedprice* (1 - l_discount)) as revenue
        from
                lineitem,
                part
        where
                (
                        p_partkey = l_partkey
                        and p_brand = 'Brand#12'
                        and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
                        and l_quantity >= 3 and l_quantity <= 3 + 10
                        and p_size between 1 and 5
                        and l_shipmode in ('AIR', 'AIR REG')
                        and l_shipinstruct = 'DELIVER IN PERSON'
                )
                or
                (
                        p_partkey = l_partkey
                        and p_brand = 'Brand#31'
                        and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
                        and l_quantity >= 11 and l_quantity <= 11 + 10
                        and p_size between 1 and 10
                        and l_shipmode in ('AIR', 'AIR REG')
                        and l_shipinstruct = 'DELIVER IN PERSON'
                )
                or
                (
                        p_partkey = l_partkey
                        and p_brand = 'Brand#14'
                        and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
                        and l_quantity >= 30 and l_quantity <= 30 + 10
                        and p_size between 1 and 15
                        and l_shipmode in ('AIR', 'AIR REG')
                        and l_shipinstruct = 'DELIVER IN PERSON'
                );

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
