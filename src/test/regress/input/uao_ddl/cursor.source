create schema cursor_@amname@;
set search_path="$user",cursor_@amname@,public;
SET default_table_access_method=@amname@;
begin;
create table uao_tab_cur_1 (a int, b int)
distributed by (a);
insert into uao_tab_cur_1 select i,i from generate_series(1,10)i;
update uao_tab_cur_1 set b = -a;
declare uao_cursor_1 cursor for select * from uao_tab_cur_1 order by a;
delete from uao_tab_cur_1 where a > 5;
fetch next in uao_cursor_1;
fetch next in uao_cursor_1;
fetch next in uao_cursor_1;
fetch next in uao_cursor_1;
fetch next in uao_cursor_1;
fetch next in uao_cursor_1;
close uao_cursor_1;
commit;
begin;
declare uao_cursor_2 cursor for select * from uao_tab_cur_1 order by b;
fetch 1 from uao_cursor_2;
-- Should fail
update uao_tab_cur_1 set b = a where current of uao_cursor_2;
end;
select * from uao_tab_cur_1;
