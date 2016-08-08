DROP language if exists plperl cascade;

CREATE language plperl;

select distinct lanvalidator, lanname from pg_language where lanname='plperl';
select distinct lanvalidator, lanname from gp_dist_random('pg_language') where lanname='plperl';


