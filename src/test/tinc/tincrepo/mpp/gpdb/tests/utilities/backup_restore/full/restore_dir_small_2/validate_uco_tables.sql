-- @gucs gp_create_table_random_default_distribution=off
set time zone PST8PDT;
select * from uco_table1_1 order by bigint_col;

select * from ucoschema1_1.uco_table3_1 order by stud_id;

select * from uco_table6_1 order by col_numeric, col_text;

select * from ucoschema1_1.uco_table7_1 order by col_numeric, col_text;

select * from uco_compr01_1 order by col_numeric;

select count(*) from uco_part01_1;

select * from uco_part01_1 where ptcol=1;

select * from uco_part03_1 order by b;
