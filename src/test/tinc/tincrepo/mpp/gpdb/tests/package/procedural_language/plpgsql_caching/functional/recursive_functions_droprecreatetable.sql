-- start_ignore
drop function if exists recursive(int);
drop table if exists aorecursive;
-- end_ignore

create table aorecursive(id int, username text) 
WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1, blocksize=32768) 
DISTRIBUTED randomly;

insert into aorecursive(id, username) values(1, 'user1');

create function recursive(i int) returns integer as
$$
declare
	rowcnt integer;
begin
        select count(*) into rowcnt from aorecursive;        
        if rowcnt = 0 then
           insert into aorecursive(id, username) values(i, 'user' || i);
           return 1;
        else
           drop table aorecursive;
           create table aorecursive(id int, username text)
           WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1, blocksize=32768)
           DISTRIBUTED randomly;
           return recursive(i+1);
        end if;
end;
$$ language plpgsql modifies sql data;

set gp_resqueue_memory_policy=none;
set gp_plpgsql_clear_cache_always=on;

select * from aorecursive order by id;

select 1 as test where recursive(1) is null;

