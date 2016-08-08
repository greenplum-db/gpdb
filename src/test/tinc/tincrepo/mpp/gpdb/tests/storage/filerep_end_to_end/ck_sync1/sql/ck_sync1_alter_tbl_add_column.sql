-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP TEST
--
CREATE TABLE ck_sync1_add_column_heap (col1 char DEFAULT 'z',col2 numeric  DEFAULT  100 )distributed randomly;
INSERT INTO ck_sync1_add_column_heap VALUES ('a',generate_series(1,100));
select count(*) from ck_sync1_add_column_heap;

ALTER TABLE sync1_add_column_heap ADD COLUMN col3 boolean DEFAULT false   ;
INSERT INTO sync1_add_column_heap VALUES ('c',generate_series(1,100),true);
select count(*) from sync1_add_column_heap;

ALTER TABLE ck_sync1_add_column_heap ADD COLUMN col3 boolean DEFAULT false   ;
INSERT INTO ck_sync1_add_column_heap VALUES ('b',generate_series(1,100),true);
select count(*) from ck_sync1_add_column_heap;


--
-- AO TEST
--
CREATE TABLE ck_sync1_add_column_ao (col1 char DEFAULT 'z',col2 numeric  DEFAULT  100 )  with (appendonly=true) distributed randomly;
INSERT INTO ck_sync1_add_column_ao VALUES ('a',generate_series(1,100));
select count(*) from ck_sync1_add_column_ao;

ALTER TABLE sync1_add_column_ao ADD COLUMN col3 boolean DEFAULT false   ;
INSERT INTO sync1_add_column_ao VALUES ('c',generate_series(1,100),true);
select count(*) from sync1_add_column_ao;

ALTER TABLE ck_sync1_add_column_ao ADD COLUMN col3 boolean DEFAULT false   ;
INSERT INTO ck_sync1_add_column_ao VALUES ('b',generate_series(1,100),true);
select count(*) from ck_sync1_add_column_ao;

--
-- CO TEST
--
CREATE TABLE ck_sync1_add_column_co (col1 char DEFAULT 'z',col2 numeric  DEFAULT  100 )  with (orientation='column',appendonly=true) distributed randomly;
INSERT INTO ck_sync1_add_column_co VALUES ('a',generate_series(1,100));
select count(*) from ck_sync1_add_column_co;

ALTER TABLE sync1_add_column_co ADD COLUMN col3 boolean DEFAULT false   ;
INSERT INTO sync1_add_column_co VALUES ('c',generate_series(1,100),true);
select count(*) from sync1_add_column_co;

ALTER TABLE ck_sync1_add_column_co ADD COLUMN col3 boolean DEFAULT false   ;
INSERT INTO ck_sync1_add_column_co VALUES ('b',generate_series(1,100),true);
select count(*) from ck_sync1_add_column_co;


