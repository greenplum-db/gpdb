-- @product_version gpdb: [4.3.0.0-]
CREATE TABLE cr_uao_ctas WITH (appendonly=true) AS SELECT * FROM cr_seed_table_for_uao;
\d cr_uao_ctas
select count(*) from cr_uao_ctas;
drop table cr_uao_ctas;
drop table cr_seed_table_for_uao;
