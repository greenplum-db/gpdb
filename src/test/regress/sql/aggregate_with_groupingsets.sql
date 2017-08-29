--
-- Drop existing table
--
DROP TABLE IF EXISTS foo;

--
-- Create new table foo
--
CREATE TABLE foo(type INTEGER, prod VARCHAR, quantity NUMERIC);

--
-- Insert some values
--
INSERT INTO foo VALUES
  (1, 'Table', 100),
  (2, 'Chair', 250),
  (3, 'Bed', 300);

--
-- Select query with grouping sets
--
SELECT type, prod, sum(quantity) s_quant
FROM
(
  SELECT type, prod, quantity
  FROM foo F1
  LIMIT 3
) F2 GROUP BY GROUPING SETS((type, prod), (prod)) ORDER BY type, s_quant;
