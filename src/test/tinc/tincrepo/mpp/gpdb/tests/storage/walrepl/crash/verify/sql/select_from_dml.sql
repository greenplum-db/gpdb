-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

select count(*) from tbl_with_all_type;

select * from int8_tbl order by q1,q2;

select * from int4_tbl order by f1;

select count(*) from tenk1;

with cte as
        (
        select count(*) from
        (
        select code, n, x
        from mpp15087_t
        , (select 100 as x) d
        ) FOO
        )
select code from mpp15087_t t where 1= (select count(*) from cte) order by code;

select * from csq_t4 order by a;

select count(*) from tt2 full outer join tt1 on (tt1.x=tt2.z) where tt2.z=2;
