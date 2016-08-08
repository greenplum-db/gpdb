-- start_ignore
drop table if exists preptable cascade;
-- end_ignore

create table preptable(a int, b int) distributed by (b);

insert into preptable(a, b) values (1,1);
insert into preptable(a, b) values (2,2);
insert into preptable(a, b) values (3,3);
insert into preptable(a, b) values (4,1);
insert into preptable(a, b) values (5,2);
insert into preptable(a, b) values (6,3);

prepare pstmt as select a, b, sum(b) over (partition by b order by a rows between 1 preceding and 1 following) from preptable order by 1, 3;

drop table preptable;

execute pstmt;
