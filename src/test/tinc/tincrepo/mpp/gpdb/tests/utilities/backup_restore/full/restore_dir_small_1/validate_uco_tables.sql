-- @gucs gp_create_table_random_default_distribution=off
set time zone PST8PDT;
select * from uco_table1 order by bigint_col;

select * from ucoschema1.uco_table3 order by stud_id;

select * from uco_table6 order by col_numeric, col_text;

select * from ucoschema1.uco_table7 order by col_numeric;

select * from uco_compr01 order by col_numeric;

select count(*) from uco_part01;

select * from uco_part01 where ptcol=1;

select * from uco_part03 order by b;
