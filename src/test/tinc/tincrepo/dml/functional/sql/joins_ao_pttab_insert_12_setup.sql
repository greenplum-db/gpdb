-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
