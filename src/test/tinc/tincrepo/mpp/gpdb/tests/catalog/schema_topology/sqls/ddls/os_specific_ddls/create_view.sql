-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Creation of different views


\c db_test_bed
CREATE OR REPLACE TEMP VIEW vista AS SELECT 'Hello World'; 
CREATE TEMPORARY VIEW vista1 AS SELECT text 'Hello World' AS hello;
CREATE TABLE test_emp_view (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);

CREATE VIEW emp_view AS SELECT ename ,eno ,salary ,ssn FROM test_emp_view WHERE salary >5000;

--Create the tables and views

CREATE TABLE sch_tbint (
    rnum integer NOT NULL,
    cbint bigint
) DISTRIBUTED BY (rnum);
COMMENT ON TABLE sch_tbint IS 'This describes table SCH_TBINT.';

CREATE TABLE sch_tchar (
    rnum integer NOT NULL,
    cchar character(32)
) DISTRIBUTED BY (rnum);
COMMENT ON TABLE sch_tchar IS 'This describes table SCH_TCHAR.';

CREATE TABLE sch_tclob (
    rnum integer NOT NULL,
    cclob text
) DISTRIBUTED BY (rnum);
COMMENT ON TABLE sch_tclob IS 'This describes table SCH_TCLOB.';

CREATE VIEW sch_vbint AS
    SELECT tbint.rnum, tbint.cbint FROM sch_tbint as tbint;
COMMENT ON VIEW sch_vbint IS 'This describes view SCH_VBINT.';

CREATE VIEW sch_vchar AS
    SELECT tchar.rnum, tchar.cchar FROM sch_tchar as tchar;
COMMENT ON VIEW sch_vchar IS 'This describes view SCH_VCHAR.';

CREATE VIEW sch_vclob AS
    SELECT tclob.rnum, tclob.cclob FROM sch_tclob as tclob;
COMMENT ON VIEW sch_vclob IS 'This describes view SCH_VCLOB.';


CREATE TABLE sch_tversion (
    rnum integer NOT NULL,
    c1 integer,
    cver character(6),
    cnnull integer,
    ccnull character(1)
) DISTRIBUTED BY (rnum);

COPY sch_tversion (rnum, c1, cver, cnnull, ccnull) FROM stdin;
0	1	1.0   	\N	\N
\.

CREATE TABLE sch_tjoin2 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);

COPY sch_tjoin2 (rnum, c1, c2) FROM stdin;
3	10	FF
0	10	BB
1	15	DD
2	\N	EE
\.

create view sch_view1001 as 
select 10 f1, 'BB' f2 from sch_tversion union
select sch_tjoin2.c1, sch_tjoin2.c2 from sch_tjoin2 where sch_tjoin2.c2 like '_B';


--Views with deep sliced queries

create table sch_T43(	C487 int,	C488 int,	C489 int,	C490 int,	C491 int,	C492 int,	C493 int,	C494 int,	C495 int,	C496 int,	C497 int,	C498 int,	C499 int,	C500 int,	C501 int) WITH (compresstype=quicklz,compresslevel=1,appendonly=true,blocksize=1327104,checksum=true);

COPY sch_T43 FROM STDIN delimiter '|';
5|3|4|1|3|2|1|2|5|2|4|3|2|5|1
4|1|2|5|4|5|3|5|4|3|4|2|5|3|1
5|3|3|1|5|3|2|1|5|5|3|5|1|4|4
3|2|1|4|3|3|5|1|1|3|3|5|4|5|4
2|3|4|4|5|3|3|3|3|4|2|1|5|3|4
3|5|5|3|4|1|4|4|4|5|2|3|1|3|1
5|5|3|4|3|1|2|2|5|2|4|2|1|5|3
2|5|2|3|1|1|2|4|5|4|2|1|3|5|1
2|4|3|3|1|1|3|1|5|4|5|1|2|5|1
3|3|3|4|1|1|2|3|2|5|4|2|5|1|3
\.


create table sch_T33(	C383 int,	C384 int,	C385 int,	C386 int,	C387 int,	C388 int,	C389 int,	C390 int,	C391 int,	C392 int,	C393 int,	C394 int,	C395 int,	C396 int,	C397 int,	C398 int,	C399 int,	C400 int,	C401 int) WITH (compresstype=zlib,blocksize=548864,appendonly=true,compresslevel=2,checksum=true);

INSERT INTO sch_T33 VALUES ( 5, 1, 3, 1, 5, 2, 4, 2, 2, 2, 4, 5, 3, 4, 1, 4, 2, 1, 3 );

INSERT INTO sch_T33 VALUES ( 2, 2, 1, 5, 2, 4, 3, 4, 5, 5, 2, 4, 2, 4, 2, 1, 2, 3, 5 );

INSERT INTO sch_T33 VALUES ( 1, 3, 2, 3, 5, 3, 2, 2, 5, 5, 5, 1, 4, 1, 5, 2, 4, 2, 4 );

INSERT INTO sch_T33 VALUES ( 1, 1, 4, 3, 1, 5, 1, 2, 1, 1, 3, 2, 4, 3, 5, 1, 1, 2, 2 );

INSERT INTO sch_T33 VALUES ( 1, 4, 1, 2, 5, 2, 5, 1, 4, 2, 3, 5, 3, 2, 3, 3, 2, 2, 4 );

INSERT INTO sch_T33 VALUES ( 4, 2, 5, 3, 4, 4, 3, 2, 1, 2, 1, 3, 3, 3, 5, 4, 2, 1, 4 );

INSERT INTO sch_T33 VALUES ( 2, 3, 1, 5, 2, 2, 3, 4, 2, 5, 2, 3, 4, 2, 4, 4, 5, 5, 3 );

INSERT INTO sch_T33 VALUES ( 3, 1, 2, 2, 2, 5, 3, 3, 3, 5, 3, 2, 2, 4, 3, 5, 3, 4, 1 );

INSERT INTO sch_T33 VALUES ( 2, 5, 5, 3, 1, 2, 4, 3, 3, 4, 1, 4, 3, 2, 5, 3, 2, 1, 1 );

INSERT INTO sch_T33 VALUES ( 1, 2, 5, 1, 2, 2, 4, 5, 2, 1, 2, 2, 3, 5, 3, 5, 5, 1, 2 );


create table sch_T29(	C334 int,	C335 int,	C336 int,	C337 int,	C338 int,	C339 int,	C340 int) WITH (compresstype=quicklz,checksum=true,appendonly=true,blocksize=1155072,compresslevel=1);

INSERT INTO sch_T29 VALUES ( 5, 1, 1, 3, 1, 2, 4 );

INSERT INTO sch_T29 VALUES ( 5, 5, 4, 3, 3, 3, 2 );

INSERT INTO sch_T29 VALUES ( 5, 1, 3, 2, 1, 5, 4 );

INSERT INTO sch_T29 VALUES ( 4, 1, 4, 4, 3, 2, 3 );

INSERT INTO sch_T29 VALUES ( 3, 4, 1, 5, 3, 3, 3 );

INSERT INTO sch_T29 VALUES ( 2, 5, 4, 3, 5, 3, 4 );

INSERT INTO sch_T29 VALUES ( 4, 5, 1, 1, 1, 3, 2 );

INSERT INTO sch_T29 VALUES ( 4, 5, 2, 1, 5, 3, 4 );

INSERT INTO sch_T29 VALUES ( 4, 5, 2, 1, 5, 3, 1 );

INSERT INTO sch_T29 VALUES ( 1, 2, 2, 2, 4, 3, 3 );



CREATE VIEW sch_view_ao (col1,col2,col3,col4,col5,col6) AS
SELECT
	DT102.C339
	, DT102.C340
	, DT100.C498
	, SUM( DT102.C339 )
	, SUM( DT102.C340 )
	, COUNT( DT100.C498 )
FROM
	(
		(
			sch_T43 DT100
		INNER JOIN
			sch_T33 DT101
		ON
			DT100.C498 = DT101.C401
		)
	INNER JOIN
		sch_T29 DT102
	ON
		DT101.C385 = DT102.C338
	)
WHERE
	(
		(
			DT101.C390 = DT100.C500
		)
		OR
		(
			(
				DT102.C338 <> DT101.C388
			)
			AND
			(
				DT102.C336 < DT100.C494
			)
		)
	)
	OR
	(
		(
			DT101.C391 < DT101.C399
		)
		AND
		(
			DT102.C335 = DT100.C490
		)
	)
GROUP BY
	DT100.C498
	, DT102.C339
	, DT102.C340
ORDER BY
	DT102.C339
	, DT102.C340
	, DT100.C498
	, SUM( DT102.C339 )
	, SUM( DT102.C340 )
	, COUNT( DT100.C498 )
LIMIT 497;

SELECT count(*) FROM sch_view_ao;

--Create view from set_returning_functions
create view sch_srf_view1 as select * from generate_series(1,10);

Select count(*) from sch_srf_view1;

--Create view from functions

CREATE FUNCTION sch_multiply(value integer,times integer) RETURNS integer AS $$
DECLARE
    res integer;
BEGIN
    RETURN $1*$2;
END;
 
$$ LANGUAGE plpgsql;

create view sch_fn_view2 as select  sch_multiply(4,5);

Select * from sch_fn_view2;
