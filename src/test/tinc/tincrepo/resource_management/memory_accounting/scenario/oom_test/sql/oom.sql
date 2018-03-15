-- @author ramans2
-- @created 2014-02-05 12:00:00
-- @modified 2014-02-05 12:00:00
-- @description Single query that runs OOM

show gp_vmem_protect_limit;

--start_ignore
set optimizer_enable_motion_redistribute=off;
--end_ignore

select 2 as oom_test;
select count(*) from
  (select l0.l_partkey from
    (lineitem l0
     left outer join lineitem l1 on l0.l_partkey = l1.l_partkey
     left outer join lineitem l2 on l1.l_partkey = l2.l_partkey
     left outer join lineitem l3 on l2.l_partkey = l3.l_partkey
     left outer join lineitem l4 on l3.l_partkey = l4.l_partkey
     left outer join lineitem l5 on l4.l_partkey = l5.l_partkey
     left outer join lineitem l6 on l5.l_partkey = l6.l_partkey
     left outer join lineitem l7 on l6.l_partkey = l7.l_partkey
     left outer join lineitem l8 on l7.l_partkey = l8.l_partkey
     left outer join lineitem l9 on l8.l_partkey = l9.l_partkey
     left outer join lineitem l10 on l9.l_partkey = l10.l_partkey
     left outer join lineitem l11 on l10.l_partkey = l11.l_partkey
     left outer join lineitem l12 on l11.l_partkey = l12.l_partkey
     left outer join lineitem l13 on l12.l_partkey = l13.l_partkey
     left outer join lineitem l14 on l13.l_partkey = l14.l_partkey
     left outer join lineitem l15 on l14.l_partkey = l15.l_partkey)
    order by l0.l_partkey) as foo;
