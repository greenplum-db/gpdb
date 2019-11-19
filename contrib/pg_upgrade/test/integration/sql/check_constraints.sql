create table users_with_check_constraints (
    id int, 
    name text check (id>=1 and id<2)
);

insert into users_with_check_constraints values (1, 'Joe');
insert into users_with_check_constraints values (2, 'Jane');
