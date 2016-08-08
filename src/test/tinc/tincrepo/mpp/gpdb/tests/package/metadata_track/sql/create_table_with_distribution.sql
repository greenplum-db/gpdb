--Tables with distributed randomly and distributed columns

    CREATE TABLE mdt_table_distributed_by (
    col_with_default numeric DEFAULT 0,
    col_with_default_drop_default character varying(30) DEFAULT 'test1',
    col_with_constraint numeric UNIQUE
    ) DISTRIBUTED BY (col_with_constraint);

    CREATE TABLE mdt_table_distributed_randomly (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

drop table mdt_table_distributed_by;
drop table mdt_table_distributed_randomly;

