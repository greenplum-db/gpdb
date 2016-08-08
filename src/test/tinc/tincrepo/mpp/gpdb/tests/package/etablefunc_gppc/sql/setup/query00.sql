-- Create test tables which will be used for table function testing
DROP TABLE IF EXISTS t1 CASCADE;
CREATE TABLE t1 (a int, b int, c int, d int, e text)
DISTRIBUTED BY (a);

INSERT INTO t1 SELECT i, i/3, i%2, 100-i, 'text'||i 
FROM generate_series(1,100) i;

select count(*) from t1;

DROP FUNCTION IF EXISTS sessionize(anytable, interval);
DROP FUNCTION IF EXISTS ud_project(anytable, text);
DROP FUNCTION IF EXISTS ud_project(anytable);
DROP FUNCTION IF EXISTS ud_project2(anytable, text);
DROP FUNCTION IF EXISTS ud_project2(anytable);
DROP FUNCTION IF EXISTS ud_describe(internal);
DROP FUNCTION IF EXISTS ud_describe2(internal);

DROP TYPE IF EXISTS outcomp CASCADE;
DROP TABLE IF EXISTS outtable CASCADE;
DROP TABLE IF EXISTS intable CASCADE;
DROP TABLE IF EXISTS randtable;

CREATE TABLE intable(id int, value text) distributed by (id);
CREATE TABLE outtable(a text, b int) distributed randomly;
CREATE TABLE randtable(id int, value text) distributed randomly;

INSERT INTO intable   SELECT id::int, ('value_'||id)::text FROM generate_series(1, 10) id;
INSERT INTO randtable SELECT id::int, ('value_'||id)::text FROM generate_series(1, 10) id;

\d intable
\d outtable
\d randtable

select * from intable order by id,value;

DROP TABLE IF EXISTS history CASCADE;
CREATE TABLE history(id integer, time timestamp) DISTRIBUTED BY (id);
INSERT INTO history values 
(1,'2011/08/21 10:15:02am'),
(1,'2011/08/21 10:15:30am'),
(1,'2011/08/22 10:15:04am'),
(1,'2011/08/22 10:16:10am'),
(2,'2011/08/21 10:15:02am'),
(2,'2011/08/21 10:15:02am'),
(2,'2011/08/21 10:16:02am'),
(2,'2011/08/21 10:16:02am'),
(3,'2011-08-19 19:05:13'),
(3,'2011-08-19 19:06:50'),
(3,'2011-08-19 19:07:35'),
(3,'2011-08-19 19:08:18'),
(3,'2011-08-19 19:09:07'),
(3,'2011-08-20 10:07:10'),
(3,'2011-08-20 10:07:35'),
(3,'2011-08-20 10:11:29'),
(3,'2011-08-20 10:17:10'),
(3,'2011-08-20 10:17:42');

SELECT * FROM history order  by id, time;
