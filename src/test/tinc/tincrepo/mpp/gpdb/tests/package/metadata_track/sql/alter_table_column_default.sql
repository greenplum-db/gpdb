--ALTER column SET DEFAULT expression

          CREATE TABLE mdt_test_alter_table3(
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

          ALTER TABLE mdt_test_alter_table3 ALTER COLUMN col_set_default SET DEFAULT 0;

--dropping default

    CREATE TABLE mdt_test_column2(
    toast_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    non_toast_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_with_constraint numeric UNIQUE,
    col_with_default_text character varying(30) DEFAULT 'test1'
    );

    ALTER TABLE mdt_test_column2 ALTER COLUMN col_with_default_text DROP DEFAULT;

drop table mdt_test_alter_table3;
drop table mdt_test_column2;

