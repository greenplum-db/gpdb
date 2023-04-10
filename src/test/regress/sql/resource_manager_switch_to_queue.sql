-- start_ignore
\! gpconfig -c gp_resource_manager -v queue
-- end_ignore

\! echo $?

-- start_ignore
\! gpstop -rai
-- end_ignore

\! echo $?
