-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists duplicate_entries_check_test1;
drop table if exists duplicate_entries_check_test2;
drop table if exists duplicate_entries_check_test3; 

create table duplicate_entries_check_test1 (i int) partition by range(i) (start(1) end(1000) every(1));
create table duplicate_entries_check_test2 (i int) partition by range(i) (start(1) end(1000) every(1));
alter table duplicate_entries_check_test2 drop partition for (100);
alter table duplicate_entries_check_test2 drop partition for (101);
alter table duplicate_entries_check_test2 drop partition for (102);
alter table duplicate_entries_check_test2 drop partition for (103);
alter table duplicate_entries_check_test2 drop partition for (104);
alter table duplicate_entries_check_test2 drop partition for (105);
drop table duplicate_entries_check_test1;
create table duplicate_entries_check_test3(i int);
 
drop table if exists duplicate_entries_check_t1heap;
drop table if exists duplicate_entries_check_t2;
create table duplicate_entries_check_t1heap (c1 int, c2 char(100)) distributed by (c1);
insert into duplicate_entries_check_t1heap select generate_series(1,100000), 'abcdefg';
insert into duplicate_entries_check_t1heap values (112232, 'xyz');
 
delete from duplicate_entries_check_t1heap where c1 not in (1, 12, 45, 46, 112232);
select count(*) from duplicate_entries_check_test2;
select count(*) from duplicate_entries_check_test3;
select count(*) from duplicate_entries_check_t1heap;
