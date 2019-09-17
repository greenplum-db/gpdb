--
-- Drop all user defined indexes in the regression suite. We are choosing to not upgrade them at this time:
--
CREATE OR REPLACE FUNCTION get_user_defined_indexes() RETURNS table(index_name name, namespace_name name, database_name name) AS $$
    SELECT relname as index_name, nspname as namespace_name, current_database() as database_name     
        FROM pg_index ind 
        LEFT JOIN pg_class rel 
            ON ind.indexrelid = rel.oid 
            AND relkind IN ('i', '')    
        LEFT JOIN pg_namespace nsp 
            ON rel.relnamespace = nsp.oid     
        WHERE nsp.nspname NOT IN (
            'pg_catalog', 'pg_toast', 'pg_aoseg', 'information_schema'    
        )    
        AND nsp.nspname !~ '^pg_toast';
$$ language sql;

CREATE OR REPLACE FUNCTION drop_user_defined_indexes() RETURNS void AS $$
DECLARE
    user_defined_indexes RECORD;
BEGIN
    FOR user_defined_indexes IN select * from get_user_defined_indexes()
    LOOP
        execute 'drop index if exists ' || 
            quote_ident(user_defined_indexes.namespace_name) || 
            '.' || 
            quote_ident(user_defined_indexes.index_name);
    END LOOP;
END;
$$ language plpgsql;

select * from drop_user_defined_indexes();