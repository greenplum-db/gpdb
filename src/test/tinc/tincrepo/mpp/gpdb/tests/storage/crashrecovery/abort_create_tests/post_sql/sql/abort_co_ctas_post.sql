CREATE TABLE cr_co_ctas WITH (appendonly=true,orientation=column) AS SELECT * FROM cr_seed_table_for_co;
\d cr_co_ctas
select count(*) from cr_co_ctas;
drop table cr_co_ctas;
drop table cr_seed_table_for_co;
