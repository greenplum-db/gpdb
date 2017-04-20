-- @gucs gp_create_table_random_default_distribution=off
set time zone PST8PDT;
select * from public.ao_table1 order by 1;
select * from public.co_part01_1_prt_3_2_prt_sub1 order by 1,2,3;
select * from new_coschema1.co_table3 order by 1;
select count(*) from ao_table4;
select count(*) from heap_table3;
select count(*) from heap_table4;
select count(*) from mixed_part01;
