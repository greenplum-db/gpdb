-- If a cursor's query is SELECT with locking clause, Greenplum
-- should not generate lockrows plannode to avoid lock tuples
-- in segments because cursor is excuted by a reader gang.
create table cursor_initplan(a int, b int);
insert into cursor_initplan select i, i from generate_series(1, 10)i;

create or replace function func_test_cursor1() returns void as
$body$
declare cur cursor for select * from cursor_initplan for update;--
var1 record;--
var2 record;--
begin
open cur;--
fetch cur into var1;--
update cursor_initplan set b = var1.b + 1 where current of cur; end;--
$body$ language 'plpgsql';

create or replace function func_test_cursor2() returns void as
$body$
declare cur refcursor;--
var1 record;--
var2 record;--
begin
open cur for select * from cursor_initplan for update;--
fetch cur into var1;--
update cursor_initplan set b = var1.b + 1 where current of cur; end;--
$body$ language 'plpgsql';

create or replace function func_test_cursor3() returns void as
$body$
declare cur refcursor;--
var1 record;--
var2 record;--
begin
open cur for execute 'select * from cursor_initplan for update';--
fetch cur into var1;--
update cursor_initplan set b = var1.b + 1 where current of cur; end;--
$body$ language 'plpgsql';

select func_test_cursor1();
select func_test_cursor2();
select func_test_cursor3();
