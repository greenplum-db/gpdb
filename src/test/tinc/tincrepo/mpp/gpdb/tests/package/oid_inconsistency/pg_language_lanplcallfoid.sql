DROP language if exists plperl cascade;

CREATE language plperl;

select distinct lanplcallfoid, lanname from pg_language where lanname='plperl';
select distinct lanplcallfoid, lanname from gp_dist_random('pg_language') where lanname='plperl';

