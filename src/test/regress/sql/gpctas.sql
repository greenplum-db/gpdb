set optimizer_print_missing_stats = off;
drop table if exists ctas_src;
drop table if exists ctas_dst;

create table ctas_src (domain integer, class integer, attr text, value integer) distributed by (domain);
insert into ctas_src values(1, 1, 'A', 1);
insert into ctas_src values(2, 1, 'A', 0);
insert into ctas_src values(3, 0, 'B', 1);

-- MPP-2859
create table ctas_dst as 
SELECT attr, class, (select count(distinct class) from ctas_src) as dclass FROM ctas_src GROUP BY attr, class distributed by (attr);

drop table ctas_dst;

create table ctas_dst as 
SELECT attr, class, (select max(class) from ctas_src) as maxclass FROM ctas_src GROUP BY attr, class distributed by (attr);

drop table ctas_dst;

create table ctas_dst as 
SELECT attr, class, (select count(distinct class) from ctas_src) as dclass, (select max(class) from ctas_src) as maxclass, (select min(class) from ctas_src) as minclass FROM ctas_src GROUP BY attr, class distributed by (attr);

-- MPP-4298: "unknown" datatypes.
drop table if exists ctas_foo;
drop table if exists ctas_bar;
drop table if exists ctas_baz;

create table ctas_foo as select * from generate_series(1, 100);
create table ctas_bar as select a.generate_series as a, b.generate_series as b from ctas_foo a, ctas_foo b;

create table ctas_baz as select 'delete me' as action, * from ctas_bar distributed by (a);
-- "action" has no type.
\d ctas_baz
select action, b from ctas_baz order by 1,2 limit 5;
select action, b from ctas_baz order by 2 limit 5;
select action::text, b from ctas_baz order by 1,2 limit 5;

alter table ctas_baz alter column action type text;
\d ctas_baz
select action, b from ctas_baz order by 1,2 limit 5;
select action, b from ctas_baz order by 2 limit 5;
select action::text, b from ctas_baz order by 1,2 limit 5;

-- Test CTAS with a function that executes another query that's dispatched.
-- Once upon a time, we had a bug in dispatching the table's OID in this
-- scenario.
create table ctas_input(x int);
insert into ctas_input select * from generate_series(1, 10);

CREATE FUNCTION ctas_inputArray() RETURNS INT[] AS $$
DECLARE theArray INT[];
BEGIN
   SELECT array(SELECT * FROM ctas_input ORDER BY x) INTO theArray;
   RETURN theArray;
--EXCEPTION WHEN OTHERS THEN RAISE NOTICE 'Catching the exception ...%', SQLERRM;
END;
$$ LANGUAGE plpgsql;

create table ctas_output as select ctas_inputArray()::int[] as x;


-- Test CTAS with VALUES.

CREATE TEMP TABLE yolo(i, j, k) AS (VALUES (0,0,0), (1, NULL, 0), (2, NULL, 0), (3, NULL, 0)) DISTRIBUTED BY (i);


--
-- Test that the rows are distributed correctly in CTAS, even if the query
-- has an ORDER BY. This used to tickle a bug at one point.
--

DROP TABLE IF EXISTS ctas_src, ctas_dst;

CREATE TABLE ctas_src(
col1 int4,
col2 decimal,
col3 char,
col4 boolean,
col5 int
) DISTRIBUTED by(col4);

-- I'm not sure why, but dropping a column was crucial to tickling the
-- original bug.
ALTER TABLE ctas_src DROP COLUMN col2;
INSERT INTO ctas_src(col1, col3,col4,col5)
    SELECT g, 'a',True,g from generate_series(1,5) g;

CREATE TABLE ctas_dst as SELECT col1,col3,col4,col5 FROM ctas_src order by 1;

-- This will fail to find some of the rows, if they're distributed incorrectly.
SELECT * FROM ctas_src, ctas_dst WHERE ctas_src.col1 = ctas_dst.col1
