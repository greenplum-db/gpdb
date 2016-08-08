CREATE or replace FUNCTION sec_definer_create_test() RETURNS void AS $$                                                                                                           
BEGIN                                                                                                                                                                              
         RAISE NOTICE 'Creating table';                                                                                                                                                   BEGIN                                                                                                                                                                       
		 execute 'create temporary table wmt_toast_issue_temp (name varchar, address varchar) distributed randomly';                                                                        
END;                                                                                                                                                                            
         RAISE NOTICE 'Table created';                                                                                                                                               
END;  
$$ LANGUAGE plpgsql SECURITY DEFINER;

create role sec_definer_role with login ;

grant execute on function sec_definer_create_test() to sec_definer_role;

set role sec_definer_role;

select sec_definer_create_test() ;

select count(*) from pg_tables where schemaname like 'pg_temp%';
