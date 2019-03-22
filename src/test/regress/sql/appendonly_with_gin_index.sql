-- Given I have an append-only table
create table users(
  first_name tsvector
) with (appendonly=true);

-- And I have a large amount of data in the table
insert into users
  select to_tsvector(md5(random()::text))
  from generate_series(1, 6000) i;

insert into users values ('John');

-- When I create a GIN index on users
set gp_debug_linger=1;
CREATE INDEX users_search_idx ON users USING gin (first_name);
reset gp_debug_linger;

-- Then I should be able to query the table
select * from users where first_name = 'John';
