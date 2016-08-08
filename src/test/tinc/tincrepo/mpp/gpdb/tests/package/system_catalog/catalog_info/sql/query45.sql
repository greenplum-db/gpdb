\echo '-- start_ignore'
drop external table mpp7328_check_env;
\echo '-- end_ignore'
CREATE EXTERNAL WEB TABLE mpp7328_check_env (x text)  
 execute E'( env | sort)'  
 on SEGMENT 0          
 format 'text';

select table_name, table_type, is_insertable_into  from information_schema.tables where table_name like 'mpp7328_check_env'; 
