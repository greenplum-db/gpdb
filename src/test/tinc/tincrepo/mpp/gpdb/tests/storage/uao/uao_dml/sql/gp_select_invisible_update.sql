-- @Description Tests that gp_select_invisible is working correctly for updates
-- Test case for issue reported in STO-258
-- 

set gp_select_invisible = true;
select * from bar2 order by 1;
update bar2 set b = b+200 where a = 5;
select * from bar2 order by 1;
insert into bar2 select x , x from generate_series(11,20) as x;
select * from bar2 order by 1;
