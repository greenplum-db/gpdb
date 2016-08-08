-- @Description select into
-- 

DROP TABLE IF EXISTS uao_orders_into;
DROP TABLE IF EXISTS new_uao_orders_into;
CREATE TABLE uao_orders_into(order_id serial , customer_id integer, 
      order_datetime timestamp, order_total numeric(10,2)) with (appendonly=true); 

INSERT INTO uao_orders_into(customer_id, order_datetime, order_total)
VALUES (1,'2009-05-01 10:00 AM', 500),
    (1,'2009-05-15 11:00 AM', 650),
    (2,'2009-05-11 11:00 PM', 100),
    (2,'2009-05-12 11:00 PM', 5),
       (3,'2009-04-11 11:00 PM', 100),
          (1,'2009-05-20 11:00 AM', 3);

select * into new_uao_orders_into from uao_orders_into where order_total > 500 order by order_id;

select * from new_uao_orders_into;
