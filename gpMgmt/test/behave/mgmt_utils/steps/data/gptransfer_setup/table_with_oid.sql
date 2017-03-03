--Table with Oids

    CREATE TABLE table_with_oid (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) WITH OIDS DISTRIBUTED RANDOMLY;

    insert into table_with_oid values ('0_zero', 0, '0_zero', 0);
    insert into table_with_oid values ('1_zero', 1, '1_zero', 1);
    insert into table_with_oid values ('2_zero', 2, '2_zero', 2);
    insert into table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
