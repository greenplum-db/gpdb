1:drop table if exists mpp24606;
1:create table mpp24606(a int, b int);
1:insert into mpp24606 select i, i from generate_series(1, 1000)i;
1:select case when pg_sleep(3) is not null then 1 end from pg_class where relname = 'mpp24606' and pg_total_relation_size(oid) is not null;
2:drop table mpp24606;
