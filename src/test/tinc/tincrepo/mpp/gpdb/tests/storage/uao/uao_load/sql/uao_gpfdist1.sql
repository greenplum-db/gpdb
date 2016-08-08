-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE EXTERNAL TABLE ext_customer
      (id int, name text, sponsor text)
      LOCATION ( 'gpfdist://localhost:8082/*.txt' )
      FORMAT 'TEXT' ( DELIMITER ';' NULL ' ')
      LOG ERRORS SEGMENT REJECT LIMIT 5;

INSERT INTO customer SELECT * FROM ext_customer;
