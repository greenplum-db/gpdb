-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE SEQUENCE ct_seq1  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ct_seq2  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ct_seq3  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ct_seq4  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE ct_seq5  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;

DROP SEQUENCE sync1_seq4;
DROP SEQUENCE ck_sync1_seq3;
DROP SEQUENCE ct_seq1;

-- SEQUENCE USAGE

CREATE TABLE ct_tbl1 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ct_tbl1 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ct_seq11 START 101 OWNED BY ct_tbl1.col1;
INSERT INTO ct_tbl1 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ct_tbl2 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ct_tbl2 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ct_seq22 START 101 OWNED BY ct_tbl2.col1;
INSERT INTO ct_tbl2 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ct_tbl3 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ct_tbl3 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ct_seq33 START 101 OWNED BY ct_tbl3.col1;
INSERT INTO ct_tbl3 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ct_tbl4 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ct_tbl4 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ct_seq44 START 101 OWNED BY ct_tbl4.col1;
INSERT INTO ct_tbl4 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE ct_tbl5 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO ct_tbl5 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE ct_seq55 START 101 OWNED BY ct_tbl5.col1;
INSERT INTO ct_tbl5 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));

DROP SEQUENCE sync1_seq44;
DROP SEQUENCE ck_sync1_seq33;
DROP SEQUENCE ct_seq11;
