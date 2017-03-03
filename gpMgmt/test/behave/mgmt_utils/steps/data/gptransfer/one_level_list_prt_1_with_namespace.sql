-- list partition type, column key: gender
DROP TABLE IF EXISTS schema1.employee;
DROP SCHEMA IF EXISTS schema1;
CREATE SCHEMA schema1;

CREATE TABLE schema1.employee(id int, rank int, gender char(1))
DISTRIBUTED BY (id) 
PARTITION BY list (gender)
(PARTITION girls VALUES ('F'),
 PARTITION boys VALUES ('M'), 
 DEFAULT PARTITION other );
