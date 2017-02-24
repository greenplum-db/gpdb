-- @author balasr3
-- @description TPC-H query16
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select 
                p_brand, 
                p_type, 
                p_size, 
                count(distinct ps_suppkey) as supplier_cnt 
        from 
                partsupp, 
                part 
        where 
                p_partkey = ps_partkey 
                and p_brand <> 'Brand#55' 
                and p_type not like 'SMALL POLISHED%' 
                and p_size in (14, 36, 50, 23, 2, 19, 18, 43)
                and ps_suppkey not in (
                    select
                        s_suppkey
                    from
                        supplier
                    where
                        s_comment like '%Customer%Complaints%'
                ) 
        group by 
                p_brand, 
                p_type, 
                p_size 
        order by 
                supplier_cnt desc, 
                p_brand, 
                p_type, 
                p_size;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
