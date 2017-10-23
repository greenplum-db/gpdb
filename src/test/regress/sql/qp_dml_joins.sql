-- First create a bunch of test tables

CREATE TABLE dml_ao_check_r (
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


CREATE TABLE dml_ao_check_s (
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

CREATE TABLE dml_ao_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);


INSERT INTO dml_ao_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_ao_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_ao_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);


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


CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');


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


CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);

CREATE TABLE dml_co_r (a int , b int default -1, c text) WITH (appendonly = true, orientation = column) DISTRIBUTED BY (a);
CREATE TABLE dml_co_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true, orientation = column) DISTRIBUTED BY (a,b);
CREATE TABLE dml_co_s as select dml_co_r.b, dml_co_r.a, dml_co_r.c from dml_co_r, dml_co_p WHERE dml_co_r.a = dml_co_p.a;
ALTER TABLE dml_co_s SET DISTRIBUTED BY (b);

INSERT INTO dml_co_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_co_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_co_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_co_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_co_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_co_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_co_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_co_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_co_r VALUES(generate_series(1,10),NULL,'rn');


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
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

CREATE TABLE dml_heap_check_s (
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

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);


CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);


-- test1: Insert data that satisfy the check constraints
begin;
SELECT COUNT(*) FROM dml_ao_check_s;
SELECT COUNT(*) FROM (SELECT dml_ao_check_s.a, dml_ao_check_s.b, 'text', dml_ao_check_s.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.b)foo;
INSERT INTO dml_ao_check_s SELECT dml_ao_check_s.a, dml_ao_check_s.b, 'text', dml_ao_check_s.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.b ;
SELECT COUNT(*) FROM dml_ao_check_s;
rollback;

-- test2: Negative test - Insert default value violates check constraint
SELECT COUNT(*) FROM dml_ao_check_p;
INSERT INTO dml_ao_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_ao_check_p;

-- test3: Negative test - Insert default value violates NOT NULL constraint
SELECT COUNT(*) FROM dml_ao_check_s;
INSERT INTO dml_ao_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_ao_check_s;

-- test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint
SELECT COUNT(*) FROM dml_ao_check_r;
SELECT COUNT(*) FROM (SELECT dml_ao_check_r.a + 110 , dml_ao_check_r.b, dml_ao_check_r.c, dml_ao_check_r.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a)foo;
INSERT INTO dml_ao_check_r SELECT dml_ao_check_r.a + 110 , dml_ao_check_r.b, dml_ao_check_r.c, dml_ao_check_r.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a AND dml_ao_check_r.b > 10;
SELECT COUNT(*) FROM dml_ao_check_r;

-- test5: Insert with joins where the result tuples violate violates multiple check constraints
SELECT COUNT(*) FROM dml_ao_check_r;
SELECT COUNT(*) FROM (SELECT dml_ao_check_r.a + 110 , 0, dml_ao_check_r.c, NULL FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a)foo;
INSERT INTO dml_ao_check_r SELECT dml_ao_check_r.a + 110 , 0, dml_ao_check_r.c, NULL FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a;
SELECT COUNT(*) FROM dml_ao_check_r;

-- test4: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_ao_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test5: Insert with generate_series and NULL
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
ALTER TABLE dml_ao_pt_r ADD DEFAULT partition def;
INSERT INTO dml_ao_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_ao_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_ao_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test6: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
TRUNCATE TABLE dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT generate_series(1,10),1;
SELECT * FROM dml_ao_pt_r ORDER BY 1;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test7: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_ao_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test8: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT *,1 from generate_series(1,10);
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test9: Join on the non-distribution key of target table
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.a)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.a;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test10: Join on the distribution key of target table
begin;
SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_s.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a)foo;
INSERT INTO dml_ao_pt_s SELECT dml_ao_pt_s.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a;
SELECT COUNT(*) FROM dml_ao_pt_s;
rollback;

-- test11: Join on distribution key of target table
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.a = dml_ao_pt_s.a)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.a = dml_ao_pt_s.a ;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test12: Join on the distribution key of target table. Insert Large number of rows
begin;
SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_s SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.b = dml_ao_pt_s.b ;
SELECT COUNT(*) FROM dml_ao_pt_s;
rollback;

-- test13: Join with different column order
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_s.a,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r(b,a,c) SELECT dml_ao_pt_s.a,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test14: Join with Aggregate
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
ALTER TABLE dml_ao_pt_r ADD DEFAULT partition def;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_ao_pt_s.a, dml_ao_pt_r.b + dml_ao_pt_r.a ,'text' FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b GROUP BY dml_ao_pt_s.a,dml_ao_pt_r.b,dml_ao_pt_r.a)foo;
INSERT INTO dml_ao_pt_r SELECT COUNT(*) + dml_ao_pt_s.a, dml_ao_pt_r.b + dml_ao_pt_r.a ,'text' FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b GROUP BY dml_ao_pt_s.a,dml_ao_pt_r.b,dml_ao_pt_r.a ;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test15: Join with DISTINCT
begin;
SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_s.a = dml_ao_pt_r.a)foo;
INSERT INTO dml_ao_pt_s SELECT DISTINCT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_s.a = dml_ao_pt_r.a ;
SELECT COUNT(*) FROM dml_ao_pt_s;
rollback;

-- test16: Insert NULL with Joins
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT NULL,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test18: Insert 0 rows
SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_ao_pt_r;

-- test19: Insert 0 rows
SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.a and false;
SELECT COUNT(*) FROM dml_ao_pt_r;

-- test20: Negative tests Insert column of different data type
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT ('a')::int, dml_ao_pt_r.b,10 FROM dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT ('a')::int, dml_ao_pt_r.b,10 FROM dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;

-- test21: Negative test case. INSERT has more expressions than target columns
begin;
SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a GROUP BY dml_ao_pt_r.a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d)foo;
INSERT INTO dml_ao_pt_s SELECT COUNT(*) as a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a GROUP BY dml_ao_pt_r.a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d;
SELECT COUNT(*) FROM dml_ao_pt_s;
rollback;

-- test22: Insert with join on the partition key
begin;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.* FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
rollback;

-- test24: Negative test - Insert NULL value to a table without default partition
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.b, NULL, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.b, NULL, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;

-- test25: Negative test - Insert out of partition range values for table without default partition
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.b ,dml_ao_pt_r.a + dml_ao_pt_s.a + 100, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.a = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.b ,dml_ao_pt_r.a + dml_ao_pt_s.a + 100, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.a = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;

-- test4: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_ao_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test5: Insert with generate_series and NULL
begin;
SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_ao_r WHERE c ='text' ORDER BY 1;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test6: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_r;
TRUNCATE TABLE dml_ao_r;
INSERT INTO dml_ao_r SELECT generate_series(1,10);
SELECT * FROM dml_ao_r ORDER BY 1;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test7: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_ao_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test8: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT * from generate_series(1,10);
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test9: Join on the non-distribution key of target table
begin;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test10: Join on the distribution key of target table
begin;
SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT dml_ao_s.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a)foo;
INSERT INTO dml_ao_s SELECT dml_ao_s.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a;
SELECT COUNT(*) FROM dml_ao_s;
rollback;

-- test11: Join on distribution key of target table
begin;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.a = dml_ao_s.a)foo;
INSERT INTO dml_ao_r SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.a = dml_ao_s.a ;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test12: Join on the distribution key of target table. Insert Large number of rows
begin;
SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.b <> dml_ao_s.b )foo;
INSERT INTO dml_ao_s SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.b <> dml_ao_s.b ;
SELECT COUNT(*) FROM dml_ao_s;
rollback;

-- test13: Join with different column order
begin;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT dml_ao_s.a,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r(b,a,c) SELECT dml_ao_s.a,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test14: Join with Aggregate
begin;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_ao_s.a, dml_ao_r.b + dml_ao_r.a ,'text' FROM dml_ao_r, dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b GROUP BY dml_ao_s.a,dml_ao_r.b,dml_ao_r.a)foo;
INSERT INTO dml_ao_r SELECT COUNT(*) + dml_ao_s.a, dml_ao_r.b + dml_ao_r.a ,'text' FROM dml_ao_r, dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b GROUP BY dml_ao_s.a,dml_ao_r.b,dml_ao_r.a ;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test15: Join with DISTINCT
begin;
SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_s.a = dml_ao_r.a)foo;
INSERT INTO dml_ao_s SELECT DISTINCT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_s.a = dml_ao_r.a ;
SELECT COUNT(*) FROM dml_ao_s;
rollback;

-- test16: Insert NULL with Joins
begin;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r SELECT NULL,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test17: Insert and CASE
begin;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_ao_p.a) as A, dml_ao_p.a as B, dml_ao_s.c as C FROM dml_ao_p, dml_ao_s WHERE dml_ao_p.a = dml_ao_s.a GROUP BY dml_ao_p.a,dml_ao_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_ao_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_ao_p.a) as A, dml_ao_p.a as B, dml_ao_s.c as C FROM dml_ao_p, dml_ao_s WHERE dml_ao_p.a = dml_ao_s.a GROUP BY dml_ao_p.a,dml_ao_s.c)as x GROUP BY A,B,C;
SELECT COUNT(*) FROM dml_ao_r;
rollback;

-- test18: Insert 0 rows
SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_ao_r;

-- test19: Insert 0 rows
SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.a and false;
SELECT COUNT(*) FROM dml_ao_r;

-- test21: Negative test case. INSERT has more expressions than target columns
SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_ao_r.* FROM dml_ao_r, dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a GROUP BY dml_ao_r.a, dml_ao_r.b, dml_ao_r.c)foo;
INSERT INTO dml_ao_s SELECT COUNT(*) as a, dml_ao_r.* FROM dml_ao_r, dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a GROUP BY dml_ao_r.a, dml_ao_r.b, dml_ao_r.c;
SELECT COUNT(*) FROM dml_ao_s;


-- test1: Insert data that satisfy the check constraints
begin;
SELECT COUNT(*) FROM dml_co_check_s;
SELECT COUNT(*) FROM (SELECT dml_co_check_s.a, dml_co_check_s.b, 'text', dml_co_check_s.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.b)foo;
INSERT INTO dml_co_check_s SELECT dml_co_check_s.a, dml_co_check_s.b, 'text', dml_co_check_s.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.b ;
SELECT COUNT(*) FROM dml_co_check_s;
rollback;

-- test2: Negative test - Insert default value violates check constraint
SELECT COUNT(*) FROM dml_co_check_p;
INSERT INTO dml_co_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_co_check_p;

-- test3: Negative test - Insert default value violates NOT NULL constraint
SELECT COUNT(*) FROM dml_co_check_s;
INSERT INTO dml_co_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_co_check_s;

-- test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint
SELECT COUNT(*) FROM dml_co_check_r;
SELECT COUNT(*) FROM (SELECT dml_co_check_r.a + 110 , dml_co_check_r.b, dml_co_check_r.c, dml_co_check_r.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a)foo;
INSERT INTO dml_co_check_r SELECT dml_co_check_r.a + 110 , dml_co_check_r.b, dml_co_check_r.c, dml_co_check_r.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a AND dml_co_check_r.b > 10;
SELECT COUNT(*) FROM dml_co_check_r;

-- test5: Insert with joins where the result tuples violate violates multiple check constraints
SELECT COUNT(*) FROM dml_co_check_r;
SELECT COUNT(*) FROM (SELECT dml_co_check_r.a + 110 , 0, dml_co_check_r.c, NULL FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a)foo;
INSERT INTO dml_co_check_r SELECT dml_co_check_r.a + 110 , 0, dml_co_check_r.c, NULL FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a;
SELECT COUNT(*) FROM dml_co_check_r;

-- test4: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_co_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test5: Insert with generate_series and NULL
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
ALTER TABLE dml_co_pt_r ADD DEFAULT partition def;
INSERT INTO dml_co_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_co_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_co_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test6: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
TRUNCATE TABLE dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT generate_series(1,10),1;
SELECT * FROM dml_co_pt_r ORDER BY 1;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test7: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_co_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test8: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT *,1 from generate_series(1,10);
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test9: Join on the non-distribution key of target table
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.a)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.a;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test10: Join on the distribution key of target table
begin;
SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT dml_co_pt_s.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a)foo;
INSERT INTO dml_co_pt_s SELECT dml_co_pt_s.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a;
SELECT COUNT(*) FROM dml_co_pt_s;
rollback;

-- test11: Join on distribution key of target table
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.a = dml_co_pt_s.a)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.a = dml_co_pt_s.a ;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test12: Join on the distribution key of target table. Insert Large number of rows
begin;
SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_s SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.b = dml_co_pt_s.b ;
SELECT COUNT(*) FROM dml_co_pt_s;
rollback;

-- test13: Join with different column order
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_s.a,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r(b,a,c) SELECT dml_co_pt_s.a,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test14: Join with Aggregate
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
ALTER TABLE dml_co_pt_r ADD DEFAULT partition def;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_co_pt_s.a, dml_co_pt_r.b + dml_co_pt_r.a ,'text' FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b GROUP BY dml_co_pt_s.a,dml_co_pt_r.b,dml_co_pt_r.a)foo;
INSERT INTO dml_co_pt_r SELECT COUNT(*) + dml_co_pt_s.a, dml_co_pt_r.b + dml_co_pt_r.a ,'text' FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b GROUP BY dml_co_pt_s.a,dml_co_pt_r.b,dml_co_pt_r.a ;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test15: Join with DISTINCT
begin;
SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_s.a = dml_co_pt_r.a)foo;
INSERT INTO dml_co_pt_s SELECT DISTINCT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_s.a = dml_co_pt_r.a ;
SELECT COUNT(*) FROM dml_co_pt_s;
rollback;

-- test16: Insert NULL with Joins
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT NULL,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test18: Insert 0 rows
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_co_pt_r;

-- test19: Insert 0 rows
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.a and false;
SELECT COUNT(*) FROM dml_co_pt_r;

-- test21: Negative test case. INSERT has more expressions than target columns
begin;
SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a GROUP BY dml_co_pt_r.a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d)foo;
INSERT INTO dml_co_pt_s SELECT COUNT(*) as a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a GROUP BY dml_co_pt_r.a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d;
SELECT COUNT(*) FROM dml_co_pt_s;
rollback;

-- test22: Insert with join on the partition key
begin;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.* FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
rollback;

-- test24: Negative test - Insert NULL value to a table without default partition
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.b, NULL, dml_co_pt_r.c FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.b, NULL, dml_co_pt_r.c FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;

-- test25: Negative test - Insert out of partition range values for table without default partition
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.b ,dml_co_pt_r.a + dml_co_pt_s.a + 100, dml_co_pt_r.c FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.a = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;

-- test4: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_co_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test5: Insert with generate_series and NULL
begin;
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_co_r WHERE c ='text' ORDER BY 1;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test6: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_r;
TRUNCATE TABLE dml_co_r;
INSERT INTO dml_co_r SELECT generate_series(1,10);
SELECT * FROM dml_co_r ORDER BY 1;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test7: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_co_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test8: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT * from generate_series(1,10);
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test9: Join on the non-distribution key of target table
begin;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT dml_co_r.* FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test10: Join on the distribution key of target table
begin;
SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT dml_co_s.* FROM dml_co_r,dml_co_s WHERE dml_co_s.a = dml_co_r.a)foo;
INSERT INTO dml_co_s SELECT dml_co_s.* FROM dml_co_r,dml_co_s WHERE dml_co_s.a = dml_co_r.a;
SELECT COUNT(*) FROM dml_co_s;
rollback;

-- test11: Join on distribution key of target table
begin;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.a = dml_co_s.a)foo;
INSERT INTO dml_co_r SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.a = dml_co_s.a ;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test12: Join on the distribution key of target table. Insert Large number of rows
begin;
SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.b <> dml_co_s.b )foo;
INSERT INTO dml_co_s SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.b <> dml_co_s.b ;
SELECT COUNT(*) FROM dml_co_s;
rollback;

-- test13: Join with different column order
begin;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT dml_co_s.a,dml_co_r.b,'text' FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r(b,a,c) SELECT dml_co_s.a,dml_co_r.b,'text' FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test14: Join with Aggregate
begin;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_co_s.a, dml_co_r.b + dml_co_r.a ,'text' FROM dml_co_r, dml_co_s WHERE dml_co_r.b = dml_co_s.b GROUP BY dml_co_s.a,dml_co_r.b,dml_co_r.a)foo;
INSERT INTO dml_co_r SELECT COUNT(*) + dml_co_s.a, dml_co_r.b + dml_co_r.a ,'text' FROM dml_co_r, dml_co_s WHERE dml_co_r.b = dml_co_s.b GROUP BY dml_co_s.a,dml_co_r.b,dml_co_r.a ;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test15: Join with DISTINCT
begin;
SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_s.a = dml_co_r.a)foo;
INSERT INTO dml_co_s SELECT DISTINCT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_s.a = dml_co_r.a ;
SELECT COUNT(*) FROM dml_co_s;
rollback;

-- test16: Insert NULL with Joins
begin;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_co_r.b,'text' FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT NULL,dml_co_r.b,'text' FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test17: Insert and CASE
begin;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_co_p.a) as A, dml_co_p.a as B, dml_co_s.c as C FROM dml_co_p, dml_co_s WHERE dml_co_p.a = dml_co_s.a GROUP BY dml_co_p.a,dml_co_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_co_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_co_p.a) as A, dml_co_p.a as B, dml_co_s.c as C FROM dml_co_p, dml_co_s WHERE dml_co_p.a = dml_co_s.a GROUP BY dml_co_p.a,dml_co_s.c)as x GROUP BY A,B,C;
SELECT COUNT(*) FROM dml_co_r;
rollback;

-- test18: Insert 0 rows
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_co_r;

-- test19: Insert 0 rows
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.a and false;
SELECT COUNT(*) FROM dml_co_r;

-- test20: Negative tests Insert column of different data type
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM ( SELECT ('a')::int, dml_co_r.b,10 FROM dml_co_s WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT ('a')::int, dml_co_r.b,10 FROM dml_co_s WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;

-- test21: Negative test case. INSERT has more expressions than target columns
SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_co_r.* FROM dml_co_r, dml_co_s WHERE dml_co_s.a = dml_co_r.a GROUP BY dml_co_r.a, dml_co_r.b, dml_co_r.c)foo;
INSERT INTO dml_co_s SELECT COUNT(*) as a, dml_co_r.* FROM dml_co_r, dml_co_s WHERE dml_co_s.a = dml_co_r.a GROUP BY dml_co_r.a, dml_co_r.b, dml_co_r.c;
SELECT COUNT(*) FROM dml_co_s;

-- test1: Insert data that satisfy the check constraints
begin;
SELECT COUNT(*) FROM dml_heap_check_s;
SELECT COUNT(*) FROM (SELECT dml_heap_check_s.a, dml_heap_check_s.b, 'text', dml_heap_check_s.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.b )foo;
INSERT INTO dml_heap_check_s SELECT dml_heap_check_s.a, dml_heap_check_s.b, 'text', dml_heap_check_s.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.b ;
SELECT COUNT(*) FROM dml_heap_check_s;
rollback;

-- test2: Negative test - Insert default value violates check constraint
SELECT COUNT(*) FROM dml_heap_check_p;
INSERT INTO dml_heap_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_heap_check_p;

-- test3: Negative test - Insert default value violates NOT NULL constraint
SELECT COUNT(*) FROM dml_heap_check_s;
INSERT INTO dml_heap_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_heap_check_s;

-- test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint
SELECT COUNT(*) FROM dml_heap_check_r;
SELECT COUNT(*) FROM (SELECT dml_heap_check_r.a + 110 , dml_heap_check_r.b, dml_heap_check_r.c, dml_heap_check_r.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a)foo;
INSERT INTO dml_heap_check_r SELECT dml_heap_check_r.a + 110 , dml_heap_check_r.b, dml_heap_check_r.c, dml_heap_check_r.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a AND dml_heap_check_r.b > 10;
SELECT COUNT(*) FROM dml_heap_check_r;

-- test5: Insert with joins where the result tuples violate violates multiple check constraints
SELECT COUNT(*) FROM dml_heap_check_r;
SELECT COUNT(*) FROM (SELECT dml_heap_check_r.a + 110 , 0, dml_heap_check_r.c, NULL FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a)foo;
INSERT INTO dml_heap_check_r SELECT dml_heap_check_r.a + 110 , 0, dml_heap_check_r.c, NULL FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a;
SELECT COUNT(*) FROM dml_heap_check_r;

-- test1: Update data that satisfy the check constraints
begin;
SELECT SUM(d) FROM dml_heap_check_s;
UPDATE dml_heap_check_s SET d = dml_heap_check_s.d * 1 FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.b;
SELECT SUM(d) FROM dml_heap_check_s;
rollback;

-- test2: Negative test: Update data that does not satisfy the check constraints
UPDATE dml_heap_check_s SET a = 100 + dml_heap_check_s.a FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.a AND dml_heap_check_s.a = 99;

-- test3: Negative test - Update violates check constraint(not NULL constraint)
UPDATE dml_heap_check_s SET b = NULL FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.b and dml_heap_check_s.b = 99;

-- test4: Negative test - Update moving tuple across partition .also violates the check constraint
UPDATE dml_heap_check_s SET a = 110 + dml_heap_check_s.a FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.a;

-- test4: Delete with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s USING generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_s;
rollback;

-- test5: Delete with join on distcol
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test6: Delete with join on non-distribution column
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test7: Delete with join on non-distribution column
begin;
SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s USING dml_heap_r WHERE dml_heap_s.a = dml_heap_r.a;
SELECT COUNT(*) FROM dml_heap_s;
rollback;

-- test8: Delete and using
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test9: Delete and using (with no rows)
begin;
TRUNCATE TABLE dml_heap_s;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test10: Delete with join in using
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT dml_heap_r.a FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a)foo WHERE dml_heap_r.a = foo.a;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test11: Delete with join in USING (Delete all rows )
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT 1)foo;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test12: Delete with join in using
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT 1 as t) foo WHERE foo.t = dml_heap_r.a;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test13: Delete with multiple joins
begin;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s,dml_heap_p WHERE dml_heap_r.a = dml_heap_s.b and dml_heap_r.b = dml_heap_p.a;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test14: Delete on table with composite distcol
begin;
SELECT COUNT(*) FROM dml_heap_p;
DELETE FROM dml_heap_p USING dml_heap_r WHERE dml_heap_p.b::int = dml_heap_r.b::int and dml_heap_p.a = dml_heap_r.a;
SELECT COUNT(*) FROM dml_heap_p;
rollback;

-- test15: Delete with PREPARE plan
begin;
SELECT COUNT(*) FROM (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b) foo;
SELECT COUNT(*) FROM dml_heap_r;
PREPARE plan_del as DELETE FROM dml_heap_r WHERE b in (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b);
EXECUTE plan_del;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test16: Delete with PREPARE plan
begin;
SELECT COUNT(*) FROM (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a)foo;
SELECT COUNT(*) FROM dml_heap_s;
PREPARE plan_del_2 as DELETE FROM dml_heap_s WHERE b in (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a);
EXECUTE plan_del_2;
SELECT COUNT(*) FROM dml_heap_s;
rollback;

-- test17: Delete with sub-query
SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s WHERE a = (SELECT dml_heap_r.a FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a);
SELECT COUNT(*) FROM dml_heap_s;

-- test1: Delete from table
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test2: Delete with predicate
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE a > 100;
DELETE FROM dml_heap_pt_s WHERE a > 100;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE a > 100;
rollback;

-- test3: Delete with predicate
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE a is NULL;
DELETE FROM dml_heap_pt_s WHERE a is NULL;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test4: Delete with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s USING generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test5: Delete with join on distcol
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test6: Delete with join on non-distribution column
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test7: Delete with join on non-distribution column
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s USING dml_heap_pt_r WHERE dml_heap_pt_s.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test8: Delete and using
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test9: Delete and using (with no rows)
begin;
TRUNCATE TABLE dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test10: Delete with join in using
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT dml_heap_pt_r.a FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a)foo WHERE dml_heap_pt_r.a = foo.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test11: Delete with join in USING (Delete all rows )
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT 1)foo;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test12: Delete with join in using
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT 1 as t) foo WHERE foo.t = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test13: Delete with multiple joins
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s,dml_heap_pt_p WHERE dml_heap_pt_r.a = dml_heap_pt_s.b and dml_heap_pt_r.b = dml_heap_pt_p.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test14: Delete on table with composite distcol
begin;
SELECT COUNT(*) FROM dml_heap_pt_p;
DELETE FROM dml_heap_pt_p USING dml_heap_pt_r WHERE dml_heap_pt_p.b::int = dml_heap_pt_r.b::int and dml_heap_pt_p.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_p;
rollback;

-- test15: Delete with PREPARE plan
begin;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b) foo;
SELECT COUNT(*) FROM dml_heap_pt_r;
PREPARE plan_del_3 as DELETE FROM dml_heap_pt_r WHERE b in (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b);
EXECUTE plan_del_3;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test16: Delete with PREPARE plan
begin;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a)foo;
SELECT COUNT(*) FROM dml_heap_pt_s;
PREPARE plan_del_4 as DELETE FROM dml_heap_pt_s WHERE b in (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a);
EXECUTE plan_del_4;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test17: Delete with sub-query
SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s WHERE a = (SELECT dml_heap_pt_r.a FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a);
SELECT COUNT(*) FROM dml_heap_pt_s;

-- test4: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test5: Insert with generate_series and NULL
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
INSERT INTO dml_heap_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_heap_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test6: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
TRUNCATE TABLE dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT generate_series(1,10),1;
SELECT * FROM dml_heap_pt_r ORDER BY 1;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test7: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test8: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT *,1 from generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test9: Join on the non-distribution key of target table
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.a)foo;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test10: Join on the distribution key of target table
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_s.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a)foo;
INSERT INTO dml_heap_pt_s SELECT dml_heap_pt_s.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test11: Join on distribution key of target table
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.a = dml_heap_pt_s.a)foo;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.a = dml_heap_pt_s.a ;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test12: Join on the distribution key of target table. Insert Large number of rows
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_s SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.b = dml_heap_pt_s.b ;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test13: Join with different column order
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_s.a,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r(b,a,c) SELECT dml_heap_pt_s.a,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test14: Join with Aggregate
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_heap_pt_s.a, dml_heap_pt_r.b + dml_heap_pt_r.a ,'text' FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b GROUP BY dml_heap_pt_s.a,dml_heap_pt_r.b,dml_heap_pt_r.a)foo;
INSERT INTO dml_heap_pt_r SELECT COUNT(*) + dml_heap_pt_s.a, dml_heap_pt_r.b + dml_heap_pt_r.a ,'text' FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b GROUP BY dml_heap_pt_s.a,dml_heap_pt_r.b,dml_heap_pt_r.a ;
SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
rollback;

-- test15: Join with DISTINCT
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_s.a = dml_heap_pt_r.a)foo;
INSERT INTO dml_heap_pt_s SELECT DISTINCT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_s.a = dml_heap_pt_r.a ;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test16: Insert NULL with Joins
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r SELECT NULL,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test18: Insert 0 rows
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_heap_pt_r;

-- test19: Insert 0 rows
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a and false;
SELECT COUNT(*) FROM dml_heap_pt_r;

-- test20: Negative tests Insert column of different data type
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT ('a')::int, dml_heap_pt_r.b,10 FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r SELECT ('a')::int, dml_heap_pt_r.b,10 FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;

-- test21: Negative test case. INSERT has more expressions than target columns
begin;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a GROUP BY dml_heap_pt_r.a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d)foo;
INSERT INTO dml_heap_pt_s SELECT COUNT(*) as a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a GROUP BY dml_heap_pt_r.a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d;
SELECT COUNT(*) FROM dml_heap_pt_s;
rollback;

-- test22: Insert with join on the partition key
begin;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.* FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
rollback;

-- test24: Negative test - Insert NULL value to a table without default partition
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.b, NULL, dml_heap_pt_r.c FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;

-- test25: Negative test - Insert out of partition range values for table without default partition
SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.b ,dml_heap_pt_r.a + dml_heap_pt_s.a + 100, dml_heap_pt_r.c FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;


-- update_test1: Update to constant value
begin;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
UPDATE dml_heap_pt_r SET c = 'text';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
rollback;

-- update_test2: Update and set distribution key to constant
begin;
SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_pt_s)foo;
UPDATE dml_heap_pt_s SET b = 10;
SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_pt_s)foo;
rollback;

-- update_test3: Update to default value
begin;
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = DEFAULT;
SELECT SUM(a) FROM dml_heap_pt_r;
rollback;

-- update_test4: Update to default value
begin;
SELECT SUM(a) FROM dml_heap_pt_r;
SELECT SUM(b) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET a = DEFAULT, b = DEFAULT;
SELECT SUM(a) FROM dml_heap_pt_r;
SELECT SUM(b) FROM dml_heap_pt_r;
rollback;

-- update_test5: Update and reset the value
begin;
SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_pt_r)foo;
UPDATE dml_heap_pt_r SET a = a;
SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_pt_r)foo;
rollback;

-- update_test6: Update and generate_series
begin;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='n';
UPDATE dml_heap_pt_r SET a = generate_series(1,10), c ='n';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='n';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
rollback;

-- update_test7: Update distcol where join on target table non dist key
begin;
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_r.a + 1 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT SUM(a) FROM dml_heap_pt_r;
rollback;

-- update_test8: Update and from values
begin;
SELECT * FROM dml_heap_pt_r WHERE b = 20 ORDER BY 1;
UPDATE dml_heap_pt_r SET a = v.i + 1 FROM (VALUES(100, 20)) as v(i, j) WHERE dml_heap_pt_r.b = v.j;
SELECT * FROM dml_heap_pt_r WHERE b = 20 ORDER BY 1;
rollback;

-- update_test9: Update with Joins and set to constant value
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = 10;
UPDATE dml_heap_pt_s SET b = 10 FROM dml_heap_pt_r WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = 10;

-- update_test10: Update distcol with predicate in subquery
begin;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_r.a + 1 FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a and dml_heap_pt_s.b in (SELECT dml_heap_pt_s.b + dml_heap_pt_r.a FROM dml_heap_pt_s,dml_heap_pt_r WHERE dml_heap_pt_r.a > 10);
rollback;

-- update_test11: Update with aggregate in subquery
begin;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = (SELECT COUNT(*) FROM dml_heap_pt_s);
SELECT COUNT(*) FROM dml_heap_pt_s;
UPDATE dml_heap_pt_s SET b = (SELECT COUNT(*) FROM dml_heap_pt_s) FROM dml_heap_pt_r WHERE dml_heap_pt_r.a = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = (SELECT COUNT(*) FROM dml_heap_pt_s);
rollback;

-- update_test12: Update and limit in subquery
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
SELECT DISTINCT(b) FROM dml_heap_pt_s ORDER BY 1 LIMIT 1;
UPDATE dml_heap_pt_r SET a = (SELECT DISTINCT(b) FROM dml_heap_pt_s ORDER BY 1 LIMIT 1) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;

-- update_test13: Update multiple columns
begin;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE b is NULL;
SELECT dml_heap_pt_s.a + 10 FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a ORDER BY 1 LIMIT 1;
SELECT * FROM dml_heap_pt_r WHERE a = 1;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_s.a + 10 ,b = NULL FROM dml_heap_pt_s WHERE dml_heap_pt_r.a + 2= dml_heap_pt_s.b;
SELECT * FROM dml_heap_pt_r WHERE a = 11 ORDER BY 1,2;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE b is NULL;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
rollback;

-- update_test14: Update multiple columns
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c='z';
SELECT dml_heap_pt_s.a ,dml_heap_pt_s.b,'z' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b ORDER BY 1,2 LIMIT 1;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET (a,b,c) = (dml_heap_pt_s.a ,dml_heap_pt_s.b,'z') FROM dml_heap_pt_s WHERE dml_heap_pt_r.a + 1= dml_heap_pt_s.b;
SELECT * FROM dml_heap_pt_r WHERE c='z' ORDER BY 1 LIMIT 1;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c='z';
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;

-- update_test15: Update with prepare plans
begin;
PREPARE plan_upd as UPDATE dml_heap_pt_r SET a = dml_heap_pt_s.a +1 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b ;
EXECUTE plan_upd;
rollback;

-- update_test16: Update and case
begin;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 20 ;
UPDATE dml_heap_pt_r SET a = (SELECT case when c = 'r' then MAX(b) else 100 end FROM dml_heap_pt_r GROUP BY c) ;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 20 ;
rollback;

-- update_test17: Negative test - Update with sub-query returning more than one row
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = ( SELECT DISTINCT(b) FROM dml_heap_pt_s ) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT SUM(a) FROM dml_heap_pt_r;

-- update_test18: Negative test - Update with sub-query returning more than one row
SELECT SUM(b) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET b = (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a );
SELECT SUM(b) FROM dml_heap_pt_r;

-- update_test20: Negative test - Update WHERE join returns more than one tuple with different values.
CREATE TABLE dml_heap_pt_u as SELECT i as a, i as b  FROM generate_series(1,10)i;
CREATE TABLE dml_heap_pt_v as SELECT i as a, i as b FROM generate_series(1,10)i;
SELECT SUM(a) FROM dml_heap_pt_v;
UPDATE dml_heap_pt_v SET a = dml_heap_pt_u.a FROM dml_heap_pt_u WHERE dml_heap_pt_u.b = dml_heap_pt_v.b;
SELECT SUM(a) FROM dml_heap_pt_v;

-- update_test21: Update with joins on multiple table
begin;
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_r.b+1 FROM dml_heap_pt_p,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b and dml_heap_pt_r.a = dml_heap_pt_p.b+1;
SELECT SUM(a) FROM dml_heap_pt_r;
rollback;

-- update_test22: Update on table with composite distribution key
-- This currently falls back to planner, even if ORCA is enabled. And planner can't
-- produce plans that update distribution key columns.
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_p SET a = dml_heap_pt_p.b % 2 FROM dml_heap_pt_r WHERE dml_heap_pt_p.b::int = dml_heap_pt_r.b::int and dml_heap_pt_p.a = dml_heap_pt_r.a;
SELECT SUM(a) FROM dml_heap_pt_r;

-- update_test23: Update on table with composite distribution key
begin;
SELECT SUM(b) FROM dml_heap_pt_p;
UPDATE dml_heap_pt_p SET b = (dml_heap_pt_p.b * 1.1)::int FROM dml_heap_pt_r WHERE dml_heap_pt_p.b = dml_heap_pt_r.a and dml_heap_pt_p.b = dml_heap_pt_r.b;
SELECT SUM(b) FROM dml_heap_pt_p;
rollback;

-- test24: Update the partition key and move tuples across partitions( moving tuple to default partition)
begin;
SELECT SUM(a) FROM dml_heap_pt_s;
UPDATE dml_heap_pt_s SET a = dml_heap_pt_r.a + 30000 FROM dml_heap_pt_r WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT SUM(a) FROM dml_heap_pt_s;
rollback;

-- test25: Negative test update partition key (no default partition)
SELECT SUM(b) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET b = dml_heap_pt_r.a + 3000000 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT SUM(b) FROM dml_heap_pt_r;

-- test4: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test5: Insert with generate_series and NULL
begin;
SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_heap_r WHERE c ='text' ORDER BY 1;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test6: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_r;
TRUNCATE TABLE dml_heap_r;
INSERT INTO dml_heap_r SELECT generate_series(1,10);
SELECT * FROM dml_heap_r ORDER BY 1;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test7: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test8: Insert with generate_series
begin;
SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT * from generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test9: Join on the non-distribution key of target table
begin;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test10: Join on the distribution key of target table
begin;
SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT dml_heap_s.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a)foo;
INSERT INTO dml_heap_s SELECT dml_heap_s.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a;
SELECT COUNT(*) FROM dml_heap_s;
rollback;

-- test11: Join on distribution key of target table
begin;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.a = dml_heap_s.a)foo;
INSERT INTO dml_heap_r SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.a = dml_heap_s.a ;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test12: Join on the distribution key of target table. Insert Large number of rows
begin;
SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.b <> dml_heap_s.b )foo;
INSERT INTO dml_heap_s SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.b <> dml_heap_s.b ;
SELECT COUNT(*) FROM dml_heap_s;
rollback;

-- test13: Join with different column order
begin;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT dml_heap_s.a,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r(b,a,c) SELECT dml_heap_s.a,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test14: Join with Aggregate
begin;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_heap_s.a, dml_heap_r.b + dml_heap_r.a ,'text' FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b GROUP BY dml_heap_s.a,dml_heap_r.b,dml_heap_r.a)foo;
INSERT INTO dml_heap_r SELECT COUNT(*) + dml_heap_s.a, dml_heap_r.b + dml_heap_r.a ,'text' FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b GROUP BY dml_heap_s.a,dml_heap_r.b,dml_heap_r.a ;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test15: Join with DISTINCT
begin;
SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_s.a = dml_heap_r.a)foo;
INSERT INTO dml_heap_s SELECT DISTINCT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_s.a = dml_heap_r.a ;
SELECT COUNT(*) FROM dml_heap_s;
rollback;

-- test16: Insert NULL with Joins
begin;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r SELECT NULL,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test17: Insert and CASE
begin;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_heap_p.a) as A, dml_heap_p.a as B, dml_heap_s.c as C FROM dml_heap_p, dml_heap_s WHERE dml_heap_p.a = dml_heap_s.a GROUP BY dml_heap_p.a,dml_heap_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_heap_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_heap_p.a) as A, dml_heap_p.a as B, dml_heap_s.c as C FROM dml_heap_p, dml_heap_s WHERE dml_heap_p.a = dml_heap_s.a GROUP BY dml_heap_p.a,dml_heap_s.c)as x GROUP BY A,B,C;
SELECT COUNT(*) FROM dml_heap_r;
rollback;

-- test18: Insert 0 rows
SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_heap_r;

-- test19: Insert 0 rows
SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a and false;
SELECT COUNT(*) FROM dml_heap_r;

-- test20: Negative tests Insert column of different data type
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM ( SELECT ('a')::int, dml_heap_r.b,10 FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r SELECT ('a')::int, dml_heap_r.b,10 FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;

-- test21: Negative test case. INSERT has more expressions than target columns
SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_heap_r.* FROM dml_heap_r, dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a GROUP BY dml_heap_r.a, dml_heap_r.b, dml_heap_r.c)foo;
INSERT INTO dml_heap_s SELECT COUNT(*) as a, dml_heap_r.* FROM dml_heap_r, dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a GROUP BY dml_heap_r.a, dml_heap_r.b, dml_heap_r.c;
SELECT COUNT(*) FROM dml_heap_s;

-- update_test6: Update and generate_series
begin;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_r WHERE c ='n';
UPDATE dml_heap_r SET a = generate_series(1,10), c ='n';
SELECT COUNT(*) FROM dml_heap_r WHERE c ='n';
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
rollback;

-- update_test7: Update distcol where join on target table non dist key
begin;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = dml_heap_r.a + 1 FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
rollback;

-- update_test8: Update and from values
begin;
SELECT SUM(b) FROM dml_heap_r WHERE b = 20;
UPDATE dml_heap_r SET a = v.i + 1 FROM (VALUES(100, 20)) as v(i, j) WHERE dml_heap_r.b = v.j;
SELECT SUM(b) FROM dml_heap_r WHERE b = 20;
rollback;

-- update_test9: Update with Joins and set to constant value
begin;
SELECT COUNT(*) FROM dml_heap_s WHERE b = 10;
UPDATE dml_heap_s SET b = 10 FROM dml_heap_r WHERE dml_heap_r.b = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_s WHERE b = 10;
rollback;

-- update_test10: Update distcol with predicate in subquery
begin;
UPDATE dml_heap_r SET a = dml_heap_r.a + 1 FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a and dml_heap_s.b in (SELECT dml_heap_s.b + dml_heap_r.a FROM dml_heap_s,dml_heap_r WHERE dml_heap_r.a > 10);
rollback;

-- update_test11: Update with aggregate in subquery
begin;
SELECT COUNT(*) FROM dml_heap_s WHERE b = (SELECT COUNT(*) FROM dml_heap_s);
UPDATE dml_heap_s SET b = (SELECT COUNT(*) FROM dml_heap_s) FROM dml_heap_r WHERE dml_heap_r.a = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_s WHERE b = (SELECT COUNT(*) FROM dml_heap_s);
rollback;

-- update_test12: Update and limit in subquery
begin;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = (SELECT DISTINCT(b) FROM dml_heap_s ORDER BY 1 LIMIT 1) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 1;
rollback;

-- update_test13: Update multiple columns
begin;
SELECT COUNT(*) FROM dml_heap_r WHERE b is NULL;
SELECT dml_heap_s.a + 10 FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a ORDER BY 1 LIMIT 1;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = dml_heap_s.a + 10 ,b = NULL FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_r WHERE b is NULL;
rollback;

-- update_test14: Update multiple columns
begin;
SELECT COUNT(*) FROM dml_heap_r WHERE c='z';
SELECT dml_heap_s.a ,dml_heap_s.b,'z' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b ORDER BY 1,2 LIMIT 1;
UPDATE dml_heap_r SET (a,b,c) = (dml_heap_s.a ,dml_heap_s.b,'z') FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b;
SELECT * FROM dml_heap_r WHERE c='z' ORDER BY 1 LIMIT 1;
SELECT COUNT(*) FROM dml_heap_r WHERE c='z';
rollback;

-- update_test15: Update with prepare plans
begin;
PREPARE plan_upd_2 as UPDATE dml_heap_r SET a = dml_heap_s.a +1 FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b ;
EXECUTE plan_upd_2;
rollback;

-- update_test16: Update and case
begin;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 100 ;
UPDATE dml_heap_r SET a = (SELECT case when c = 'r' then MAX(b) else 100 end FROM dml_heap_r GROUP BY c ORDER BY 1 LIMIT 1) ;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 100 ;
rollback;

-- update_test17: Negative test - Update with sub-query returning more than one row
SELECT SUM(a) FROM dml_heap_r;
UPDATE dml_heap_r SET a = ( SELECT DISTINCT(b) FROM dml_heap_s ) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r;

-- update_test18: Negative test - Update with sub-query returning more than one row
SELECT SUM(b) FROM dml_heap_r;
UPDATE dml_heap_r SET b = (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a );
SELECT SUM(b) FROM dml_heap_r;

-- update_test19: Negative test - Update with aggregates
SELECT SUM(b) FROM dml_heap_r;
UPDATE dml_heap_r SET b = MAX(dml_heap_s.b) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT SUM(b) FROM dml_heap_r;

-- update_test20: Negative test - Update WHERE join returns more than one tuple with different values.
CREATE TABLE dml_heap_u as SELECT i as a, 1 as b  FROM generate_series(1,10)i;
CREATE TABLE dml_heap_v as SELECT i as a ,i as b FROM generate_series(1,10)i;
SELECT SUM(a) FROM dml_heap_v;
UPDATE dml_heap_v SET a = dml_heap_u.a FROM dml_heap_u WHERE dml_heap_u.b = dml_heap_v.b;
SELECT SUM(a) FROM dml_heap_v;

-- update_test21: Update with joins on multiple table
UPDATE dml_heap_r SET a = dml_heap_r.b+1 FROM dml_heap_p,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b and dml_heap_r.a = dml_heap_p.b+1;

-- update_test22: Update on table with composite distribution key
UPDATE dml_heap_p SET a = dml_heap_p.b % 2 FROM dml_heap_r WHERE dml_heap_p.b::int = dml_heap_r.b::int and dml_heap_p.a = dml_heap_r.a;

-- update_test23: Update on table with composite distribution key
UPDATE dml_heap_p SET b = (dml_heap_p.b * 1.1)::int FROM dml_heap_r WHERE dml_heap_p.b = dml_heap_r.a and dml_heap_p.b = dml_heap_r.b;
