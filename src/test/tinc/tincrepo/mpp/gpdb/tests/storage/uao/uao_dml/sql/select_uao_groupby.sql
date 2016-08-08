-- @description : Create Data and execute select statements on UAO tables GROUP BY
-- 
DROP TABLE IF EXISTS sto_uao_emp_groupby;
CREATE TABLE sto_uao_emp_groupby (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) with (appendonly=true) distributed by (empno);

insert into sto_uao_emp_groupby values 
 (1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,2)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);
DROP TABLE IF EXISTS sto_uao_dept_groupby;
CREATE TABLE sto_uao_dept_groupby (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) with (appendonly=true) distributed by (deptno);

insert into sto_uao_dept_groupby values 
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(4, 'OPERATIONS','SEATTLE');


-- Simple Group by
select count(*) as num_of_emp, loc from sto_uao_dept_groupby, sto_uao_emp_groupby where sto_uao_dept_groupby.deptno = sto_uao_emp_groupby.dept group by loc order by 1 asc;
select  loc ,job, sum(sal) as sum_of_salary, count(*) as num_emp  from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
group by loc,job order by loc,job;

-- Rollup

select  loc ,job,comm,sum(sal),  count(*) as cnt from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
and loc in ('NEW YORK', 'ATLANTA')
group by rollup (loc,job,comm) order by loc,job,comm;

-- Cube
select  loc ,job,comm,sum(sal),  count(*) as cnt from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
and loc != 'SEATTLE'
group by cube (loc,job,comm) order by loc,job,comm;

-- grouping sets
select  loc ,job,comm,sum(sal) as sum from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
group by grouping sets ((loc,job),  (loc,comm)) 
order by loc,job,comm;

-- grouping set , rollup
select  loc ,job, sum(sal) from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
group by grouping sets (loc, rollup (loc,job) ) 
order by loc,job ;

