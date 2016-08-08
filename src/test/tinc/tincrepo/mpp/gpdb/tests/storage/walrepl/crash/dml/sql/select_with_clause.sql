-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- start_ignore
drop table if exists mpp15087_t;
-- end_ignore

create table mpp15087_t(code char(3), n numeric);
insert into mpp15087_t values ('abc',1);
insert into mpp15087_t values ('xyz',2);  
insert into mpp15087_t values ('def',3); 

with cte as 
	(
	select code, n, x 
	from mpp15087_t 
	, (select 100 as x) d
	)
select code from mpp15087_t t where 1= (select count(*) from cte where cte.code::text=t.code::text or cte.code::text = t.code::text);


with cte as
        (
        select count(*) from
        (
        select code, n, x
        from mpp15087_t
        , (select 100 as x) d
        ) FOO
        )
select code from mpp15087_t t where 1= (select * from cte);

with cte as
        (
        select count(*) from
        (
        select code, n, x
        from mpp15087_t
        , (select 100 as x) d
        ) FOO
        )
select code from mpp15087_t t where 1= (select count(*) from cte);
