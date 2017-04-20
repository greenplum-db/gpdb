set time zone PST8PDT;
-- @gucs gp_create_table_random_default_distribution=off
select * from uco_table1 order by bigint_col;

select * from uco_table2 order by col_numeric, col_text;

select * from ucoschema1.uco_table3 order by stud_id;

select * from  uco_table3 order by col_numeric, col_text;

select * from uco_table4 order by bigint_col;

select * from uco_table5 order by bigint_col;

select * from uco_table6 order by col_numeric, col_text;

select * from ucoschema1.uco_table7 order by col_numeric, col_text;

select * from uco_table8 order by bigint_col;

select * from uco_table9 order by bigint_col;

select * from uco_compr01 order by col_numeric;

select * from uco_compr02 order by col_numeric;

select count(*) from uco_part01;

select * from uco_part01 where ptcol=1;

select count(*) from uco_part02;

select * from uco_part04 where a=2;

\d+ uco_part05

select * from uco_part03 order by b;
