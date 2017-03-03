-- @author ramans2
-- @created 2014-03-25 12:00:00
-- @modified 2014-03-25 12:00:00
-- @gucs gp_dump_memory_usage=on
-- @description Single query to test GUC gp_dump_memory_usage

show gp_vmem_protect_limit;

select count(*) from
  (select l0.l_orderkey from
    (lineitem l0
     left outer join lineitem l1 on l0.l_orderkey = l1.l_orderkey
     left outer join lineitem l2 on l1.l_orderkey = l2.l_orderkey
     left outer join lineitem l3 on l2.l_orderkey = l3.l_orderkey
     left outer join lineitem l4 on l3.l_orderkey = l4.l_orderkey
     left outer join lineitem l5 on l4.l_orderkey = l5.l_orderkey
     left outer join lineitem l6 on l5.l_orderkey = l6.l_orderkey
     left outer join lineitem l7 on l6.l_orderkey = l7.l_orderkey
     left outer join lineitem l8 on l7.l_orderkey = l8.l_orderkey
     left outer join lineitem l9 on l8.l_orderkey = l9.l_orderkey
     left outer join lineitem l10 on l9.l_orderkey = l10.l_orderkey
     left outer join lineitem l11 on l10.l_orderkey = l11.l_orderkey
     left outer join lineitem l12 on l11.l_orderkey = l12.l_orderkey
     left outer join lineitem l13 on l12.l_orderkey = l13.l_orderkey
     left outer join lineitem l14 on l13.l_orderkey = l14.l_orderkey
     left outer join lineitem l15 on l14.l_orderkey = l15.l_orderkey)
    order by l0.l_orderkey) as foo;
