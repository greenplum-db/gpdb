drop language if exists plpythonu;
create language plpythonu;

select count(foo.*) 
from (
      select oid, lanname, lanplcallfoid, lanvalidator
      from pg_language
      where lanname='plpythonu'
      union
      select oid, lanname, lanplcallfoid, lanvalidator
      from gp_dist_random('pg_language')
      where lanname='plpythonu' ) foo;

