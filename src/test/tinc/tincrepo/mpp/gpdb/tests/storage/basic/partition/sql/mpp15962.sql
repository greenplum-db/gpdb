--start_ignore
Drop table if exists pt_indx_tab  cascade;
--end_ignore
create table pt_indx_tab (c1 integer, c2 int, c3 text) partition by range (c1) (partition A start (integer '0') end (integer '5') every (integer '1'));

--start_ignore
Drop index if exists pt_indx_drop cascade;
--end_ignore

create unique index pt_indx_drop on pt_indx_tab(c1);

\d+ pt_indx_tab
\d pt_indx_tab_1_prt_a_1

drop index pt_indx_drop;

\d pt_indx_tab_1_prt_a_1


