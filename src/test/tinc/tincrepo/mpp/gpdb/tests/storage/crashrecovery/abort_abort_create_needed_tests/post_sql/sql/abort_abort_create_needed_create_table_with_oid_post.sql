CREATE TABLE abort_create_needed_cr_table_with_oid (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
) WITH OIDS DISTRIBUTED RANDOMLY;
insert into abort_create_needed_cr_table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
select count(*) from abort_create_needed_cr_table_with_oid;
DROP TABLE abort_create_needed_cr_table_with_oid;
