-- @author balasr3
-- @description TPC-H query17
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

select
              sum(l_extendedprice) / 7.0 as avg_yearly
        from
              lineitem,
              part
        where
              p_partkey = l_partkey
              and p_brand = 'Brand#52'
              and p_container = 'SM DRUM'
              and l_quantity < (
		select
			0.2 * avg(l_quantity)
		from
			lineitem
		where
			l_partkey = p_partkey
              );
