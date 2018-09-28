CREATE SCHEMA partition_with_user_defined_function;

-- Given there is a partitioned table
	create table partition_with_user_defined_function.some_partitioned_table
	(
		a integer
	)
	partition by range (a) (
		partition b start (0)
	);

-- And a function that queried the partitioned table
	CREATE OR REPLACE FUNCTION partition_with_user_defined_function.query_a_partition_table() RETURNS VOID AS
	$$
	BEGIN
	    PERFORM * FROM partition_with_user_defined_function.some_partitioned_table;
	END;
	$$ LANGUAGE plpgsql;

-- When I call the function twice
	select partition_with_user_defined_function.query_a_partition_table();

-- Then I get the same result both times (no rows)
-- Note: We're using a cached plan that includes a Dynamic Table Scan.
-- Ensure the dynamic table scan information in the cached plan does not get freed.
	select partition_with_user_defined_function.query_a_partition_table();

