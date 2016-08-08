-- start_ignore
drop function if exists cacheheap();
-- end_ignore

create function cacheheap() returns void as
$$
begin
	drop table if exists heaptest;
	create table heaptest(id int) distributed randomly;
	insert into heaptest values(1);
end;
$$ language plpgsql;

select cacheheap(), null as expected;

select * from heaptest order by id;

select cacheheap(), null as expected;
