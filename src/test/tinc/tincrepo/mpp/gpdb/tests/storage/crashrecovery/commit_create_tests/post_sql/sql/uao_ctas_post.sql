-- @product_version gpdb: [4.3.0.0-]
\d+ cr_uao_ctas
select count(*) from cr_uao_ctas;
drop table cr_uao_ctas;
drop table cr_seed_table_for_uao;
