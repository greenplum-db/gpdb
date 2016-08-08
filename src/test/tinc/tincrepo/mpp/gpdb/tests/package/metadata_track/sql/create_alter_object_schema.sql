create schema myschema;
create schema myschema_new;

create table myschema.test_tbl_heap ( a int, b text) distributed by (a);
analyze myschema.test_tbl_heap;
vacuum myschema.test_tbl_heap;
ALTER TABLE myschema.test_tbl_heap  SET SCHEMA myschema_new;
analyze myschema_new.test_tbl_heap;
vacuum myschema_new.test_tbl_heap;

create table myschema.test_tbl_ao ( a int, b text) with (appendonly=true) distributed by (a);  
analyze myschema.test_tbl_ao;
vacuum myschema.test_tbl_ao;
ALTER TABLE myschema.test_tbl_ao  SET SCHEMA myschema_new;
analyze myschema_new.test_tbl_ao;
vacuum myschema_new.test_tbl_ao;

create table myschema.test_tbl_co ( a int, b text) with (appendonly=true, orientation='column') distributed by (a);
analyze myschema.test_tbl_co;
vacuum myschema.test_tbl_co;
ALTER TABLE myschema.test_tbl_co  SET SCHEMA myschema_new;
analyze myschema_new.test_tbl_co;
vacuum myschema_new.test_tbl_co;

create view myschema.test_view as SELECT * from myschema_new.test_tbl_heap;
create index test_tbl_idx on myschema_new.test_tbl_heap(a);
create sequence myschema.seq1 start with 101;
ALTER SEQUENCE myschema.seq1 SET SCHEMA myschema_new;


drop view myschema.test_view;
drop table myschema_new.test_tbl_heap;
drop table myschema_new.test_tbl_ao;
drop table myschema_new.test_tbl_co;
drop sequence myschema_new.seq1;
drop schema myschema;
drop schema myschema_new;
