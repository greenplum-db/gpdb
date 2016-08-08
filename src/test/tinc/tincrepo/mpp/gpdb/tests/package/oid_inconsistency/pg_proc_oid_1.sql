DROP language if exists plperl cascade;

CREATE language plperl;

select distinct oid, proname from pg_proc where proname ~ 'plperl_call_handler';
select distinct oid, proname from gp_dist_random('pg_proc') where proname ~ 'plperl_call_handler';

