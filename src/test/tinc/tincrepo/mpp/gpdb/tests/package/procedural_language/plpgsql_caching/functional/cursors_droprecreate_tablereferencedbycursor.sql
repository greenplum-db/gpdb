-- start_ignore
drop function if exists cursorfunc();
drop table if exists cursordata;
drop table if exists maxvalues;
-- end_ignore

create table maxvalues(max int) distributed randomly;
create table cursordata(id int) distributed randomly;
insert into cursordata select generate_series(1, 100);

select * from cursordata order by id desc limit 5;

create function cursorfunc()
returns void
as 
$$
declare
   a int;
   curs1 cursor for select max(id) from cursordata;
begin
   open curs1;
   fetch curs1 into a;
   close curs1;
   --drop table if exists maxvalues;
   --create table maxvalues(id int) distributed randomly;
   insert into  maxvalues values(a);
   drop table cursordata;
   create table cursordata(id int) distributed randomly;
end;
$$ language plpgsql;

select cursorfunc();

select cursorfunc();
