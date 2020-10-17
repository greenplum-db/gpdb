create schema alter_drop_allcol_@amname@;
set search_path="$user",alter_drop_allcol_@amname@,public;
SET default_table_access_method=@amname@;
--
-- Drop all columns one by one then add new column.
-- Perform VACUUM and SELECT again.
--
BEGIN;
CREATE TABLE alter_drop_allcoll (a INT, b INT, c CHAR(128));
CREATE INDEX alter_drop_allcoll_index ON alter_drop_allcoll(b);
INSERT INTO alter_drop_allcoll SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,5) AS i;

select * from alter_drop_allcoll order by a,b;
ALTER TABLE alter_drop_allcoll DROP COLUMN c;
select count(*) as c from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='alter_drop_allcoll' and attname='c';
select * from alter_drop_allcoll order by a,b;

ALTER TABLE alter_drop_allcoll DROP COLUMN b;
select count(*) as b from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='alter_drop_allcoll' and attname='b';
select * from alter_drop_allcoll order by a;

ALTER TABLE alter_drop_allcoll DROP COLUMN a;
select count(*) as a from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='alter_drop_allcoll' and attname='a';
select * from alter_drop_allcoll;

ALTER TABLE alter_drop_allcoll ADD COLUMN a1 int default 10;
select count(*) as a from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='alter_drop_allcoll' and attname='a';
select * from alter_drop_allcoll;
COMMIT;
vacuum alter_drop_allcoll;
select * from alter_drop_allcoll;
