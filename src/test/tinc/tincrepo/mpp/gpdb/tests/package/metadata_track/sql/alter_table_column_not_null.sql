--ALTER column [ SET | DROP ] NOT NULL

          CREATE TABLE mdt_alter_table1(
          col_text text,
          col_numeric numeric NOT NULL
          ) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_alter_table1 ALTER COLUMN col_numeric DROP NOT NULL;

          CREATE TABLE mdt_alter_table2(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_alter_table2 ALTER COLUMN col_numeric SET NOT NULL;

drop table mdt_alter_table1;
drop table mdt_alter_table2;

