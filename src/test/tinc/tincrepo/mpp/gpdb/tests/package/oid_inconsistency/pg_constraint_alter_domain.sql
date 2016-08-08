DROP domain if exists pg_alter_domain;

CREATE domain pg_alter_domain AS TEXT ;

ALTER domain pg_alter_domain add CONSTRAINT cons_check check (char_length(VALUE) = 5);
select oid, gp_segment_id, conname from pg_constraint where conname ~ 'cons_check';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conname ~ 'cons_check';

