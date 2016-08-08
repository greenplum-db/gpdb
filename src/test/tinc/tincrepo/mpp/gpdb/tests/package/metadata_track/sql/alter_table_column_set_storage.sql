--ALTER column SET STORAGE

          CREATE TABLE mdt_alter_set_storage_table(
          col_text text,
          col_numeric numeric NOT NULL
          ) DISTRIBUTED RANDOMLY;

         ALTER TABLE mdt_alter_set_storage_table ALTER col_text SET STORAGE PLAIN;

drop table mdt_alter_set_storage_table;

