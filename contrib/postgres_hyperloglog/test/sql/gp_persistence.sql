SET search_path = public, pg_catalog;

create temp table foo with (appendonly=true, compresstype=quicklz) as
select (a%100)::int gb, a::varchar(255) v
from generate_series(1,100000) a;

select gb, hyperloglog_distinct(v) from foo group by 1 order by 1;

create temp table bar as
select gb, hyperloglog_accum(v) from foo group by 1;

select * from bar where gb = 1 order by 1;
