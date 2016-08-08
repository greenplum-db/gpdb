DROP view if exists tt1 cascade;
DROP table if exists tt2 cascade;

CREATE table tt1 (a int) distributed by (a);
CREATE table tt2 (a int) distributed by (a);

CREATE rule "_RETURN" as on select to tt1
        do instead select * from tt2;

select oid,gp_segment_id,rulename,ev_class::regclass from pg_rewrite where ev_class = 'tt1'::regclass;

select oid,gp_segment_id,rulename,ev_class::regclass from gp_dist_random('pg_rewrite') where ev_class = 'tt1'::regclass;


