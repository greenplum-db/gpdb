-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE SEQUENCE ck_sync1_seq1  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ck_sync1_seq2  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ck_sync1_seq3  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ck_sync1_seq4  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ck_sync1_seq5  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ck_sync1_seq6  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ck_sync1_seq7  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;

DROP SEQUENCE sync1_seq2;
DROP SEQUENCE ck_sync1_seq1;

-- SEQUENCE USAGE

CREATE TABLE ck_sync1_tbl1 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl1 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq11 START 101 OWNED BY ck_sync1_tbl1.col1;
INSERT INTO ck_sync1_tbl1 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ck_sync1_tbl2 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl2 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq22 START 101 OWNED BY ck_sync1_tbl2.col1;
INSERT INTO ck_sync1_tbl2 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ck_sync1_tbl3 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl3 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq33 START 101 OWNED BY ck_sync1_tbl3.col1;
INSERT INTO ck_sync1_tbl3 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ck_sync1_tbl4 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl4 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq44 START 101 OWNED BY ck_sync1_tbl4.col1;
INSERT INTO ck_sync1_tbl4 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ck_sync1_tbl5 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl5 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq55 START 101 OWNED BY ck_sync1_tbl5.col1;
INSERT INTO ck_sync1_tbl5 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ck_sync1_tbl6 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl6 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq66 START 101 OWNED BY ck_sync1_tbl6.col1;
INSERT INTO ck_sync1_tbl6 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ck_sync1_tbl7 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ck_sync1_tbl7 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ck_sync1_seq77 START 101 OWNED BY ck_sync1_tbl7.col1;
INSERT INTO ck_sync1_tbl7 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


DROP SEQUENCE sync1_seq22;
DROP SEQUENCE ck_sync1_seq11;
