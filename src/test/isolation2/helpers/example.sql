create or replace language plpython3u;

create or replace function example() returns text as $$
	return "WITHIN TESTING"
$$ language plpython3u;
