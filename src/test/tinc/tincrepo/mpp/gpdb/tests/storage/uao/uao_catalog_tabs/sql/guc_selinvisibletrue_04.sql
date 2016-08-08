select * from uao_tab_test2;
update uao_tab_test2 set j = 'test_update' where i = 1;
select * from uao_tab_test2;
set gp_select_invisible = true;
select * from uao_tab_test2;
