-- start_ignore
drop table if exists heap_t; 
drop function if exists plperlfunc();
-- end_ignore

create table heap_t
( a numeric, b int, c varchar )
distributed by (a);

insert into heap_t(a, b, c) values (9.9999999999, 1, 'test');
insert into heap_t(a, b, c) values (8.888888, 2, 'test');
insert into heap_t(a, b, c) values (4.444444444444, 4, 'testdef');
insert into heap_t(a, b, c) values (7.7777, 10, 'testdef');
insert into heap_t(a, b, c) values (5.5555555555555, 1, 'test');

create function plperlfunc() returns integer as $$
   my $droprv = spi_exec_query('drop table heap_t'); 
   my $createrv = spi_exec_query('create table heap_t(a numeric, b int, c varchar) distributed by (a);'); 
   $insertq = "insert into heap_t(a, b, c) values (9.9999999999, 1, 'test');";
   my $insertrv = spi_exec_query($insertq); 
   return $insertrv->{processed};
$$ LANGUAGE plperl;

select plperlfunc();

select plperlfunc();
