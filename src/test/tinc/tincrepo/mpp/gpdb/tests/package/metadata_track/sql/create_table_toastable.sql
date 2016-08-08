--Toast Table

    CREATE TABLE mdt_toast_table (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

drop table mdt_toast_table;

