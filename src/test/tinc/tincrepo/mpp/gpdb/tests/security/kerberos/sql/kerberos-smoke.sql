-- @product_version gpdb: [4.3.98-]
--
-- Perform a very simple query, just to make sure that we're connected
-- properly. Avoid doing anything complicated that might fail for other
-- reasons.

select 123 as works;
