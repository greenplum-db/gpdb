DROP table if exists bar3 cascade;
DROP table if exists foo3 cascade;

CREATE table foo3 (a int) distributed by (a);
CREATE table bar3 (a int) distributed by (a);

CREATE rule three as on insert to bar3 do instead insert into foo3(a) values(1);

SELECT oid,gp_segment_id,rulename,ev_class::regclass from pg_rewrite where ev_class = 'bar3'::regclass;

SELECT oid,gp_segment_id,rulename,ev_class::regclass from gp_dist_random('pg_rewrite') where ev_class = 'bar3'::regclass;


