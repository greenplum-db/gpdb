-- @Description: The Test executes Procedure  to exercise UDF Exception Handling in various subtransaction condition for debug_dtm_action_target=protocol and debug_dtm_action_segment = 1
-- 
DROP TABLE IF EXISTS employees;
DROP TABLE IF EXISTS shops;
select test_protocol_allseg(1, 2,'f','abc',100,0);
select * from employees;
select * from shops order by id;
