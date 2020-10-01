-- start_ignore
CREATE LANGUAGE plpython3u;
-- end_ignore
CREATE OR REPLACE FUNCTION pybusyloop() RETURNS double precision AS $$
import math
while True:
    a = 1
return 1
$$ LANGUAGE plpython3u;

CREATE OR REPLACE FUNCTION pysleep() RETURNS double precision AS $$
import time
time.sleep(100)
return 1
$$ LANGUAGE plpython3u;

CREATE OR REPLACE FUNCTION pyspisleep() RETURNS double precision AS $$
# container: plc_python_shared
rv = plpy.execute("select pg_sleep(100)")
return 1
$$ LANGUAGE plpython3u;

CREATE OR REPLACE FUNCTION pynestsleep() RETURNS double precision AS $$
# container: plc_python_shared
rv = plpy.execute("select pyspisleep()")
return 1
$$ LANGUAGE plpython3u;

CREATE OR REPLACE FUNCTION pynestsleep2() RETURNS double precision AS $$
# container: plc_python_shared
rv = plpy.execute("select pysleep()")
return 1
$$ LANGUAGE plpython3u;


CREATE TABLE a(i int);
insert into a values(1),(10),(20),(100);

1&: select pybusyloop();
2&: select pybusyloop() from a;
3&: select pysleep();
4&: select pysleep() from a;
5&: select pyspisleep();
6&: select pynestsleep();
7&: select pynestsleep2();


SELECT pg_cancel_backend(pid, 'test pg_cancel_backend') 
FROM pg_stat_activity WHERE query LIKE 'select pybusyloop()%' ORDER BY pid LIMIT 2;
SELECT pg_cancel_backend(pid, 'test pg_cancel_backend') 
FROM pg_stat_activity WHERE query LIKE 'select pysleep()%' ORDER BY pid LIMIT 2;
SELECT pg_cancel_backend(pid, 'test pg_cancel_backend')
FROM pg_stat_activity WHERE query LIKE 'select pyspisleep()%' ORDER BY pid LIMIT 1;
SELECT pg_cancel_backend(pid, 'test pg_cancel_backend')
FROM pg_stat_activity WHERE query LIKE 'select pynestsleep()%' ORDER BY pid LIMIT 1;
SELECT pg_cancel_backend(pid, 'test pg_cancel_backend')
FROM pg_stat_activity WHERE query LIKE 'select pynestsleep2()%' ORDER BY pid LIMIT 1;
-- start_ignore
-- start_ignore
-- start_ignore
-- start_ignore
1<:
1q:
2<:
2q:
3<:
3q:
4<:
4q:
5<:
5q:
6<:
6q:
7<:
7q:
-- end_ignore

