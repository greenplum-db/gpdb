-- @gucs gp_create_table_random_default_distribution=off
set time zone PST8PDT;
select * from uao_table1_1 order by bigint_col;

select * from uao_table2_1 order by col_numeric;

select * from uaoschema1_1.uao_table3_1 order by stud_id;

select * from  uao_table3_1 order by col_numeric;

select * from uao_table4_1 order by bigint_col;

select * from uao_table5_1 order by bigint_col;

select * from uao_table6_1 order by col_numeric;

select * from uaoschema1_1.uao_table7_1 order by col_numeric;

select * from uao_table8_1 order by bigint_col;

select * from uao_table9_1 order by bigint_col;

select * from uao_compr01_1 order by col_numeric;

select * from uao_compr02_1 order by col_numeric;

select count(*) from uao_part01_1;

select * from uao_part01_1 where ptcol=1;

select count(*) from uao_part02_1;

select * from uao_part04_1 where a=2;

\d+ uao_part05_1

select * from uao_part03_1 order by b;
