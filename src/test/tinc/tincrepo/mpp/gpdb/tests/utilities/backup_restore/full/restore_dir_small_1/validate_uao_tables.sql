-- @gucs gp_create_table_random_default_distribution=off
set time zone PST8PDT;
select * from uao_table1 order by bigint_col;

select * from uao_table2 order by col_numeric;

select * from uaoschema1.uao_table3 order by stud_id;

select * from  uao_table3 order by col_numeric;

select * from uao_table4 order by bigint_col;

select * from uao_table5 order by bigint_col;

select * from uao_table6 order by col_numeric;

select * from uaoschema1.uao_table7 order by col_numeric;

select * from uao_table8 order by bigint_col;

select * from uao_table9 order by bigint_col;

select * from uao_compr01 order by col_numeric;

select * from uao_compr02 order by col_numeric;

select count(*) from uao_part01;

select * from uao_part01 where ptcol=1;

select count(*) from uao_part02;

select * from uao_part04 where a=2;

\d+ uao_part05

select * from uao_part03 order by b;
