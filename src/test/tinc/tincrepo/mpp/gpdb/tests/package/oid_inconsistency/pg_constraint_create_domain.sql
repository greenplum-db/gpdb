DROP domain if exists pg_create_domain;

CREATE domain pg_create_domain AS TEXT CONSTRAINT cons2_check check (char_length(VALUE) = 5);

select oid, gp_segment_id, conname from pg_constraint where conname ~ 'cons2_check';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conname ~ 'cons2_check';

