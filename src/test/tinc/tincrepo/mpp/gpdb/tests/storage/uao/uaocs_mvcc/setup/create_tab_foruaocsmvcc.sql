-- @Description create tables for MVCC tests
-- 

DROP TABLE IF EXISTS sto_uaocs_emp_formvcc cascade;
CREATE TABLE sto_uaocs_emp_formvcc (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) with (appendonly=true) distributed by (empno);

insert into sto_uaocs_emp_formvcc values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18001,10,4)
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
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18002,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18003,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',18004,30,1);

DROP TABLE IF EXISTS sto_uaocs_dept_formvcc cascade;
CREATE TABLE sto_uaocs_dept_formvcc (
   deptno INT NOT NULL,
   dname  VARCHAR(40),
   loc    VARCHAR(13)) with (appendonly=true) distributed by (deptno);

insert into sto_uaocs_dept_formvcc values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(5,'LOGISTICS','BOSTON')
,(4, 'OPERATIONS','SEATTLE');


DROP TABLE IF EXISTS sto_uaocs_mvcc_status;
CREATE TABLE sto_uaocs_mvcc_status (
  workload varchar(50),
  script   varchar(50),
  starttime timestamp NOT NULL default CURRENT_TIMESTAMP, 
  updover timestamp  NULL ,
  endtime timestamp  NULL )
with (appendonly = true ) DISTRIBUTED BY (starttime);
 


DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum cascade;

create  table sto_uaocs_mvcc_vacuum (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1000000) as i;
delete from sto_uaocs_mvcc_vacuum where id < 1000000;

DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum2 cascade;

create  table sto_uaocs_mvcc_vacuum2 (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum2 select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1000000) as i;
delete from sto_uaocs_mvcc_vacuum2 where id < 1000000;

DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum_upd cascade;
create  table sto_uaocs_mvcc_vacuum_upd (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum_upd select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1200000) as i;
delete from sto_uaocs_mvcc_vacuum_upd where id < 300000;

DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum_alter cascade;

create  table sto_uaocs_mvcc_vacuum_alter (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum_alter select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1000000) as i;
delete from sto_uaocs_mvcc_vacuum_alter where id < 1000000;


DROP TABLE IF EXISTS sto_uaocs_emp_formvcc_serial cascade;
CREATE TABLE sto_uaocs_emp_formvcc_serial (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) with (appendonly=true) distributed by (empno);

insert into sto_uaocs_emp_formvcc_serial values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18001,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',18002,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',18003,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',18004,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',18005,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',18006,20,4)
,(7,'POLK','TECH',6,'09-22-1997',18007,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',18008,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',18009,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',18010,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',18011,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18012,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',18013,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',18014,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',18015,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18016,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',18017,30,1);

DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum_upd_serial cascade;
create  table sto_uaocs_mvcc_vacuum_upd_serial (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum_upd_serial select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1200000) as i;
delete from sto_uaocs_mvcc_vacuum_upd_serial where id < 300000;

DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum_alter_serial cascade;
create  table sto_uaocs_mvcc_vacuum_alter_serial (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum_alter_serial select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1000000) as i;
delete from sto_uaocs_mvcc_vacuum_alter_serial where id < 1000000;





DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum_serial cascade;

create  table sto_uaocs_mvcc_vacuum_serial (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum_serial select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1000000) as i;
delete from sto_uaocs_mvcc_vacuum_serial where id < 1000000;

DROP TABLE IF EXISTS sto_uaocs_mvcc_vacuum_drop cascade;
create  table sto_uaocs_mvcc_vacuum_drop (
    id integer NOT NULL,
    name text NOT NULL,
    population integer NOT NULL
)  with (appendonly=true) distributed by(id);

insert into sto_uaocs_mvcc_vacuum_drop select i, 'abc'||i, (random() * 500)::integer from generate_series(1,1000000) as i;
delete from sto_uaocs_mvcc_vacuum_drop where id < 1000000;
