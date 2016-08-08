-- @product_version gpdb: [4.3.0.0-]
CREATE TABLE cr_uaocs_ctas WITH (appendonly=true,orientation=column) AS SELECT * FROM cr_seed_table_for_uaocs;
\d cr_uaocs_ctas
select count(*) from cr_uaocs_ctas;
drop table cr_uaocs_ctas;
drop table cr_seed_table_for_uaocs;
