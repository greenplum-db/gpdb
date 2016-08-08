DROP table if exists bar_ao cascade;
DROP table if exists foo_ao cascade;

CREATE table foo_ao (a int) with (appendonly=true) distributed by (a);
CREATE table bar_ao (a int) distributed by (a);

CREATE rule one as on insert to bar_ao do instead update foo_ao set a=1;

SELECT oid,gp_segment_id,rulename,ev_class::regclass from pg_rewrite where ev_class = 'bar_ao'::regclass;

SELECT oid,gp_segment_id,rulename,ev_class::regclass from gp_dist_random('pg_rewrite') where ev_class = 'bar_ao'::regclass;


