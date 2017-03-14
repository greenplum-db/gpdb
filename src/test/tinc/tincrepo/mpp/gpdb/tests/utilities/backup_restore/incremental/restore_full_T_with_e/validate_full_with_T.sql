-- @gucs gp_create_table_random_default_distribution=off
set time zone PST8PDT;
select * from public.ao_table1 order by 1;
select count(*) from heap_table3;
select count(*) from mixed_part01;
