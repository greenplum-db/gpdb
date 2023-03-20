create table bm_test (i int, t text);
insert into bm_test select i % 3000, (i % 3000)::text  from generate_series(1, 30000) i;

1: select * from bm_test limit 10 offset 20;
2: select * from bm_test limit 10 offset 20;
3: select * from bm_test limit 10 offset 20;

create index bm_idx on bm_test using bitmap(t);

drop index bm_idx;

1: delete from bm_test where i = 1024;
2: update bm_test set i = 2048 where i = 256;
3: vacuum bm_test;
4: insert into bm_test select i % 3000, (i % 3000)::text  from generate_series(1, 30000) i;

create index bm_idx on bm_test using bitmap(t);

-- clean up
drop table bm_test;
