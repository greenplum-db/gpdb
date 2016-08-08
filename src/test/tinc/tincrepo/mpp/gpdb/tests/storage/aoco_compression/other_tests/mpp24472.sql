-- 
-- @created 2014-09-17 13:30:00
-- @tags MPP-24472 bfv
-- @optimizer_mode on
-- @gucs optimizer_enable_ctas=on
-- @gpopt 1.485
-- @product_version gpdb: [4.3.3-]
-- @description MPP-24472: Inconsistent compression ratio with ORCA on

insert into tbl_cds_buysell_orders_new (a,b,c) select ex.a,ex.b,ex.c from ext_tbl_cds_buysell_orders_new ex order by a,b,c;

select pg_size_pretty(pg_relation_size('tbl_cds_buysell_orders_new')), get_ao_compression_ratio('tbl_cds_buysell_orders_new');

truncate table tbl_cds_buysell_orders_new;

insert into tbl_cds_buysell_orders_new (a,b,c) select ex.a,ex.b,ex.c from tbl_cds_buysell_orders_new2 ex  order by a,b,c;

select pg_size_pretty(pg_relation_size('tbl_cds_buysell_orders_new')), get_ao_compression_ratio('tbl_cds_buysell_orders_new');
