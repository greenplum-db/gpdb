create table employees1(id integer,empname varchar,sal integer) distributed randomly;

insert into employees1 values (1,'mohit',1000);
insert into employees1 values (2,'lalit',2000);
insert into employees1 select i,i||'_'||repeat('text',100),i from generate_series(3,100)i;

create view empvw1 as select * from employees1;
