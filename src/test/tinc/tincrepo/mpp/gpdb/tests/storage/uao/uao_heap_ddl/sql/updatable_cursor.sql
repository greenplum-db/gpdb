-- @Description Tests the updatable cursors on heap tables.
-- 

BEGIN;
declare c cursor for select * from ao where a = 9;
FETCH c;
update ao set b = 30 where current of c;
ROLLBACK;
