-- list partition type, column key: gender
DROP TABLE IF EXISTS employee;

CREATE TABLE employee(id int, rank int, gender char(1))
DISTRIBUTED BY (id) 
PARTITION BY list (rank, gender)
(PARTITION girls VALUES ((2, 'F')),
 PARTITION boys VALUES ((1, 'M')), 
 DEFAULT PARTITION other );
