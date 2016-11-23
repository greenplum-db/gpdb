-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE if exists mytab;
CREATE TABLE mytab(
          col_int int,
          col_text text,
          col_numeric numeric,
          col_unq int
          ) with(appendonly=true) DISTRIBUTED RANDOMLY;

Create index mytab_int_idx1 on mytab(col_int);
select * from mytab order by col_int;

insert into mytab values(1,'aa',1001,101),(2,'bb',1002,102);
