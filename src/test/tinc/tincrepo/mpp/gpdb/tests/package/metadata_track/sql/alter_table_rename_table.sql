--Rename Table

          CREATE TABLE mdt_table_name(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_table_name RENAME TO mdt_table_new_name;

drop table mdt_table_new_name;

