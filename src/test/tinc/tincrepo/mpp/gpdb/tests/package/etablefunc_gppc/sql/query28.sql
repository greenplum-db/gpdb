-- TABLE value expression only takes a select subquery as input.
-- Directly using a table (or view) as input to TVE should fail.

select * from transform( TABLE(intable));

select * from transform( TABLE(t1_view order by id scatter by id));
