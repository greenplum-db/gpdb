--ADD column type [ column_constraint [ ... ] ]

          CREATE TABLE mdt_test_alter_table(
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

          ALTER TABLE mdt_test_alter_table ADD COLUMN added_col character varying(30);

drop table mdt_test_alter_table;

