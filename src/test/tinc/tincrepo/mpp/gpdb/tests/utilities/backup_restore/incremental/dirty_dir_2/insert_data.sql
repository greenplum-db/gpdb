-- @gucs gp_create_table_random_default_distribution=off
-- AO tables
insert into ao_table24 values ('`12`',200);
insert into ao_table24 values (E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.\'\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ',201);

-- Heap tables
insert into heap_table1 values (E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.\'\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ',201);
insert into heap_table1 values ('`12`',200);

-- CO tables
insert into co_table24 values ('`12`',200);
insert into co_table24 values (E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.\'\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ',201);

-- AO part table
insert into ao_part12 values (7, E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.\'\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ');
insert into ao_part12 values(7,'^Ma a^Ha
a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[');
-- CO part table
insert into co_part12 values (7, E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.\'\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ');
insert into co_part12 values(7,'^Ma a^Ha
a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[');

-- Heap part table
insert into heap_part12 values (7, E'!@#$%$^%^%&^*&()()(__**#$%$#)+_+_*(^&*^%$$#%#@#@!#!  ?/<>,.\'\"~^:;##@$%^%&|}{[]\t\nASDWEWRERWvjfneve\rns\bjkdwhd ');
insert into heap_part12 values(7,'^Ma a^Ha
a^Ka^La^Ma\aa^@a\ca\sa\ea\$\}\{\]\[');
