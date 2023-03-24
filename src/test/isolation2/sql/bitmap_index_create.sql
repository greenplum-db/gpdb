create table bm_test(a int, b int);

-- insert some data into a one segment
insert into bm_test values (1, 1);
insert into bm_test values (1, 2);
insert into bm_test values (1, 3);
insert into bm_test values (12, 1);

-- update the first tuple using HOT, since this page
-- just have 4 tuples, there have full free space to
-- use HOT update.
update bm_test set b = 1 where a = 1 and b = 1;

-- After the update, the tids that the value of b is equal to 1
-- we scanned will not be in order, due to HOT.
create index idx_bm_test on bm_test using bitmap(b);
select * from bm_test where b = 1;

-- clean up
drop table bm_test;
