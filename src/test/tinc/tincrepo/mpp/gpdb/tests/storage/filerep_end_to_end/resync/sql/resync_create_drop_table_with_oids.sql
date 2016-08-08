-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--Table with Oids

    CREATE TABLE resync_table_with_oid1 (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) WITH OIDS DISTRIBUTED RANDOMLY;

-- start_ignore
    insert into resync_table_with_oid1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
-- end_ignore



    CREATE TABLE resync_table_with_oid2 (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) WITH OIDS DISTRIBUTED RANDOMLY;

-- start_ignore
    insert into resync_table_with_oid2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
-- end_ignore


    CREATE TABLE resync_table_with_oid3 (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) WITH OIDS DISTRIBUTED RANDOMLY;

-- start_ignore
    insert into resync_table_with_oid3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
-- end_ignore


DROP TABLE sync1_table_with_oid6;
DROP TABLE ck_sync1_table_with_oid5;
DROP TABLE ct_table_with_oid3;
DROP TABLE resync_table_with_oid1;
