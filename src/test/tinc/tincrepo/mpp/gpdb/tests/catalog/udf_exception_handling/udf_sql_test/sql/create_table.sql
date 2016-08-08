-- @Description Tests with create table failure in function with exception
-- 

-- Create schema needed
DROP SCHEMA IF EXISTS service_management cascade;
DROP SCHEMA IF EXISTS emcas_lce_analytics cascade;
DROP FUNCTION IF EXISTS service_management.column_type_override_gp() CASCADE;

CREATE SCHEMA service_management;
CREATE SCHEMA emcas_lce_analytics;

-- Creating function
CREATE OR REPLACE FUNCTION service_management.column_type_override_gp() RETURNS integer AS
$BODY$
	declare 
		v_source_schema character varying(300);
		v_source_table character varying(300);
	begin
		v_source_schema:='emcas_lce_analytics' ;
		v_source_table:='corrupted_catalog' ;

		-- Issue was not observed if we use schemaname with the newly renamed table in the below query
		execute('ALTER TABLE ' ||v_source_schema || '.'|| v_source_table || ' rename to ' || v_source_table ||'temp');
		-- The below query has raised exception 
	        execute('CREATE TABLE ' ||v_source_schema || '.'|| v_source_table ||'(feed_no char(10),feed_no char(10))WITH ( OIDS=FALSE ) DISTRIBUTED BY ( feed_no )');
		execute('DROP TABLE ' ||v_source_schema || '.'|| v_source_table ||'temp'); 
		RETURN 1;
	-- when exception section has not been used then the cleanup done successfully and the table has been created successfully from the next section 
	 exception
	      WHEN OTHERS THEN
		RAISE NOTICE 'EXCEPTION HIT !!!';
		RETURN 0;   
	end;
$BODY$
LANGUAGE plpgsql VOLATILE SECURITY DEFINER;
 
CREATE TABLE emcas_lce_analytics.corrupted_catalog
(
  feed_no character varying(4000)
)
WITH ( OIDS=FALSE );

SELECT service_management.column_type_override_gp();

DROP TABLE emcas_lce_analytics.corrupted_catalogtemp;

CREATE TABLE emcas_lce_analytics.corrupted_catalogtemp
(
  feed_no character varying(4000)
)
WITH ( OIDS=FALSE );
