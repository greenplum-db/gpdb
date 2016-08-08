--ALTER column SET STATISTICS integer

          CREATE TABLE mdt_alter_set_statistics_table(
          col_text text,
          col_numeric numeric NOT NULL
          ) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_alter_set_statistics_table ALTER col_numeric SET STATISTICS 1;

drop table mdt_alter_set_statistics_table;

