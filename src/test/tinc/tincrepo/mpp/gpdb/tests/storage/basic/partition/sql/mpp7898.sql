--start_ignore
drop table if exists mpp7898_r cascade;
drop table if exists mpp7898_s cascade;
--end_ignore

-- Inheritance parent: mpp7898_s

create table mpp7898_s 
    (a int, b text) 
    distributed by (a);
    
insert into mpp7898_s values 
    (1, 'one');

-- Try to create a table that mixes inheritance and partitioning.
-- Correct behavior: ERROR

-- Partition: mpp7898_r

create table mpp7898_r 
    ( c int, d int) 
    inherits (mpp7898_s)
    partition by range(d) 
    (
        start (0) 
        end (2) 
        every (1)
    );
