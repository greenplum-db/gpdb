-- @Description Ensures that an insert during a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 128;
4: BEGIN;
4: SELECT COUNT(*) FROM ao;
5: BEGIN;
4: SELECT COUNT(*) FROM ao;
4: BEGIN;
4: SELECT COUNT(*) FROM ao;
2>: VACUUM ao;
4: SELECT COUNT(*) FROM ao;SELECT COUNT(*) FROM ao;BEGIN;insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);COMMIT;
2<:
3: SELECT COUNT(*) FROM ao WHERE a = 1500;
3: INSERT INTO ao VALUES (0);
