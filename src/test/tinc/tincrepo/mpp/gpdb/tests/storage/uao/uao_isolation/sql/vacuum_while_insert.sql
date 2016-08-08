-- @Description Ensures that a vacuum during insert operations is ok
-- 

DELETE FROM ao WHERE a < 128;
1: BEGIN;
1>: insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);COMMIT;
4: BEGIN;
4>: insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);insert into ao select generate_series(1001,2000);COMMIT;
2: VACUUM ao;
1<:
4<:
3: SELECT COUNT(*) FROM ao WHERE a = 1500;
3: INSERT INTO ao VALUES (0);
