-- start_ignore
create schema qp_subquery;
set search_path to qp_subquery;
CREATE TABLE SUBSELECT_TBL1 (
  							f1 integer,
							f2 integer,
  							f3 float
						);
		
INSERT INTO SUBSELECT_TBL1 VALUES (1, 2, 3); 
INSERT INTO SUBSELECT_TBL1 VALUES (2, 3, 4); 
INSERT INTO SUBSELECT_TBL1 VALUES (3, 4, 5); 
INSERT INTO SUBSELECT_TBL1 VALUES (1, 1, 1); 
INSERT INTO SUBSELECT_TBL1 VALUES (2, 2, 2); 
INSERT INTO SUBSELECT_TBL1 VALUES (3, 3, 3); 
INSERT INTO SUBSELECT_TBL1 VALUES (6, 7, 8); 
INSERT INTO SUBSELECT_TBL1 VALUES (8, 9, NULL); 
-- end_ignore

SELECT '' AS eight, * FROM SUBSELECT_TBL1 ORDER BY 2,3,4;
                        

SELECT '' AS two, f1 AS "Constant Select" FROM SUBSELECT_TBL1
 					 WHERE f1 IN (SELECT 1) ORDER BY 2;
                        

-- order 2
SELECT '' AS six, f1 AS "Uncorrelated Field" FROM SUBSELECT_TBL1
					  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL1) ORDER BY 2;
                        

-- order 2
SELECT '' AS six, f1 AS "Uncorrelated Field" FROM SUBSELECT_TBL1
				 WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL1 WHERE
				   f2 IN (SELECT f1 FROM SUBSELECT_TBL1)) ORDER BY 2;
                        

-- order 2,3
SELECT '' AS three, f1, f2
  				FROM SUBSELECT_TBL1
  				WHERE (f1, f2) NOT IN (SELECT f2, CAST(f3 AS int4) FROM SUBSELECT_TBL1
                         	WHERE f3 IS NOT NULL) ORDER BY 2,3;
                        

SELECT 1 AS one WHERE 1 IN (SELECT 1);
                        

SELECT 1 AS zero WHERE 1 IN (SELECT 2);
			 

SELECT 1 AS zero WHERE 1 NOT IN (SELECT 1);
                         

SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
                                FROM SUBSELECT_TBL1 upper
                                WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL1 WHERE f1 = upper.f1);
                         

SELECT '' AS six, f1 AS "Correlated Field", f3 AS "Second Field"
                                FROM SUBSELECT_TBL1 upper
                                WHERE f1 IN
                                (SELECT f2 FROM SUBSELECT_TBL1 WHERE CAST(upper.f2 AS float) = f3);

                         

SELECT '' AS six, f1 AS "Correlated Field", f3 AS "Second Field"
                                FROM SUBSELECT_TBL1 upper
                                WHERE f3 IN (SELECT upper.f1 + f2 FROM SUBSELECT_TBL1
                                WHERE f2 = CAST(f3 AS integer));
                         

SELECT '' AS five, f1 AS "Correlated Field"
                                FROM SUBSELECT_TBL1
                                WHERE (f1, f2) IN (SELECT f2, CAST(f3 AS int4) FROM SUBSELECT_TBL1
                                WHERE f3 IS NOT NULL);
                         

-- start_ignore
create table join_tab1 ( i integer, j integer, t text);
INSERT INTO join_tab1 VALUES (1, 4, 'one');
INSERT INTO join_tab1 VALUES (2, 3, 'two');
INSERT INTO join_tab1 VALUES (3, 2, 'three');
INSERT INTO join_tab1 VALUES (4, 1, 'four');
INSERT INTO join_tab1 VALUES (5, 0, 'five');
INSERT INTO join_tab1 VALUES (6, 6, 'six');
INSERT INTO join_tab1  VALUES (7, 7, 'seven');
INSERT INTO join_tab1 VALUES (8, 8, 'eight');
INSERT INTO join_tab1 VALUES (0, NULL, 'zero');
INSERT INTO join_tab1 VALUES (NULL, NULL, 'null');
INSERT INTO join_tab1 VALUES (NULL, 0, 'zero');
-- end_ignore

select * from join_tab1 order by i, t;				
                         

-- start_ignore
create table join_tab2 ( i integer, k integer);
INSERT INTO join_tab2 VALUES (1, -1);
INSERT INTO join_tab2 VALUES (2, 2);
INSERT INTO join_tab2 VALUES (3, -3);
INSERT INTO join_tab2 VALUES (2, 4);
INSERT INTO join_tab2 VALUES (5, -5);
INSERT INTO join_tab2 VALUES (5, -5);
INSERT INTO join_tab2 VALUES (0, NULL);
INSERT INTO join_tab2 VALUES (NULL, NULL);
INSERT INTO join_tab2 VALUES (NULL, 0);
-- end_ignore
select * from join_tab2; 
                         
select * from ( SELECT '' AS "col", * FROM join_tab1 AS tx)A;
                         

select * from ( SELECT '' AS "col", * FROM join_tab1 AS tx) AS A;
                         

select * from(SELECT '' AS "col", * FROM join_tab1 AS tx) as A(a,b,c);
                         

select * from(SELECT '' AS "col", t1.a, t2.e FROM join_tab1 t1 (a, b, c), join_tab2 t2 (d, e) 
				WHERE t1.a = t2.d)as A;

                         

select * from join_tab1 where exists(select * from join_tab2 where join_tab1.i=join_tab2.i);
                         

select * from join_tab1 where not exists(select * from join_tab2 where join_tab1.i=join_tab2.i) order by i,j;
                         

select 25 = any ('{1,2,3,4}');
                         

select 25 = any ('{1,2,25}');
                         

select 'abc' = any('{abc,d,e}');
                         

-- start_ignore
create table subq_abc(a int);
insert into subq_abc values(1);
insert into subq_abc values(9);
insert into subq_abc values(3);
insert into subq_abc values(6);
-- end_ignore
select * from subq_abc;
                         

SELECT 9 = any (select * from subq_abc);
                         

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
create table emp_list(empid int,name char(20),sal float); 
insert into emp_list values(1,'empone',1000); 
insert into emp_list values(2,'emptwo',2000); 
insert into emp_list values(3,'empthree',3000); 
insert into emp_list values(4,'empfour',4000); 
insert into emp_list values(5,'empfive',4000); 
-- end_ignore
select * from emp_list;
                       

select name from emp_list where sal=(select max(sal) from emp_list);
                        

select name from emp_list where sal=(select min(sal) from emp_list);
                       

select name from emp_list where sal>(select avg(sal) from emp_list);
                       

select name from emp_list where sal<(select avg(sal) from emp_list);
                      

CREATE TABLE subq_test1 (s1 INT, s2 CHAR(5), s3 FLOAT);
INSERT INTO subq_test1 VALUES (1,'1',1.0); 
INSERT INTO subq_test1 VALUES (2,'2',2.0);
INSERT INTO subq_test1 VALUES (3,'3',3.0);
INSERT INTO subq_test1 VALUES (4,'4',4.0);
SELECT sb1,sb2,sb3 FROM (SELECT s1 AS sb1, s2 AS sb2, s3*2 AS sb3 FROM subq_test1) AS sb WHERE sb1 > 1;

                      

select to_char(Avg(sum_col1),'9999999.9999999') from (select sum(s1) as sum_col1 from subq_test1 group by s1) as tab1;
                      

select g2,count(*) from (select I, count(*) as g2 from join_tab1 group by I) as vtable group by g2;
                      

-- start_ignore
create table join_tab4 ( i integer, j integer, t text);
insert into join_tab4 values (1,7,'sunday'); 
insert into join_tab4 values (2,6,'monday');
insert into join_tab4 values (3,5,'tueday');
insert into join_tab4 values (4,4,'wedday');
insert into join_tab4 values (5,3,'thuday');
insert into join_tab4 values (6,2,'friday');
insert into join_tab4 values (7,1,'satday');
-- end_ignore
select * from join_tab4;
                      

select i,j,t from (select * from (select i,j,t from join_tab1)as dtab1 
				UNION select * from(select i,j,t from join_tab4) as dtab2 )as mtab; 	
                      

select * from join_tab1 where i = (select i from join_tab4 where t='satday');
                      

select * from join_tab1 where i = (select i from join_tab4);
         
--
-- Testing NOT-IN Subquery
--              

-- start_ignore
create table Tbl8352_t1(a int, b int) distributed by (a);
create table Tbl8352_t2(a int, b int) distributed by (a);
insert into Tbl8352_t1 values(1,null),(null,1),(1,1),(null,null);
-- end_ignore

select * from Tbl8352_t1 order by 1,2;

-- start_ignore
insert into Tbl8352_t2 values(1,1);
-- end_ignore

select * from Tbl8352_t2;
select * from Tbl8352_t1 where (Tbl8352_t1.a,Tbl8352_t1.b) not in (select Tbl8352_t2.a,Tbl8352_t2.b from Tbl8352_t2);

-- start_ignore
create table Tbl8352_t1a(a int, b int) distributed by (a);
create table Tbl8352_t2a(a int, b int) distributed by (a);
insert into Tbl8352_t1a values(1,2),(3,null),(null,4),(null,null);
-- end_ignore

select * from Tbl8352_t1a order by 1,2;

-- start_ignore
insert into Tbl8352_t2a values(1,2);
-- end_ignore

select * from Tbl8352_t2a;
select * from Tbl8352_t1a where (Tbl8352_t1a.a,Tbl8352_t1a.b) not in (select Tbl8352_t2a.a,Tbl8352_t2a.b from Tbl8352_t2a) order by 1,2;

select (1,null::int) not in (select 1,1);
select (3,null::int) not in (select 1,1);

-- start_ignore
create table t1(a int, b int);
create table t2(a int, b int);
create table t3(a int, b int);
create table t4(a int, b int);

insert into t1 values(1,2);
insert into t1 values(3,4);
insert into t1 values(5,6);

insert into t2 values(1,2);
insert into t2 values(3,4);
insert into t2 values(7,8);

insert into t3 values(1,2);
insert into t3 values(3,4);

insert into t4 values(1,2);

create table i1(a int, b int);
create table i2(a int, b int);

insert into i1 values(1,2);
-- end_ignore

--
-- not in subquery involving vars from different rels with inner join
--

select t1.a, t2.b from t1, t2 where t1.a=t2.a and ((t1.a,t2.b) not in (select i1.a,i1.b from i1));

select t1.a, t2.b from t1 inner join t2 on  (t1.a=t2.a and ((t1.a,t2.b) not in (select i1.a,i1.b from i1)));

select t1.a, t2.b from t1 inner join t2 on  (t1.a=t2.a) where ((t1.a,t2.b) not in (select i1.a,i1.b from i1));

-- unsupported case
explain select t1.a, t2.b from t1, t2 where t1.a=t2.a or ((t1.a,t2.b) not in (select i1.a,i1.b from i1));

--
-- not in subquery involving vars from different rels with left join. 
--
select t1.a, t2.b from t1 left join t2 on  (t1.a=t2.a) where ((t1.a,t2.b) not in (select i1.a,i1.b from i1));

select t1.a, t2.b from t1 left join t2 on  (t1.a=t2.a and ((t1.a,t2.b) not in (select i1.a,i1.b from i1)));

--
-- not in subquery involving vars from different rels with outer join
--

select t1.a, t2.b from t1 full outer join t2 on  (t1.a=t2.a) where ((t1.a,t2.b) not in (select i1.a,i1.b from i1));

-- not in subquery with a row var in FULL JOIN condition
select t1.a, t2.b from t1 full outer join t2 on  (t1.a=t2.a and ((t1.a,t2.b) not in (select i1.a,i1.b from i1))); 

--
-- more complex case
--

select t1.a,t2.b from t1 left join (t2 inner join t3 on (t3.a not in (select t4.a from t4))) on (t1.a=t2.a);

-- start_ignore
create table Tbl01(a int, b int, c int);

insert into Tbl01 values(1,2,3);
insert into Tbl01 values(4,5,6);
insert into Tbl01 values(7,8,9);
insert into Tbl01 values(null,11,12);

create table Tbl03(a int);

insert into Tbl03 values(1),(4);

create or replace function foo(int) returns int as $$
	select case when $1 is null then 13::int
	       	    else null::int
	       end;
$$ language sql immutable;
-- end_ignore

select Tbl01.*,foo(Tbl01.a) as foo from Tbl01; -- showing foo values

select Tbl01.* from Tbl01 where foo(Tbl01.a) not in (select a from Tbl03);

-- start_ignore
create table Tbl02 as select Tbl01.*,foo(Tbl01.a) as foo from Tbl01;
-- end_ignore

select Tbl02.* from Tbl02 where foo not in (select a from Tbl03);

-- start_ignore
create table Tbl04(a int, b int);
insert into Tbl04 values(1,2),(3,4),(5,6);

create table Tbl05(a int, b int);
insert into Tbl05 values(1,2);

create table Tbl06(a int, b int);
insert into Tbl06 values(1,2),(3,4);

create table i3(a int not null, b int not null);
insert into i3 values(1,2);

create table Tbl07(a int, b int);
insert into Tbl07 values(1,2),(3,4),(null,null);

create table Tbl08(a int, b int);
insert into Tbl08 values(1,2),(3,4),(null,null);

create table Tbl09(a int, b int);
insert into Tbl09 values(1,2),(5,null),(null,8);
-- end_ignore

--
-- Positive cases: We should be inferring non-nullability of the not-in subquery. This should result in HLASJ.
--

-- non-nullability due to inner join

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05,Tbl06 where Tbl05.a=Tbl06.a and Tbl05.b < 10); -- expected: (3,4),(5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07 inner join Tbl08 on (Tbl07.a=Tbl08.a and Tbl07.b=Tbl08.b) inner join i3 on (i3.a=Tbl08.a and i3.b=Tbl08.b)); -- expected:(3,4), (5,6)

-- non-nullability due to where clause condition

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05 where Tbl05.a < 2 and Tbl05.b < 10); -- expected: (3,4), (5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07 left join Tbl08 on (Tbl07.a=Tbl08.a) where Tbl07.a = 1 and Tbl07.b = 2); -- expected: (3,4),(5,6)

-- not null condition in the where clause

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07 full outer join Tbl08 on (Tbl07.a=Tbl08.a) where Tbl07.a is not null and Tbl07.b is not null); -- (5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07 left join Tbl08 on (Tbl07.a=Tbl08.a) where Tbl07.a is not null and Tbl07.b is not null); -- (5,6)

-- or clauses that should lead to non-nullability

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05 where (Tbl05.a < 2 or Tbl05.a > 100) AND (Tbl05.b < 4 or Tbl05.b > 100)); -- expected: (3,4), (5,6)

-- base-table constraints

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a,i3.b from i3); -- expected: (3,4),(5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05,i3 where	Tbl05.a = i3.a and	Tbl05.b = i3.b);

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05,i3 where Tbl05.a < i3.a and Tbl05.b > i3.b);

-- non-null constant values

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select 1,2); -- (3,4),(5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in ((1,2));

-- multiple NOT-IN expressions

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl06.a,Tbl06.b from Tbl06) and (Tbl04.a,Tbl04.b) not in (select i3.a, i3.b from i3); -- expected: (5,6)

select Tbl04.* from Tbl04 where not ((Tbl04.a,Tbl04.b) in (select Tbl06.a,Tbl06.b from Tbl06) or (Tbl04.a,Tbl04.b) in (select i3.a, i3.b from i3)); -- expected: (5,6)

-- single column in the target list should always give a HLASJ

select Tbl04.* from Tbl04 where Tbl04.a NOT IN (select Tbl09.a from Tbl09 where Tbl09.b is null); -- (1,2) (3,4)

select Tbl04.* from Tbl04 where Tbl04.a NOT IN (select i3.a from i3);

select Tbl04.* from Tbl04 where Tbl04.a NOT IN (select Tbl05.a from	Tbl05 left	join i3	on (Tbl05.a=i3.a));

--
-- Negative tests: we should not be inferring non-nullability in these cases. Therefore, we should see NLASJ.
--

-- No where clause

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05); -- expected: (3,4), (5,6)

-- INDF in the where clause

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07,Tbl08 where Tbl07.a is not distinct from Tbl08.a and Tbl07.b is not distinct from Tbl08.b); -- no rows

-- null conditions in the where clause

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07 left join Tbl08 on (Tbl07.a=Tbl08.a and Tbl07.b=Tbl08.b) where Tbl07.a is null and Tbl07.b is null); -- no rows

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07 full outer join Tbl08 on (Tbl07.a=Tbl08.a and Tbl07.b=Tbl08.b) where Tbl07.a is null and Tbl07.b is null); -- no rows

-- OR clauses that should not lead to non-nullability

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl07.a,Tbl07.b from Tbl07,Tbl08 where Tbl07.a is not distinct from Tbl08.a or Tbl07.a=1); -- no rows

-- values list: we don't support it yet. not worth the effort.

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (values(1,2),(3,4)); -- (3,4),(5,6)

-- functions/ops in the target list of the subquery

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a+2,i3.b+2 from i3); -- expected: (5,6)

-- group by does not guarantee removal of nulls. 

select Tbl09.a, Tbl09.b from Tbl09;

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl09.a,Tbl09.b from Tbl09); -- expected: (3,4)

select Tbl09.a, Tbl09.b from Tbl09 group by Tbl09.a, Tbl09.b;

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl09.a, Tbl09.b from Tbl09 group by Tbl09.a, Tbl09.b); -- expected: (3,4)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select sum(i3.b),i3.a from i3 group by i3.a); -- (1,2),(3,4),(5,6)

-- infering not-nullability for only one of the columns

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a,Tbl05.b from i3,Tbl05 where	i3.a=Tbl05.a); -- (3,4),(5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) NOT IN (select i3.a,i3.b from Tbl07 left join i3 on (i3.a=Tbl07.a and i3.b=Tbl07.b) where i3.a > 2);

--
-- Unsupported test: These tests are meant to illustrate NOT-IN subqueries we do not support
-- Started supporting since RIO
--

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a,i3.b from i3 union select Tbl07.a, Tbl07.b from Tbl07); -- nulls in the inner side, should not return any rows

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a,i3.b from i3 union all select Tbl07.a, Tbl07.b from Tbl07); -- nulls in the innder side, should not return any rows

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select 1,2 union select 3,4); --(5,6)

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a,i3.b from i3) or (Tbl04.a,Tbl04.b) not in (select Tbl07.a, Tbl07.b from Tbl07);

-- Cases where the planner "should have" determined not-nullabitlity

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select i3.a,i3.b from i3 left join Tbl07 on (i3.a=Tbl07.a and i3.b=Tbl07.b));

select Tbl04.* from Tbl04 where (Tbl04.a,Tbl04.b) not in (select Tbl05.a,Tbl05.b from Tbl05 where (Tbl05.a IN (select i3.a from i3)) AND (Tbl05.b IN (select i3.b from i3)));

-- additional queries

-- start_ignore
drop table if exists Tbl04;

create table Tbl04(x int, y int);
insert into Tbl04 values(1,2);
insert into Tbl04 values(3,4);

create table Tbl10(x int, y int);
insert into Tbl10 values(1,null);
-- end_ignore

select * from Tbl04 where (x,y) not in (select x,y from Tbl10);

select * from Tbl04 where (x,y) not in (select 1,y from Tbl10);

select * from tbl10 where y not in (select 1 where false);

-- start_ignore
alter table Tbl10 alter column x set not null;
-- end_ignore
select * from Tbl04 where (x,y) not in (select x,y from Tbl10);

-- start_ignore
create table TblText1(a text, b text);
create table TblText2(a text, b text);
create table TblText3(a text, b text);

insert into TblText1 values('rob', 'klopp');
insert into TblText1 values('florian','waas');
insert into TblText1 values('oak','barrett');
insert into TblText1 values('tushar','pednekar');

insert into TblText2 select * from TblText1;

insert into TblText3 values('florian','waas');
insert into TblText3 values('oak','barrett');
-- end_ignore

SELECT TblText1.a, TblText2.b FROM TblText1 JOIN TblText2 ON TblText1.a = TblText2.a WHERE ((NOT (TblText1.a, TblText2.b) IN (SELECT TblText3.a, TblText3.b FROM TblText3)));

SELECT TblText1.a, TblText2.b FROM TblText1 JOIN TblText2 ON TblText1.a = TblText2.a WHERE (( (TblText1.a, TblText2.b) IN (SELECT TblText3.a, TblText3.b FROM TblText3)));

--
-- Delete
--
-- start_ignore
create table TabDel1(a int, b int);
insert into TabDel1 values(1,2),(3,4),(5,6);

create table TabDel2 as select * from TabDel1;

create table TabDel3(a int, b int);
insert into TabDel3 values(1,2);

create table TabDel4(a int not null, b int not null);
insert into TabDel4 values(1,2);
-- end_ignore

explain delete from TabDel1 where TabDel1.a not in (select a from TabDel3); -- do not support this because we produce NLASJ

explain delete from TabDel2 where TabDel2.a not in (select a from TabDel4); -- support this
-- start_ignore
delete from TabDel2 where TabDel2.a not in (select a from TabDel4); 
-- end_ignore
select * from TabDel2;

--
-- Update
--
-- start_ignore
create table TblUp1(a int, b int);
insert into TblUp1 values(1,2),(3,4),(5,6);

create table TblUp2 as select * from TblUp1;

create table TblUp3(a int, b int);
insert into TblUp3 values(1,2);

create table TblUp4(a int not null, b int not null);
insert into TblUp4 values(1,2);

-- end_ignore

-- planner does not support updates on distribution keys
update TblUp1 set a=100 where a not in (select a from TblUp3);
select * from TblUp1;
update TblUp2 set a=100 where a not in (select a from TblUp4);
select * from TblUp2;
-- start_ignore
drop schema qp_subquery cascade;
-- end_ignore
