-- 
-- @created 2014-05-20 12:00:00
-- @modified 2014-05-20 12:00:00
-- @tags storage
-- @description AOCO table : drop all column one by one then add new col

DROP TABLE IF EXISTS foo;

CREATE TABLE foo (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column);
CREATE INDEX foo_index ON foo(b);
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,50) AS i;
update foo set b=b+10 where a < 3;
vacuum foo;
INSERT INTO foo SELECT i as a, i as b, 'hello world again' as c FROM generate_series(51,90) AS i;


select count(*) from foo ;
ALTER TABLE foo DROP COLUMN c;
select count(*) as c from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='c';
select count(*) from foo ;

ALTER TABLE foo DROP COLUMN b;
select count(*) as b from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='b';
select count(*) from foo ;

ALTER TABLE foo DROP COLUMN a;
select count(*) as a from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='a';
select count(*) from foo;

ALTER TABLE foo ADD COLUMN a1 int default 10;
select count(*) as a from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='foo' and attname='a';
select count(*) from foo;
vacuum foo;
select count(*) from foo;
