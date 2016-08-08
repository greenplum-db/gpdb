CREATE TABLE cr_table_with_oid (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
) WITH OIDS DISTRIBUTED RANDOMLY;
insert into cr_table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
select count(*) from cr_table_with_oid;
drop table cr_table_with_oid;
