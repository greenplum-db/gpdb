--SET & RESET ( storage_parameter = value , ... )

          CREATE TABLE mdt_table_set_storage_parameters (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) with (APPENDONLY=TRUE) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_table_set_storage_parameters SET WITH (COMPRESSLEVEL= 5);

          CREATE TABLE mdt_table_set_storage_parameters1 (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) with (APPENDONLY=TRUE) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_table_set_storage_parameters1 SET WITH (FILLFACTOR=50);

          CREATE TABLE mdt_table_set_storage_parameters2 (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) with (APPENDONLY=TRUE) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_table_set_storage_parameters2 SET WITH (FILLFACTOR=50);
          ALTER TABLE mdt_table_set_storage_parameters2 RESET (FILLFACTOR);


drop table mdt_table_set_storage_parameters;
drop table mdt_table_set_storage_parameters1;
drop table mdt_table_set_storage_parameters2;

