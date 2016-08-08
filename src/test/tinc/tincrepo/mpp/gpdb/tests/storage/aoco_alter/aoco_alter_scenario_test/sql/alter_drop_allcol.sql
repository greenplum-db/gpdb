-- 
-- @created 2014-05-20 12:00:00
-- @modified 2014-05-20 12:00:00
-- @tags storage
-- @description AOCO table : drop all column one by one then add new col

DROP TABLE IF EXISTS foo;

CREATE TABLE foo (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column);
CREATE INDEX foo_index ON foo(b);
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,500) AS i;
INSERT INTO foo SELECT i as a, i as b, 'hello world again' as c FROM generate_series(501,900) AS i;

select * from foo order by a,b;
ALTER TABLE foo DROP COLUMN c;
select count(*) as c from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='c';
select * from foo order by a,b;

ALTER TABLE foo DROP COLUMN b;
select count(*) as b from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='b';
select * from foo order by a;

ALTER TABLE foo DROP COLUMN a;
select count(*) as a from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='a';
select * from foo;

ALTER TABLE foo ADD COLUMN a1 int default 10;
select count(*) as a from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='a';
select * from foo;
vacuum foo;
select * from foo;
