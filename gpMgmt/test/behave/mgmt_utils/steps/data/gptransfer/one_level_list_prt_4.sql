-- list partition type, column key: id
DROP TABLE IF EXISTS employee;

CREATE TABLE employee(id int, rank int, gender char(1))
DISTRIBUTED BY (id)
PARTITION BY list (rank)
(PARTITION main VALUES (1),
 PARTITION private VALUES (2),
 DEFAULT PARTITION other );
