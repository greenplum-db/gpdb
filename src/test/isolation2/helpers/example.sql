create or replace language plpythonu;

create or replace function example() returns text as $$
	return "WITHIN TESTING"
$$ language plpythonu;
