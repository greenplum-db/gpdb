-- setup
1: drop table if exists alter_block;
1: create table alter_block(a int, b int) distributed by (a);
1: insert into alter_block select 1, 1;
-- Validate UPDATE blocks the Alter
2: BEGIN;
2: UPDATE alter_block SET b = b + 1;
1&: ALTER TABLE alter_block SET DISTRIBUTED BY (b);
-- Alter process should be blocked
2: SELECT waiting FROM pg_stat_activity where query like 'ALTER TABLE alter_block %';
2: COMMIT;
1<:
-- Now validate ALTER blocks the UPDATE
2: BEGIN;
2: ALTER TABLE alter_block SET DISTRIBUTED BY (a);
1&: UPDATE alter_block SET b = b + 1;
2: SELECT waiting FROM pg_stat_activity where query like 'UPDATE alter_block SET %';
2: COMMIT;
1<:
