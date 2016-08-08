-- MPP-7971: Issue loading data into a table with large number of segments
-- Johnny Soedomo

-- Modify postgres.conf
-- in postgresql.conf, max_appendonly_tables=11000, restart gpdb

CREATE TABLE integer_part (a int, b int, c int)
WITH (appendonly=true, compresslevel=5)
partition by range (a)
(
 partition aa start (0) end (10000) every (1)
);

insert into integer_part select i,i,i from generate_series(0, 9999) i;
