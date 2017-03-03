\c gptest;

\d+ ao_table

select * from ao_table  order by a; -- order 1

\d+ new_ao_table

select * from new_ao_table  order by a; -- order 1
