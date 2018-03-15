-- start_ignore
--
-- Greenplum DB backup test
--

-- Check database version and pg_dump
select version();
\! pg_dump --version

-- Create target database
drop database if exists regressbkuptest1;
create database regressbkuptest1;

-- Populate some data
\c regressbkuptest1

-- Data type tests
-- table one, numeric types
create table t1(
		i int, si smallint, bi bigint, 
		de decimal, n numeric, n103 numeric(10, 3),
		r real, dou double precision
		);

insert into t1 values
	(1, 2, 3, 
	 3.14, 3.1415, 3.142,
	 2.71, 2.71828
	),
	(2, 2, 2,
	 2.0, 2.0, 2.00,
	 'Infinity', 'NaN'
	),
	(-3, -2, -1,
	 1.4, 1.41, 1.414,
	 'NaN', '-Infinity'
	),
	(NULL, NULL, NULL,
	 NULL, NULL, NULL,
	 NULL, NULL
	);

-- Data type test2 serial
create table t2 (i int, s serial, bs bigserial);

insert into t2 values (1), (2), (3);
-- it is OK to specify serial value if no unique/primary key
insert into t2 values (1, 1, 1), (4, 4, 4);
-- but sequence should continue work
insert into t2 values (4);

-- Data type test3, money (deprecated), and char, binary.
create table t3 (m money, c char(10), vc varchar(10), t text, ba bytea);
insert into t3 values 
	('$10.2', 'Simple', 'Simple VC', 'Text column', E'string as ba'),
	('$10', $$It's OK$$, $$It's OK$$, $$It's still OK$$, E'It\'s binary OK'),
	('$100', E'\t\r\n\v\\', E'\t\r\n\v\\', E'\t\r\n\v\\', E'\t\r\n\v'),
	('$100', '', '', '', ''),
	('$-3', '1234567890', '1234567890', 'Too long to test filling', E'\\001\\000\\000\\002\\\\'),
	(NULL, NULL, NULL, NULL, NULL);

-- Data type test 4, Date/Timestamp
create table t4(
		t timestamp, t2 timestamp(2), 
		tz timestamp with time zone, tz3 timestamp(3) with time zone, 
		i interval, d date,
		ti time, ti4 time(4),
		tiz time with time zone, tiz5 time(5) with time zone
		);
insert into t4 values
	(
 	'1999-01-08 04:05:06', '1999-01-08 04:05:06.23',
 	'1999-01-08 04:05:06+02', '1999-01-08 04:05:06.23PST',
	'1 12:34:56', '1999-01-08',
	'04:05:06', '00:00:00',
	'00:01:02-8', '00:01:00-2'
	),
	(
	 'epoch', 'infinity',
	 'infinity', '-infinity',
	 '0', 'epoch',
	 'allballs', 'allballs',
	 'allballs', 'allballs'
	),
	(NULL, NULL,
	 NULL, NULL,
	 NULL, NULL,
	 NULL, NULL,
	 NULL, NULL
	);

-- Data type test 5, boolean, geo,
create table t5(b bool, pt point, ls lseg, p path, bo box, po polygon, c circle);
insert into t5 values
	( true, '(1.0, 2.0)', 
	  '((1.0, 2.0), (0.0, 0.0))', '((1.0, 1.0), (2.0, 2.0), (3.0, 3.0))', 
	  '((1.0, 1.0), (2.0, 2.0))', '((1.0, 0.0), (1.0, 1.0), (2.0, 2.0))',
	  '((0.0, 1.0), 4)'
	),
	( false, '(1.0, 2.0)', 
	  '((1.0, 2.0), (0.0, 0.0))', '((1.0, 1.0), (2.0, 2.0), (3.0, 3.0), (4.0, 4.0))', 
	  '((1.0, 1.0), (2.0, 2.0))', '((1.0, 0.0), (1.0, 1.0), (2.0, 2.0))',
	  '((0.0, 2.0), 43)'
	),
	(NULL, NULL, NULL, NULL, NULL, NULL, NULL)
	;

-- Data type test 6, Network
create schema s6;
create table s6.t6(c cidr, i inet, m macaddr);
insert into s6.t6 values
	('127.0.0.1', '2001:4f8:3:ba::/64', '08:02:0b:02:ac:e3'),
	(NULL, NULL, NULL);

-- Data type test 7, Bit strings and arrays, 
create table t7 (b bit(3), bv bit varying(5), ia integer[], ta text[][]);
insert into t7 values
	(B'101', B'00101', '{1, 2, 3}', '{{"Hello", "world"},{"from", "Mars"}}'),
	(B'000', B'00', '{4, 5}', '{{"",""}, {"", ""}}'),
	(B'111', B'', '{}', '{{"1", "2"}, {"3", "4"}, {"5", "6"}}'),
	(NULL, NULL, NULL, NULL);

-- Data type test 8, Composite type
CREATE TYPE Complex As (r real, i real);
CREATE TYPE CompType As (name text, price numeric);
CREATE TABLE t8 (i int, c Complex, ct CompType);
insert into t8 values
(1, (1, 2), ('Hello', 8.3)),
(1, (0, 0), ('', NULL)),
(2, (NULL, -1), (NULL, NULL)),
(NULL, NULL, NULL);

-- Data type test 9, oid
create table t9 (i int) WITH OIDS;
create table t10 (j int) WITHOUT OIDS;
insert into t9 values (1), (2), (NULL);
insert into t10 values (3), (4), (NULL);

\c regression

-- end_ignore
