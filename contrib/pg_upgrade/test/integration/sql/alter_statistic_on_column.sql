CREATE TABLE explicitly_set_statistic_table (
    col1 integer NOT NULL
);
ALTER TABLE ONLY explicitly_set_statistic_table ALTER COLUMN col1 SET STATISTICS 10;
INSERT INTO explicitly_set_statistic_table SELECT i FROM generate_series(1,10)i;
