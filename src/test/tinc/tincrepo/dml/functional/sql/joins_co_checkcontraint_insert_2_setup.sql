-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_co_check_p;
DROP TABLE IF EXISTS dml_co_check_r;
DROP TABLE IF EXISTS dml_co_check_s;

CREATE TABLE dml_co_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_co_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_co_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_co_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_co_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_co_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_co_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
