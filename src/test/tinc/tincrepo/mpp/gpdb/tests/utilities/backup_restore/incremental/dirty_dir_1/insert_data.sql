-- @gucs gp_create_table_random_default_distribution=off
-- AO tables
insert into ao_table23 values ('`12`',200);
insert into ao_table23 values (E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.''\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ',201);
insert into ao_table23 values('^Ma a^Ha
a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[', 202);

-- Heap tables
insert into heap_table1 values (E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.''\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ',201);
insert into heap_table1 values ('`12`',200);
insert into heap_table1 values('^Ma a^Ha
a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[', 202);

-- CO tables
insert into co_table23 values ('`12`',200);
insert into co_table23 values (E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.''\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ',201);
insert into co_table23 values('^Ma a^Ha
a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[', 202);


-- AO part table
insert into ao_part12 values (2, E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.''\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ');
insert into ao_part12 values (3, '^Ma a^Ha
 17 a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[');

insert into co_part12 values (2, E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.''\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ');
insert into co_part12 values (3, '^Ma a^Ha
 17 a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[');

insert into heap_part12 values (2, E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.''\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ');
insert into heap_part12 values (3, '^Ma a^Ha
 17 a^Ka^La^Ma\a\a^@a\ca\sa\ea\$\}\{\]\[');

