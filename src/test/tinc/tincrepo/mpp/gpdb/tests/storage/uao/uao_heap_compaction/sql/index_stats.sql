-- @Description Tests basic index stats after vacuuming
-- 

select * from mytab;
update mytab set col_text=' new value' where col_int = 1;
select * from mytab;
vacuum mytab;
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab';
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab_int_idx1';
