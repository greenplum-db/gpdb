-- start_ignore
create schema qp_subquery;
CREATE TABLE qp_subquery.SUBSELECT_TBL1 (
  							f1 integer,
							f2 integer,
  							f3 float
						);
		
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (1, 2, 3); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (2, 3, 4); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (3, 4, 5); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (1, 1, 1); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (2, 2, 2); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (3, 3, 3); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (6, 7, 8); 
INSERT INTO qp_subquery.SUBSELECT_TBL1 VALUES (8, 9, NULL); 
-- end_ignore

SELECT '' AS eight, * FROM qp_subquery.SUBSELECT_TBL1 ORDER BY 2,3,4;
                        

SELECT '' AS two, f1 AS "Constant Select" FROM qp_subquery.SUBSELECT_TBL1
 					 WHERE f1 IN (SELECT 1) ORDER BY 2;
                        

-- order 2
SELECT '' AS six, f1 AS "Uncorrelated Field" FROM qp_subquery.SUBSELECT_TBL1
					  WHERE f1 IN (SELECT f2 FROM qp_subquery.SUBSELECT_TBL1) ORDER BY 2;
                        

-- order 2
SELECT '' AS six, f1 AS "Uncorrelated Field" FROM qp_subquery.SUBSELECT_TBL1
				 WHERE f1 IN (SELECT f2 FROM qp_subquery.SUBSELECT_TBL1 WHERE
				   f2 IN (SELECT f1 FROM qp_subquery.SUBSELECT_TBL1)) ORDER BY 2;
                        

-- order 2,3
SELECT '' AS three, f1, f2
  				FROM qp_subquery.SUBSELECT_TBL1
  				WHERE (f1, f2) NOT IN (SELECT f2, CAST(f3 AS int4) FROM qp_subquery.SUBSELECT_TBL1
                         	WHERE f3 IS NOT NULL) ORDER BY 2,3;
                        

SELECT 1 AS one WHERE 1 IN (SELECT 1);
                        

SELECT 1 AS zero WHERE 1 IN (SELECT 2);
			 

SELECT 1 AS zero WHERE 1 NOT IN (SELECT 1);
                         

SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
                                FROM qp_subquery.SUBSELECT_TBL1 upper
                                WHERE f1 IN (SELECT f2 FROM qp_subquery.SUBSELECT_TBL1 WHERE f1 = upper.f1);
                         

SELECT '' AS six, f1 AS "Correlated Field", f3 AS "Second Field"
                                FROM qp_subquery.SUBSELECT_TBL1 upper
                                WHERE f1 IN
                                (SELECT f2 FROM qp_subquery.SUBSELECT_TBL1 WHERE CAST(upper.f2 AS float) = f3);

                         

SELECT '' AS six, f1 AS "Correlated Field", f3 AS "Second Field"
                                FROM qp_subquery.SUBSELECT_TBL1 upper
                                WHERE f3 IN (SELECT upper.f1 + f2 FROM qp_subquery.SUBSELECT_TBL1
                                WHERE f2 = CAST(f3 AS integer));
                         

SELECT '' AS five, f1 AS "Correlated Field"
                                FROM qp_subquery.SUBSELECT_TBL1
                                WHERE (f1, f2) IN (SELECT f2, CAST(f3 AS int4) FROM qp_subquery.SUBSELECT_TBL1
                                WHERE f3 IS NOT NULL);
                         

-- start_ignore
create table qp_subquery.join_tab1 ( i integer, j integer, t text);
INSERT INTO qp_subquery.join_tab1 VALUES (1, 4, 'one');
INSERT INTO qp_subquery.join_tab1 VALUES (2, 3, 'two');
INSERT INTO qp_subquery.join_tab1 VALUES (3, 2, 'three');
INSERT INTO qp_subquery.join_tab1 VALUES (4, 1, 'four');
INSERT INTO qp_subquery.join_tab1 VALUES (5, 0, 'five');
INSERT INTO qp_subquery.join_tab1 VALUES (6, 6, 'six');
INSERT INTO qp_subquery.join_tab1  VALUES (7, 7, 'seven');
INSERT INTO qp_subquery.join_tab1 VALUES (8, 8, 'eight');
INSERT INTO qp_subquery.join_tab1 VALUES (0, NULL, 'zero');
INSERT INTO qp_subquery.join_tab1 VALUES (NULL, NULL, 'null');
INSERT INTO qp_subquery.join_tab1 VALUES (NULL, 0, 'zero');
-- end_ignore

select * from qp_subquery.join_tab1 order by i, t;				
                         

-- start_ignore
create table qp_subquery.join_tab2 ( i integer, k integer);
INSERT INTO qp_subquery.join_tab2 VALUES (1, -1);
INSERT INTO qp_subquery.join_tab2 VALUES (2, 2);
INSERT INTO qp_subquery.join_tab2 VALUES (3, -3);
INSERT INTO qp_subquery.join_tab2 VALUES (2, 4);
INSERT INTO qp_subquery.join_tab2 VALUES (5, -5);
INSERT INTO qp_subquery.join_tab2 VALUES (5, -5);
INSERT INTO qp_subquery.join_tab2 VALUES (0, NULL);
INSERT INTO qp_subquery.join_tab2 VALUES (NULL, NULL);
INSERT INTO qp_subquery.join_tab2 VALUES (NULL, 0);
-- end_ignore
select * from qp_subquery.join_tab2; 
                         
select * from ( SELECT '' AS "col", * FROM qp_subquery.join_tab1 AS tx)A;
                         

select * from ( SELECT '' AS "col", * FROM qp_subquery.join_tab1 AS tx) AS A;
                         

select * from(SELECT '' AS "col", * FROM qp_subquery.join_tab1 AS tx) as A(a,b,c);
                         

select * from(SELECT '' AS "col", t1.a, t2.e FROM qp_subquery.join_tab1 t1 (a, b, c), qp_subquery.join_tab2 t2 (d, e) 
				WHERE t1.a = t2.d)as A;

                         

select * from qp_subquery.join_tab1 where exists(select * from qp_subquery.join_tab2 where qp_subquery.join_tab1.i=qp_subquery.join_tab2.i);
                         

select * from qp_subquery.join_tab1 where not exists(select * from qp_subquery.join_tab2 where qp_subquery.join_tab1.i=qp_subquery.join_tab2.i) order by i,j;
                         

select 25 = any ('{1,2,3,4}');
                         

select 25 = any ('{1,2,25}');
                         

select 'abc' = any('{abc,d,e}');
                         

-- start_ignore
create table qp_subquery.subq_abc(a int);
insert into qp_subquery.subq_abc values(1);
insert into qp_subquery.subq_abc values(9);
insert into qp_subquery.subq_abc values(3);
insert into qp_subquery.subq_abc values(6);
-- end_ignore
select * from qp_subquery.subq_abc;
                         

SELECT 9 = any (select * from qp_subquery.subq_abc);
                         

select null::int >= any ('{}');
                         

select 'abc' = any('{" "}');
                         

select 33.4 = any (array[1,2,3]);
                         

select 40 = all ('{3,4,40,10}');
                         

select 55 >= all ('{1,2,55}');
			 

select 25 = all ('{25,25,25}');
		          

select 'abc' = all('{abc}');
                         

select 'abc' = all('{abc,d,e}');
                         

select 'abc' = all('{"abc"}');
                         

select 'abc' = all('{" "}');
                         

select null::int >= all ('{1,2,33}');
                         

select null::int >= all ('{}');
                         

select 33.4 > all (array[1,2,3]);                       
                         

-- start_ignore
create table qp_subquery.emp_list(empid int,name char(20),sal float); 
insert into qp_subquery.emp_list values(1,'empone',1000); 
insert into qp_subquery.emp_list values(2,'emptwo',2000); 
insert into qp_subquery.emp_list values(3,'empthree',3000); 
insert into qp_subquery.emp_list values(4,'empfour',4000); 
insert into qp_subquery.emp_list values(5,'empfive',4000); 
-- end_ignore
select * from qp_subquery.emp_list;
                       

select name from qp_subquery.emp_list where sal=(select max(sal) from qp_subquery.emp_list);
                        

select name from qp_subquery.emp_list where sal=(select min(sal) from qp_subquery.emp_list);
                       

select name from qp_subquery.emp_list where sal>(select avg(sal) from qp_subquery.emp_list);
                       

select name from qp_subquery.emp_list where sal<(select avg(sal) from qp_subquery.emp_list);
                      

CREATE TABLE qp_subquery.subq_test1 (s1 INT, s2 CHAR(5), s3 FLOAT);
INSERT INTO qp_subquery.subq_test1 VALUES (1,'1',1.0); 
INSERT INTO qp_subquery.subq_test1 VALUES (2,'2',2.0);
INSERT INTO qp_subquery.subq_test1 VALUES (3,'3',3.0);
INSERT INTO qp_subquery.subq_test1 VALUES (4,'4',4.0);
SELECT sb1,sb2,sb3 FROM (SELECT s1 AS sb1, s2 AS sb2, s3*2 AS sb3 FROM qp_subquery.subq_test1) AS sb WHERE sb1 > 1;

                      

select to_char(Avg(sum_col1),'9999999.9999999') from (select sum(s1) as sum_col1 from qp_subquery.subq_test1 group by s1) as tab1;
                      

select g2,count(*) from (select I, count(*) as g2 from qp_subquery.join_tab1 group by I) as vtable group by g2;
                      

-- start_ignore
create table qp_subquery.join_tab4 ( i integer, j integer, t text);
insert into qp_subquery.join_tab4 values (1,7,'sunday'); 
insert into qp_subquery.join_tab4 values (2,6,'monday');
insert into qp_subquery.join_tab4 values (3,5,'tueday');
insert into qp_subquery.join_tab4 values (4,4,'wedday');
insert into qp_subquery.join_tab4 values (5,3,'thuday');
insert into qp_subquery.join_tab4 values (6,2,'friday');
insert into qp_subquery.join_tab4 values (7,1,'satday');
-- end_ignore
select * from qp_subquery.join_tab4;
                      

select i,j,t from (select * from (select i,j,t from qp_subquery.join_tab1)as dtab1 
				UNION select * from(select i,j,t from qp_subquery.join_tab4) as dtab2 )as mtab; 	
                      

select * from qp_subquery.join_tab1 where i = (select i from qp_subquery.join_tab4 where t='satday');
                      

select * from qp_subquery.join_tab1 where i = (select i from qp_subquery.join_tab4);
                      
-- start_ignore
drop schema qp_subquery cascade;
-- end_ignore

