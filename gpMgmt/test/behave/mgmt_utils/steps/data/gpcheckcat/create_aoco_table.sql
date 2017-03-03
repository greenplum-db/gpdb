drop table if exists aoco;

CREATE TABLE aoco (col1 int) WITH (appendonly=true) distributed randomly;

insert into aoco values(1);
insert into aoco values(2);
insert into aoco values(3);
insert into aoco values(4);
insert into aoco values(5);
insert into aoco values(6);
insert into aoco values(7);
insert into aoco values(8);
insert into aoco values(9);
insert into aoco values(10);
insert into aoco values(11);
