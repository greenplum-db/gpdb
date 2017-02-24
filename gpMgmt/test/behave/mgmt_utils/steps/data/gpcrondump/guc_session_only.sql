set gp_default_storage_options="orientation=column, appendonly=true, compresstype=none";
Create table session_guc_table (i int, j int);
Insert into  session_guc_table select i, i+1 from generate_series(1,10) i;