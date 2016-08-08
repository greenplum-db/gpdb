--Temp Table

    CREATE TEMP TABLE mdt_temp_table(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

--global table

    CREATE GLOBAL TEMP TABLE mdt_temp_table_global (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

--local table

    CREATE LOCAL TEMP TABLE mdt_temp_table_local (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

drop table mdt_temp_table;
drop table mdt_temp_table_global;
drop table mdt_temp_table_local;

