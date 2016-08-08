-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP TEST
--

CREATE TABLE resync_add_column_heap (col1 char DEFAULT 'z' ,col2 numeric  DEFAULT  100 ,col3 boolean DEFAULT false, col4 character varying(512)[]  DEFAULT '{one,two,three,four,five}', col5 timestamp with time zone  DEFAULT '2000-12-13 01:51:15' ,col6 macaddr  DEFAULT 'AA:AA:AA:AA:AA:AA' ) distributed randomly ;
INSERT INTO resync_add_column_heap VALUES ('a',generate_series(1,100),true,'{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,'BB:BB:BB:BB:BB:BB');
select count(*) from resync_add_column_heap;


ALTER TABLE sync1_add_column_heap ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO sync1_add_column_heap VALUES ('g',generate_series(1,100),true,'2009-10-13 02:35:50',repeat('text_',100));
select count(*) from sync1_add_column_heap;

ALTER TABLE ck_sync1_add_column_heap ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO ck_sync1_add_column_heap VALUES ('f',generate_series(1,100),true,'2009-10-13 02:35:50',repeat('text_',100));
select count(*) from ck_sync1_add_column_heap;

ALTER TABLE ct_add_column_heap ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO ct_add_column_heap VALUES ('d',generate_series(1,100),true, '{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50',repeat('text_',100));
select count(*) from ct_add_column_heap;

ALTER TABLE resync_add_column_heap ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO resync_add_column_heap VALUES ('b',generate_series(1,100),true,'{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,'BB:BB:BB:BB:BB:BB',repeat('text_',100));
select count(*) from resync_add_column_heap;

--
-- AO TEST
--

CREATE TABLE resync_add_column_ao (col1 char DEFAULT 'z' ,col2 numeric  DEFAULT  100 ,col3 boolean DEFAULT false, col4 character varying(512)[]  DEFAULT '{one,two,three,four,five}', col5 timestamp with time zone  DEFAULT '2000-12-13 01:51:15' ,col6 macaddr  DEFAULT 'AA:AA:AA:AA:AA:AA' )  with (appendonly=true)  distributed randomly ;
INSERT INTO resync_add_column_ao VALUES ('a',generate_series(1,100),true,'{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,'BB:BB:BB:BB:BB:BB');
select count(*) from resync_add_column_ao;


ALTER TABLE sync1_add_column_ao ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO sync1_add_column_ao VALUES ('g',generate_series(1,100),true,'2009-10-13 02:35:50' ,repeat('text_',100));
select count(*) from sync1_add_column_ao;

ALTER TABLE ck_sync1_add_column_ao ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO ck_sync1_add_column_ao VALUES ('f',generate_series(1,100),true, '2009-10-13 02:35:50' ,repeat('text_',100));
select count(*) from ck_sync1_add_column_ao;

ALTER TABLE ct_add_column_ao ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO ct_add_column_ao VALUES ('d',generate_series(1,100),true, '{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,repeat('text_',100));
select count(*) from ct_add_column_ao;

ALTER TABLE resync_add_column_ao ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO resync_add_column_ao VALUES ('b',generate_series(1,100),true,'{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,'BB:BB:BB:BB:BB:BB',repeat('text_',100));
select count(*) from resync_add_column_ao;


--
-- CO TEST
--
CREATE TABLE resync_add_column_co (col1 char DEFAULT 'z' ,col2 numeric  DEFAULT  100 ,col3 boolean DEFAULT false, col4 character varying(512)[]  DEFAULT '{one,two,three,four,five}', col5 timestamp with time zone  DEFAULT '2000-12-13 01:51:15' ,col6 macaddr  DEFAULT 'AA:AA:AA:AA:AA:AA' )  with (orientation='column',appendonly=true) distributed randomly ;
INSERT INTO resync_add_column_co VALUES ('a',generate_series(1,100),true,'{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,'BB:BB:BB:BB:BB:BB');
select count(*) from resync_add_column_co;

ALTER TABLE sync1_add_column_co ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO sync1_add_column_co VALUES ('g',generate_series(1,100),true,'2009-10-13 02:35:50' ,repeat('text_',100));
select count(*) from sync1_add_column_co;

ALTER TABLE ck_sync1_add_column_co ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO ck_sync1_add_column_co VALUES ('f',generate_series(1,100),true, '2009-10-13 02:35:50' ,repeat('text_',100));
select count(*) from ck_sync1_add_column_co;

ALTER TABLE ct_add_column_co ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO ct_add_column_co VALUES ('d',generate_series(1,100),true, '{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,repeat('text_',100));
select count(*) from ct_add_column_co;

ALTER TABLE resync_add_column_co ADD COLUMN col7 text DEFAULT 'pookie'   ;
INSERT INTO resync_add_column_co VALUES ('b',generate_series(1,100),true,'{aaaa,bbbb,cccc,dddd,eeee}', '2009-10-13 02:35:50' ,'BB:BB:BB:BB:BB:BB',repeat('text_',100));
select count(*) from resync_add_column_co;



