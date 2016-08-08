-- SPI test
DROP FUNCTION IF EXISTS spifunc1(text, int);
DROP FUNCTION IF EXISTS spifunc2(text, text);
DROP FUNCTION IF EXISTS spifunc3(text, int);
DROP FUNCTION IF EXISTS spifunc4(text, text);
DROP FUNCTION IF EXISTS spifunc5(text, int, int);
DROP FUNCTION IF EXISTS spifunc5a(text, int, int);
DROP FUNCTION IF EXISTS spifunc6(text, int, int);

CREATE OR REPLACE FUNCTION spifunc1(text, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc2(text, text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc3(text, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc4(text, text) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc5(text, int, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc5a(text, int, int) RETURNS int AS '$libdir/gppc_test' LANGUAGE c STRICT;
CREATE OR REPLACE FUNCTION spifunc6(text, int, int) RETURNS text AS '$libdir/gppc_test' LANGUAGE c STRICT;

SELECT spifunc1($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2);
SELECT spifunc2($$select i, i * 2 as val from generate_series(1, 10)i order by 1$$, 'val');
SELECT spifunc3($$select i, 'foo' || i as val from generate_series(1, 10)i order by 1$$, 2);
SELECT spifunc4($$select i, 'foo' || i as val from generate_series(1, 10)i order by 1$$, 'val');

-- multiple queries in one query string
SELECT spifunc1($$select i, i * 2 from generate_series(1, 10)i order by 1; select 1+1; select 1+2$$, 1);

-- access table
DROP TABLE IF EXISTS spi_test CASCADE;
CREATE TABLE spi_test (a int, b text);
INSERT INTO spi_test (SELECT a, 'foo'||a FROM generate_series(1, 10) a);
SELECT spifunc1($$select * from spi_test order by a limit 5$$, 2);

-- access view
DROP VIEW IF EXISTS spi_view;
CREATE VIEW spi_view AS 
select * from spi_test order by a limit 5;
SELECT spifunc1($$select * from spi_view$$, 2);

-- join table and view
SELECT spifunc1($$select * from spi_test, spi_view where spi_test.a = spi_view.a order by spi_test.a$$, 2);

-- using sub-query
SELECT spifunc1($$select * from spi_test where spi_test.a in (select a from spi_view) order by spi_test.a$$, 2);

-- recursive SPI function call
SELECT spifunc1($$select (
	SELECT spifunc1('select * from spi_test, spi_view where spi_test.a = spi_view.a order by spi_test.a', 2)a
)$$, 1);

-- DDL: create table
SELECT spifunc1($$create table spi_test2 (a int, b text)$$, 1);
\d spi_test2

-- DDL: alter table
SELECT spifunc1($$alter table spi_test2 add column c int$$, 1);
\d spi_test2

-- DDL: drop table
SELECT spifunc1($$drop table spi_test2$$, 1);
\dt spi_test2

-- When tcount = 0, no limit
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, 0);
-- When tcount = 5, which is less than total 10 records
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, 5);
-- When tcount = 20, which is greater than total 10 records
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, 20);
-- When tcount = -1, returns null
SELECT spifunc5($$SELECT * FROM spi_test ORDER BY 1$$, 1, -1);

-- insert a record with 400K bytes
---- insert into spi_test values(31, repeat('test',100000));
-- SPI GppcSPIGetValue makecopy = true for long text 400K bytes
---- SELECT spifunc1($$select * from spi_test order by a $$, 2);

-- SPI GppcSPIGetValue makecopy = false
SELECT spifunc5a($$SELECT * FROM spi_test ORDER BY 1$$, 2, 10);
SELECT spifunc5a($$SELECT * FROM spi_test ORDER BY 1$$, 2, 5);
SELECT spifunc5a($$SELECT * FROM spi_test ORDER BY 1$$, 2, 0);

-- SPI GppcSPIExec select into table
DROP TABLE IF EXISTS spi_test3;
SELECT spifunc6($$select i, 'foo'||i into spi_test3 from generate_series(10,15) i$$,0,0);
SELECT * FROM spi_test3 ORDER BY 1;

-- SPI GppcSPIExec CTAS
DROP TABLE IF EXISTS spi_test4;
SELECT spifunc6($$create table spi_test4 as select i, 'foo'||i from generate_series(1,10) i$$,0,0);
SELECT * FROM spi_test4 ORDER BY 1;

-- SPI truncate
SELECT spifunc1($$truncate spi_test$$, 1);
-- After truncate
SELECT * FROM spi_test order by a;

-- DML: insert
SELECT spifunc6($$insert into spi_test select i, 'foo'||i from generate_series(1, 5) i$$, 0, 0);
SELECT spifunc6($$insert into spi_test values (6, 'foo6')$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: update
SELECT spifunc6($$update spi_test set b = 'boo' ||a$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: delete
SELECT spifunc6($$delete from spi_test where a > 5$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: insert using tcount=3, notice tcount takes no effect
SELECT spifunc6($$insert into spi_test select i, 'foo'||i from generate_series(6, 10) i$$, 3, 0);
SELECT * from spi_test order by a;

-- DML: update using tcount=3, notice tcount takes no effect
SELECT spifunc6($$update spi_test set b = 'boo' ||a$$, 0, 0);
SELECT * from spi_test order by a;

-- DML: delete using tcounti=3, notice tcount takes no effect
SELECT spifunc6($$delete from spi_test where a > 5$$, 3, 0);
SELECT * from spi_test order by a;

-- DML: create, alter, and drop index
SELECT spifunc6($$CREATE INDEX spi_idx1 ON spi_test (a, b)$$, 0, 0);
\d spi_idx1
SELECT spifunc6($$ALTER INDEX spi_idx1 RENAME TO spi_idx2$$, 0, 0);
\d spi_idx2
SELECT spifunc6($$DROP INDEX spi_idx2$$, 0 , 0);
\d spi_idx2

-- Negative: connect again when already connected
SELECT spifunc6($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2, 1);
-- Negative: try to execute without connection
SELECT spifunc6($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2, 2);
-- Negative: close connection again when it has been closed already
SELECT spifunc6($$select i, i * 2 from generate_series(1, 10)i order by 1$$, 2, 3);
