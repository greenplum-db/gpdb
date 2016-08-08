Create Language plpythonu;
DROP FUNCTION subt_fn2(int);
CREATE OR REPLACE FUNCTION subt_fn2(x int) returns int as $$
for i in range(0, x):
  plpy.execute('insert into subt_plpython_t2 values(%d)' % i);
return plpy.execute('select count(*) as a from subt_plpython_t2')[0]["a"]
$$ language plpythonu;

Drop table if exists subt_plpython_t2;
Create table  subt_plpython_t2(a int) distributed randomly;

BEGIN;
select subt_fn2(200);
ROLLBACK;
BEGIN;
select subt_fn2(2000);
Commit;
