-- @tag mpp-23801
-- @description ALTER TABLE set distribution key should check compatible with unique index. 

-- case 1

-- start_ignore
DROP TABLE IF EXISTS t_dist1;
CREATE TABLE t_dist1(col1 INTEGER, col2 INTEGER, CONSTRAINT pk_t_dist1 PRIMARY KEY(col2)) DISTRIBUTED BY(col2);
-- end_ignore
ALTER TABLE t_dist1 SET DISTRIBUTED BY(col1); 

-- case 2

-- start_ignore
DROP TABLE t_dist2;
CREATE TABLE t_dist2(col1 INTEGER, col2 INTEGER, col3 INTEGER, col4 INTEGER) DISTRIBUTED BY(col1);

DROP INDEX idx1_t_dist2;
CREATE UNIQUE INDEX idx1_t_dist2 ON t_dist2(col1, col2);

DROP INDEX idx2_t_dist2;
CREATE UNIQUE INDEX idx2_t_dist2 ON t_dist2(col1, col2, col3);

DROP INDEX idx3_t_dist2;
CREATE UNIQUE INDEX idx2_t_dist2 ON t_dist2(col1, col2, col4);
-- end_ignore

ALTER TABLE t_dist2 SET DISTRIBUTED BY(col1); 
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col2); 
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col1, col2); 

ALTER TABLE t_dist2 SET DISTRIBUTED BY(col1, col2, col3); 
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col3); 
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col4); 
