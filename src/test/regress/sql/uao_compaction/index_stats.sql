-- @Description Tests basic index stats after vacuuming

CREATE TABLE mytab(
          col_int int,
          col_text text,
          col_numeric numeric,
          col_unq int
          ) with(appendonly=true) DISTRIBUTED RANDOMLY;

Create index mytab_int_idx1 on mytab(col_int);

insert into mytab values(1,'aa',1001,101),(2,'bb',1002,102);

select * from mytab;
update mytab set col_text=' new value' where col_int = 1;
select * from mytab;
vacuum mytab;
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab';
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab_int_idx1';
