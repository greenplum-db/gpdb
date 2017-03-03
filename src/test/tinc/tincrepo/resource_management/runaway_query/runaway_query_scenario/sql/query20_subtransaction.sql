-- @author balasr3
-- @description TPC-H query20
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select 
                s_name,
                s_address 
        from 
                supplier, 
                nation 
        where 
                s_suppkey in( 
                        select 
                                ps_suppkey 
                        from 
                                partsupp
                        where 
				ps_partkey in (
					select
						p_partkey
					from
						part
					where
						p_name like 'antique%'
				)
			and ps_availqty > (
				select
					0.5 * sum(l_quantity)
				from
					lineitem
				where
					l_partkey = ps_partkey
					and l_suppkey = ps_suppkey
					and l_shipdate >= date '1994-01-01'
					and l_shipdate < date '1994-01-01' + interval '1 year'
			)
		)
                and s_nationkey = n_nationkey 
                and n_name = 'GERMANY'
        order by 
                s_name;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
