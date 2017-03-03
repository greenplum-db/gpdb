\c gptest;

-- start_ignore
drop table if exists smallt;
-- end_ignore
create table smallt (i int, t text, d date);
insert into smallt select i%10, 'text ' || (i%15), '2011-01-01'::date + ((i%20) || ' days')::interval
from generate_series(0, 99) i;
