
CREATE OR REPLACE FUNCTION test_sql_allseg() RETURNS VOID AS
$$
DECLARE tfactor int default 0;
BEGIN
  BEGIN
  DROP TABLE IF EXISTS employees;

  CREATE TABLE employees (
  id int NOT NULL ,
  shop_id int NOT NULL ,
  gender character NOT NULL,
  name varchar(32) NOT NULL,
  salary int  CHECK(salary > 0), 
  factor int ,
  bonus int 
  ) ;  
  INSERT INTO employees (id,shop_id, gender, name, salary, factor)
  VALUES (1,1,'f', 'abc', 10000, 10),    
  (2,2,'m', 'xyz', 20000, 0);    
  END;
  BEGIN
  DROP TABLE IF EXISTS shops;

  CREATE TABLE shops (
  id serial PRIMARY KEY,
  shop varchar(32)
  );  

  INSERT INTO shops (shop)
  VALUES ('san francisco'), ('New York');
  END;
  BEGIN
      BEGIN
          BEGIN
          update employees set bonus = salary/10;
          EXCEPTION
              WHEN OTHERS THEN
                   RAISE NOTICE 'catching the exception ...3';
          END;
      EXCEPTION
       WHEN OTHERS THEN
          RAISE NOTICE 'catching the exception ...2';
      END;
  EXCEPTION
     WHEN OTHERS THEN
          RAISE NOTICE 'catching the exception ...1';
  END;

END;
$$
LANGUAGE plpgsql;
