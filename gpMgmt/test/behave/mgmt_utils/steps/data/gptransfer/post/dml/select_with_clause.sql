\c gptest;

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
