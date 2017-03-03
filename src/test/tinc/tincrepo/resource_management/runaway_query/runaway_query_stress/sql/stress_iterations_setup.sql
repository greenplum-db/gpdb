CREATE TABLE stress_iterations_table(num int, letter text) distributed randomly;

insert into stress_iterations_table values('1', 'a');
insert into stress_iterations_table values('2', 'b');
insert into stress_iterations_table values('3', 'c');
insert into stress_iterations_table values('4', 'd');
insert into stress_iterations_table values('5', 'e');
insert into stress_iterations_table values('6', 'f');
insert into stress_iterations_table values('7', 'g');
