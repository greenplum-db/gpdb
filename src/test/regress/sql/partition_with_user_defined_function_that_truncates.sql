-- Given there is a partitioned table
	create table some_partitioned_table_to_truncate
	(
		a integer
	)
	partition by range (a) (
		partition b start (0)
	);

-- And a function that truncates the partitioned table
	CREATE OR REPLACE FUNCTION truncate_a_partition_table() RETURNS VOID AS
	$$
	BEGIN
	    TRUNCATE some_partitioned_table_to_truncate;
	END;
	$$ LANGUAGE plpgsql;

-- When I truncate the table twice
	insert into some_partitioned_table_to_truncate
	       select i from generate_series(1, 10) i;
   	select count(*) from some_partitioned_table_to_truncate;
	select truncate_a_partition_table();
   	select count(*) from some_partitioned_table_to_truncate;	

-- Then I get the same result both times (no rows)
	insert into some_partitioned_table_to_truncate
	       select i from generate_series(1, 10) i;
   	select count(*) from some_partitioned_table_to_truncate;
	select truncate_a_partition_table();
   	select count(*) from some_partitioned_table_to_truncate;	

