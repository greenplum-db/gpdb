-- @Description select with window 
-- 

DROP TABLE IF EXISTS uao_orders;
CREATE TABLE uao_orders(order_id serial , customer_id integer, 
      order_datetime timestamp, order_total numeric(10,2)) with (appendonly=true, orientation=column); 

INSERT INTO uao_orders(customer_id, order_datetime, order_total)
VALUES (1,'2009-05-01 10:00 AM', 500),
    (1,'2009-05-15 11:00 AM', 650),
    (2,'2009-05-11 11:00 PM', 100),
    (2,'2009-05-12 11:00 PM', 5),
       (3,'2009-04-11 11:00 PM', 100),
          (1,'2009-05-20 11:00 AM', 3);

select * from uao_orders order by order_id;
SELECT row_number()  OVER(window_custtime) As rtime_d, 
n.customer_id, lead(order_id) OVER(window_custtime) As cr_num, n.order_id, n.order_total
FROM uao_orders AS n 
WINDOW window_custtime AS (PARTITION BY n.customer_id 
                               ORDER BY n.customer_id, n.order_id, n.order_total)
ORDER BY  1;
