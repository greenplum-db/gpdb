DROP table if exists trigger_oid cascade;
DROP language if exists plpgsql;

CREATE language plpgsql;

CREATE table trigger_oid(a int, b int) distributed by (a);

CREATE OR REPLACE function trig_func() RETURNS trigger AS $$
  BEGIN
    RETURN new;
  END
$$ LANGUAGE plpgsql;

CREATE trigger troid_trigger AFTER INSERT ON trigger_oid
    FOR EACH ROW EXECUTE PROCEDURE trig_func();


select distinct oid, tgrelid::regclass, tgname 
from   pg_trigger 
where tgname='troid_trigger';


select distinct oid, tgrelid::regclass, tgname 
from   gp_dist_random('pg_trigger') 
where tgname='troid_trigger';

