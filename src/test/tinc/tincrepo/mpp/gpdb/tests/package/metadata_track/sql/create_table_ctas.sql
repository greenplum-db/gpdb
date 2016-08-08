--Table Creation using Create Table As (CTAS) with both the new tables columns being explicitly or implicitly created


    CREATE TABLE mdt_test_table(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    before_rename_col int4,
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_set_default numeric)DISTRIBUTED RANDOMLY;

CREATE TABLE mdt_ctas_table1 AS SELECT * FROM mdt_test_table;
CREATE TABLE mdt_ctas_table2 AS SELECT text_col,bigint_col,char_vary_col,numeric_col FROM mdt_test_table;

drop table mdt_test_table;
drop table mdt_ctas_table1;
drop table mdt_ctas_table2;

