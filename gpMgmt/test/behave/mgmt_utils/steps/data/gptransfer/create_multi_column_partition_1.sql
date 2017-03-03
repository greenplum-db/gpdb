-- list partition type, column key: gender
DROP TABLE IF EXISTS employee;

CREATE TABLE employee(id int, rank int, gender char(1))
DISTRIBUTED BY (id) 
PARTITION BY list (gender, rank)
(PARTITION girls VALUES (('F', 2)),
 PARTITION boys VALUES (('M', 1)), 
 DEFAULT PARTITION other );
