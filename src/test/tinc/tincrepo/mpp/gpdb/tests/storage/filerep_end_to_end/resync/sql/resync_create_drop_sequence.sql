-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE SEQUENCE resync_seq1  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE resync_seq2  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE resync_seq3  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;

DROP SEQUENCE sync1_seq6;
DROP SEQUENCE ck_sync1_seq5;
DROP SEQUENCE ct_seq3;
DROP SEQUENCE resync_seq1;

-- SEQUENCE USAGE

CREATE TABLE resync_tbl1 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO resync_tbl1 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE resync_seq11 START 101 OWNED BY resync_tbl1.col1;
INSERT INTO resync_tbl1 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE resync_tbl2 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO resync_tbl2 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE resync_seq22 START 101 OWNED BY resync_tbl2.col1;
INSERT INTO resync_tbl2 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


CREATE TABLE resync_tbl3 ( col1 int, col2 text, col3 int) DISTRIBUTED RANDOMLY;
INSERT INTO resync_tbl3 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));
CREATE SEQUENCE resync_seq33 START 101 OWNED BY resync_tbl3.col1;
INSERT INTO resync_tbl3 values (generate_series(1,100),repeat('seq_tbl_string',100),generate_series(1,100));


DROP SEQUENCE sync1_seq66;
DROP SEQUENCE ck_sync1_seq55;
DROP SEQUENCE ct_seq33;
DROP SEQUENCE resync_seq11;
