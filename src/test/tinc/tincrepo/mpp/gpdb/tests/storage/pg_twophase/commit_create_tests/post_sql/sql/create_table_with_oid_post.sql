-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\d cr_table_with_oid
insert into cr_table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
select count(*) from cr_table_with_oid;
DROP TABLE cr_table_with_oid;
