-- @Description: Setting up the tables for Exception Handling test for SQL (Exception happening while update is done on table)
-- 
 DROP TABLE IF EXISTS worker;
 CREATE TABLE worker(
  id int NOT NULL ,
  shop_id int NOT NULL ,
  gender character NOT NULL,
  name varchar(32) NOT NULL,
  salary int  CHECK(salary > 0), 
  factor int ,
  bonus int 
  ) ;  
  INSERT INTO worker(id,shop_id, gender, name, salary, factor)
  VALUES (1,1,'f', 'abc', 10000, 10),    
  (2,2,'m', 'xyz', 20000, 0);    
