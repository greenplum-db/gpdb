-- A query with limit while the table is locked from master

Drop table if exists query_lock;
Create table query_lock( i int, j int, k int) ;
Insert into query_lock select i, i % 2, i % 10 from generate_series(1, 100)i;
1:Begin;
2:Begin;
2:Alter table query_lock add column my_newcol int;
1&:Select i, count(*) over (partition by j order by i) from query_lock order by 1 limit 1;
2:Select pg_sleep(5);Commit;
1<:
