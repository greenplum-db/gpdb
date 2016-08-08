-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP TEST
--
CREATE TABLE sync1_add_column_heap (col1 char DEFAULT 'z') distributed randomly;
INSERT INTO sync1_add_column_heap VALUES ('a');
select count(*) from sync1_add_column_heap;

ALTER TABLE sync1_add_column_heap ADD COLUMN col2 numeric  DEFAULT  100;
INSERT INTO sync1_add_column_heap VALUES ('b',generate_series(1,100));
select count(*) from sync1_add_column_heap;

--
-- AO TEST
--
CREATE TABLE sync1_add_column_ao (col1 char DEFAULT 'z')  with (appendonly=true) distributed randomly;
INSERT INTO sync1_add_column_ao VALUES ('a');
select count(*) from sync1_add_column_ao;

ALTER TABLE sync1_add_column_ao ADD COLUMN col2 numeric  DEFAULT  100;
INSERT INTO sync1_add_column_ao VALUES ('b',generate_series(1,100));
select count(*) from sync1_add_column_ao;

--
-- CO TEST
--
CREATE TABLE sync1_add_column_co (col1 char DEFAULT 'z')  with (orientation='column',appendonly=true) distributed randomly;
INSERT INTO sync1_add_column_co VALUES ('a');
select count(*) from sync1_add_column_co;

ALTER TABLE sync1_add_column_co ADD COLUMN col2 numeric  DEFAULT  100;
INSERT INTO sync1_add_column_co VALUES ('b',generate_series(1,100));
select count(*) from sync1_add_column_co;
