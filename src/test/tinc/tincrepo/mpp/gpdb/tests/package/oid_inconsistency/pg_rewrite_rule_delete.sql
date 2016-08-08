DROP table if exists bar2_ao cascade;
DROP table if exists foo2_ao cascade;

CREATE table foo2_ao (a int) with (appendonly=true) distributed by (a);
CREATE table bar2_ao (a int) distributed by (a);

CREATE rule two as on insert to bar2_ao do instead delete from foo2_ao where a=1;

SELECT oid,gp_segment_id,rulename,ev_class::regclass from pg_rewrite where ev_class = 'bar2_ao'::regclass;

SELECT oid,gp_segment_id,rulename,ev_class::regclass from gp_dist_random('pg_rewrite') where ev_class = 'bar2_ao'::regclass;


