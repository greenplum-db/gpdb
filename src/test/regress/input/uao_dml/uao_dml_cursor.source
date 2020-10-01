create schema uao_dml_cursor_@amname@;
set search_path=uao_dml_cursor_@amname@;
SET default_table_access_method=@amname@;

-- @Description BINARY CURSOR.
--
DROP TABLE IF EXISTS sto_uao_emp_forcursor cascade;
CREATE TABLE sto_uao_emp_forcursor (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_forcursor values
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
DROP TABLE IF EXISTS sto_uao_dept_forcursor cascade;
CREATE TABLE sto_uao_dept_forcursor (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(15)) distributed by (deptno);

insert into sto_uao_dept_forcursor values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(4, 'OPERATIONS','SEATTLE');

BEGIN;
DECLARE 
    all_emp_upd_rollback BINARY CURSOR FOR SELECT ename, loc FROM sto_uao_emp_forcursor  JOIN sto_uao_dept_forcursor
                               on dept = deptno order by ename ;
    FETCH FIRST from all_emp_upd_rollback;
    update sto_uao_dept_forcursor set loc = 'NEW YORK CITY' where loc = 'NEW YORK';
    MOVE 5 FROM all_emp_upd_rollback;
    FETCH  1 from all_emp_upd_rollback ;
    CLOSE all_emp_upd_rollback;
ROLLBACK;

BEGIN;
DECLARE all_dept BINARY CURSOR FOR SELECT dname,loc from sto_uao_dept_forcursor order by 1;
fetch all from all_dept;
close all_dept;
commit;

BEGIN;
DECLARE all_dept_del_rollback BINARY CURSOR FOR SELECT dname,loc from sto_uao_dept_forcursor order by 1;
delete from sto_uao_dept_forcursor where loc = 'NEW YORK';
fetch all from all_dept_del_rollback;
close all_dept_del_rollback;
ROLLBACK;

BEGIN;
DECLARE all_dept_aft_del BINARY CURSOR FOR SELECT dname,loc from sto_uao_dept_forcursor order by 1;
fetch all from all_dept_aft_del;
close all_dept_aft_del;
commit;

BEGIN;
DECLARE 
    all_emp_del_commit BINARY CURSOR FOR SELECT ename FROM sto_uao_emp_forcursor order by ename desc ;
    FETCH FIRST from all_emp_del_commit;
    delete FROM sto_uao_emp_forcursor where empno > 10;
    FETCH 2 from all_emp_del_commit;
    MOVE 5 FROM all_emp_del_commit;
    FETCH  1 from all_emp_del_commit ;
    FETCH NEXT from all_emp_del_commit;
    FETCH all from all_emp_del_commit;
    CLOSE all_emp_del_commit;
COMMIT;

BEGIN;
DECLARE 
    all_emp_aft_del CURSOR FOR SELECT ename FROM sto_uao_emp_forcursor order by ename desc ;
    FETCH all from all_emp_aft_del;
    CLOSE all_emp_aft_del;

COMMIT;
BEGIN;
DECLARE 
    all_emp_upd_commit BINARY CURSOR FOR SELECT ename, loc FROM sto_uao_emp_forcursor  JOIN sto_uao_dept_forcursor
                               on dept = deptno order by ename ;
    FETCH FIRST from all_emp_upd_commit;
    FETCH 2 from all_emp_upd_commit;
    update sto_uao_dept_forcursor set loc = 'NEW YORK CITY' where loc = 'NEW YORK';
    MOVE 5 FROM all_emp_upd_commit;
    FETCH  1 from all_emp_upd_commit ;
    FETCH NEXT from all_emp_upd_commit;
    FETCH all from all_emp_upd_commit;
    CLOSE all_emp_upd_commit;
COMMIT;
BEGIN;
DECLARE all_dept2 BINARY CURSOR FOR SELECT dname,loc from sto_uao_dept_forcursor order by 1;
fetch all from all_dept2;
close all_dept2;
commit;
BEGIN;
DECLARE 
    all_emp_aft_upd BINARY CURSOR FOR SELECT ename, loc FROM sto_uao_emp_forcursor  JOIN sto_uao_dept_forcursor
                               on dept = deptno order by ename ;
    FETCH FIRST from all_emp_aft_upd;
    FETCH 2 from all_emp_aft_upd;
    MOVE 5 FROM all_emp_aft_upd;
    FETCH  1 from all_emp_aft_upd ;
    FETCH NEXT from all_emp_aft_upd;
    FETCH all from all_emp_aft_upd;
    CLOSE all_emp_aft_upd;
COMMIT;


-- @Description WITH HOLD cursor.
--
DROP TABLE IF EXISTS sto_uao_emp_forcursor cascade;
CREATE TABLE sto_uao_emp_forcursor (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) 
distributed by (empno);

insert into sto_uao_emp_forcursor values
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

BEGIN;
DECLARE 
    all_emp_withhold BINARY CURSOR WITH HOLD FOR SELECT ename FROM sto_uao_emp_forcursor order by ename asc ;
    FETCH ABSOLUTE 1 from all_emp_withhold;
    FETCH 3 from all_emp_withhold;

COMMIT;
-- fetch after commit +ve testcase
FETCH next from all_emp_withhold;
close all_emp_withhold;

BEGIN;
DECLARE 
    all_emp_withouthold BINARY CURSOR FOR SELECT ename FROM sto_uao_emp_forcursor order by ename asc ;
    FETCH ABSOLUTE 1 from all_emp_withouthold;
    FETCH 3 from all_emp_withouthold;

COMMIT;
-- fetch after commit -ve testcase
FETCH next from all_emp_withouthold;
close all_emp_withouthold;


-- @Description UPDATEABLE BINARY cursor.
--
DROP TABLE IF EXISTS sto_uao_emp_forcursor cascade;
CREATE TABLE sto_uao_emp_forcursor (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_forcursor values
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
DROP TABLE IF EXISTS sto_uao_dept_forcursor cascade;
CREATE TABLE sto_uao_dept_forcursor (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(15)) distributed by (deptno);

insert into sto_uao_dept_forcursor values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(4, 'OPERATIONS','SEATTLE');

BEGIN;
DECLARE 
    all_emp_uao_upd_commit BINARY CURSOR FOR SELECT ename FROM sto_uao_emp_forcursor order by ename ;
    FETCH forward 3 from  all_emp_uao_upd_commit;
    FETCH NEXT in all_emp_uao_upd_commit;
    update sto_uao_emp_forcursor set ename = ename || '_NEW'  where current of all_emp_uao_upd_commit;
    FETCH NEXT from all_emp_uao_upd_commit;
    FETCH all from all_emp_uao_upd_commit;
    CLOSE all_emp_uao_upd_commit;
COMMIT;

BEGIN;
DECLARE 
    all_emp_uao_aft_upd BINARY CURSOR FOR SELECT ename, loc FROM sto_uao_emp_forcursor  JOIN sto_uao_dept_forcursor
                               on dept = deptno order by ename ;
    FETCH all from all_emp_uao_aft_upd;
    CLOSE all_emp_uao_aft_upd;
COMMIT;


