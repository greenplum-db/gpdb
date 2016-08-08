-- @Description Tests the AO segment file selection policy
-- 

1: INSERT INTO AO VALUES (1);
1: INSERT INTO AO VALUES (2);
1: BEGIN;
1: INSERT INTO AO VALUES (2);
2: BEGIN;
2: INSERT INTO AO VALUES (2);
1: COMMIT;
2: COMMIT;
1: insert into ao select generate_series(1,100000);
1: INSERT INTO AO VALUES (2);
