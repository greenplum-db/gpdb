-- start_ignore
drop table if exists aotest;
drop function if exists segonly();
drop function if exists notsegonly();
-- end_ignore

CREATE TABLE aotest
(dtype char(1), partnum int, sales numeric)
WITH (appendonly=true)
DISTRIBUTED BY (partnum);

insert into aotest(dtype, partnum, sales) values ('p', 1200, 5000.25);
insert into aotest(dtype, partnum, sales) values ('a', 4455, 10000.1234);
insert into aotest(dtype, partnum, sales) values ('n', 2764, 100.345789);

create function segonly() returns char(1) as $$
declare 
	dp char(1); 
begin 
	select into dp deptype from pg_depend order by deptype desc limit 1;
	if dp = 'p'
	then
		return 'p';
	else
		return 'n';
	end if;
end;
$$ language plpgsql;

create function notsegonly() returns char(1) as $$
declare 
        dp char(1); 
begin 
        select dtype into dp from aotest order by dtype desc limit 1;
        if dp = 'p'
        then
                return 'p';
        else
                return 'n';
        end if;
end;    
$$ language plpgsql;

-- This query is getting compiled on segments only
select dtype, sales, dtype = segonly() as window, sum(sales) 
over (partition by dtype = segonly()) as sum_window
from aotest
order by 1;

-- Confirm that query above is getting compiled on segments only
--   TEST: This should throw error since function is only getting compiled on segments
select dtype, sum(sales)
over (partition by dtype = notsegonly())
from aotest
order by 1;
