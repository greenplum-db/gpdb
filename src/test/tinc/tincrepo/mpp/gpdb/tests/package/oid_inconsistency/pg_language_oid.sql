DROP language if exists plperl cascade;

CREATE language plperl;

select distinct oid, lanname from pg_language where lanname='plperl';
select distinct oid, lanname from gp_dist_random('pg_language') where lanname='plperl';

