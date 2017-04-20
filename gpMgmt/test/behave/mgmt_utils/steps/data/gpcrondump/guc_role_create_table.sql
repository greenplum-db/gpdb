Create table role_guc_table (i int, j int);
Insert into  role_guc_table select i, i+1 from generate_series(1,10) i;
