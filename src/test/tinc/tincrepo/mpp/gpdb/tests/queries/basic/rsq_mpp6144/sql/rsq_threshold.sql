
-- start_ignore
DROP TABLE IF EXISTS xyz CASCADE;
DROP ROLE IF EXISTS xyzuser;
-- end_ignore
CREATE ROLE xyzuser;
CREATE TABLE xyz (a int) distributed randomly;
GRANT ALL PRIVILEGES on xyz to xyzuser with grant option;
INSERT INTO xyz VALUES (1);
INSERT INTO xyz VALUES (2);
ALTER RESOURCE QUEUE pg_default ACTIVE THRESHOLD 2;
1: SET ROLE xyzuser;
2: SET ROLE xyzuser;
1: BEGIN WORK;
1: SELECT relname FROM pg_class where relname = 'xyz';
1: DECLARE c99 CURSOR FOR SELECT COUNT(*) FROM xyz;
1: FETCH c99;
1: ROLLBACK WORK;
4: SET ROLE xyzuser;
1: BEGIN WORK;
1: declare c0 cursor for select count(*) from xyz;
1: fetch c0;
1: declare c1 cursor for select count(*) from xyz;
1: fetch c1;
2>: select count(*) from xyz;
3: ALTER RESOURCE QUEUE pg_default ACTIVE THRESHOLD 4;
4: select count(*) from xyz;
2<:
3: ALTER RESOURCE QUEUE pg_default ACTIVE THRESHOLD 2;
1: DECLARE c4 CURSOR FOR SELECT COUNT(*) FROM xyz;
3: SELECT COUNT(*) FROM xyz;
1: ROLLBACK WORK;
2: ROLLBACK WORK;
3: ROLLBACK WORK;
4: ROLLBACK WORK;
3: ALTER RESOURCE QUEUE pg_default ACTIVE THRESHOLD 20;
3: DROP TABLE xyz;
3: DROP ROLE xyzuser;
3: COMMIT WORK;

