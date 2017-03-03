drop table if exists r11_1;
create table r11_1(a1 int, a2 char(5), a3 boolean DEFAULT false) with (appendonly=true, orientation=column) distributed randomly;
insert into r11_1(a1,a2,a3) values(10, 'xyz', 'f');
alter table r11_1 set distributed by (a2);

drop table if exists pg_attribute_r11_1;
create table pg_attribute_r11_1 as 
  select * from pg_attribute where attrelid='r11_1'::regclass order by attname;
alter table pg_attribute_r11_1 drop column attrelid;

drop table if exists r11_2;
create table r11_2 as select 1 as a, 1 as b distributed by (a);
alter table r11_2 drop column a;

drop table if exists pg_attribute_r11_2;
create table pg_attribute_r11_2 as 
  select * from pg_attribute where attrelid='r11_2'::regclass order by attname;
alter table pg_attribute_r11_2 drop column attrelid;
