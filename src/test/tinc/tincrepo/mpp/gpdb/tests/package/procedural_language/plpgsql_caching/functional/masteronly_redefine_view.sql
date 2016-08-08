-- start_ignore
drop function if exists segonlyview();
drop view if exists segview cascade;
-- end_ignore

create view segview as select deptype as dp from pg_depend where deptype = 'p' limit 1;

create function segonlyview() returns char(1) as $$
begin 
        return dp from segview;
end;    
$$ language plpgsql;

explain select deptype, segonlyview(), deptype = segonlyview() as same  
from pg_depend
where deptype = segonlyview()
order by 1
limit 2;

select deptype, segonlyview(), deptype = segonlyview() as same 
from pg_depend
where deptype = segonlyview()
order by 1
limit 2;

create or replace view segview as select deptype as dp from pg_depend where deptype = 'n' limit 1;

select deptype, segonlyview(), deptype = segonlyview() as same
from pg_depend
where deptype = segonlyview()
order by 1
limit 2;
