-- start_ignore
drop resource group rg1;
drop resource group rg2;

\! gpconfig -c gp_resource_manager -v queue
-- end_ignore

\! echo $?

-- start_ignore
\! gpstop -rai
-- end_ignore

\! echo $?
