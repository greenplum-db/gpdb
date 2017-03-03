-- create row oriented ao table
DROP TABLE IF EXISTS employee;

CREATE TABLE employee(id int, rank int, gender char(1))
WITH (appendonly=true, orientation=row)
DISTRIBUTED BY (id)
PARTITION BY list (gender)
(PARTITION girls VALUES ('F'),
 PARTITION boys VALUES ('M'),
 DEFAULT PARTITION other );
