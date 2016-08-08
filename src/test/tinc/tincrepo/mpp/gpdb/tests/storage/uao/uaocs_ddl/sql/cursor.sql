-- @Description Tests basic cursor operations on updatable AO tables.
-- 

BEGIN;
DECLARE cur CURSOR FOR SELECT a FROM ao ORDER BY a;
FETCH NEXT IN cur;
FETCH NEXT IN cur;
FETCH NEXT IN cur;
FETCH NEXT IN cur;
FETCH NEXT IN cur;
CLOSE cur;
COMMIT;
