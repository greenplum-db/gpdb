-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists tbl_cds_buysell_orders_new;
drop table if exists tbl_cds_buysell_orders_new2;
drop external table if exists ext_tbl_cds_buysell_orders_new;

create table tbl_cds_buysell_orders_new (a int, b int, c int, d int, e int, f int, g int)
  with (appendonly=true, orientation=column, compresslevel=5)
  distributed by (a,b,c);

create external table ext_tbl_cds_buysell_orders_new (a int, b int, c int)
  location ('gpfdist://localhost:8080/datafile_small.csv') format 'csv' (delimiter ' ') encoding 'utf8';

create table tbl_cds_buysell_orders_new2 as  select * from ext_tbl_cds_buysell_orders_new order by a,b,c
  distributed by (a,b,c);
