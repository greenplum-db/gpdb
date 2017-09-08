-- start_ignore
! gpconfig -c max_statement_mem -v 20GB
! gpconfig -c statement_mem -v 10GB
! gpstop -rai;
-- end_ignore

show max_statement_mem;
show statement_mem;
