# Test validating that concurrent UDFs inserting data doesn't cause distributed
# deadlocks due to ExclusiveLocks.
#
# The observed issue was that the cached plan revalidation was elevating the
# lock for INSERT to ExclusiveLock. This is required for UPDATE/DELETE but is
# too strict for INSERT where RowExclusiveLock is adequate.
#
# Runs two sessions which in turn insert into two tables in a criss-cross
# pattern to try and induce a deadlock.

setup
{
	CREATE TABLE udf_dl_one (a int, b int) DISTRIBUTED BY (a);
	CREATE TABLE udf_dl_two (a int, b int) DISTRIBUTED BY (a);

	CREATE FUNCTION i_one (val int, dyn bool) RETURNS void AS $$
	BEGIN
		IF (dyn) THEN
			EXECUTE 'INSERT INTO udf_dl_one VALUES ($1, $2)' USING val,val;
		ELSE
			INSERT INTO udf_dl_one VALUES (val, val);
		END IF;
	END;
	$$ LANGUAGE plpgsql;

	CREATE FUNCTION i_two (val int, dyn bool) RETURNS void AS $$
	BEGIN
		IF (dyn) THEN
			EXECUTE 'INSERT INTO udf_dl_two VALUES ($1, $2)' USING val,val;
		ELSE
			INSERT INTO udf_dl_two VALUES (val, val);
		END IF;
	END;
	$$ LANGUAGE plpgsql;
}

teardown
{
	DROP TABLE udf_dl_one;
	DROP TABLE udf_dl_two;

	DROP FUNCTION i_one(int, bool);
	DROP FUNCTION i_two(int, bool);
}

session "s1"
step "s1begin"         { BEGIN; }
step "s1insert_1_stat" { SELECT i_one(10, False); }
step "s1insert_1_dyn"  { SELECT i_one(15, True); }
step "s1insert_2_stat" { SELECT i_two(10, False); }
step "s1insert_2_dyn"  { SELECT i_two(15, True); }
step "s1commit"        { COMMIT; }

session "s2"
step "s2begin"         { BEGIN; }
step "s2insert_2_stat" { SELECT i_two(100, False); }
step "s2insert_2_dyn"  { SELECT i_two(150, True); }
step "s2insert_1_stat" { SELECT i_one(100, False); }
step "s2insert_1_dyn"  { SELECT i_one(150, True); }
step "s2commit"        { COMMIT; }

permutation "s1begin" "s2begin" "s1insert_1_stat" "s2insert_2_stat" "s1insert_2_stat" "s2insert_1_stat" "s1commit" "s2commit"
permutation "s1begin" "s2begin" "s1insert_1_dyn" "s2insert_2_dyn" "s1insert_2_dyn" "s2insert_1_dyn" "s1commit" "s2commit"
permutation "s1begin" "s2begin" "s1insert_1_stat" "s2insert_2_dyn" "s1insert_2_stat" "s2insert_1_dyn" "s1commit" "s2commit"
permutation "s1begin" "s2begin" "s1insert_1_dyn" "s2insert_2_stat" "s1insert_2_dyn" "s2insert_1_stat" "s1commit" "s2commit"
