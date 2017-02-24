-- start_ignore
drop table if exists foo;
drop table if exists bar;
-- end_ignore

create table foo (a int, b int);
create table bar (x int, y int);

insert into foo values (1, 2);
insert into foo values (12, 20);
insert into foo values (NULL, 2);
insert into foo values (15, 2);
insert into foo values (NULL, NULL);
insert into foo values (1, 12);
insert into foo values (1, 102);

insert into bar select i/10, i from generate_series(1, 100)i;
insert into bar values (NULL, 101);
insert into bar values (NULL, 102);
insert into bar values (NULL, NULL);
