-- start_ignore
drop function if exists cacheao();
-- end_ignore

create function cacheao() returns void as
$$
begin
	drop table if exists aocompressed;
        create table aocompressed(id int, username text) WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1, blocksize=32768) distributed randomly;
        insert into aocompressed(id, username) values(1, 'user1');
        insert into aocompressed(id, username) values(2, 'user2');
end;
$$ language plpgsql modifies sql data;

select 1 as test where cacheao() is null;

select * from aocompressed order by id;

select 1 as test where cacheao() is null;
