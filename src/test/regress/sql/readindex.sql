CREATE OR REPLACE FUNCTION readindex(oid) RETURNS SETOF record AS '$libdir/indexscan', 'readindex' LANGUAGE C STRICT;
select  gp_segment_id,readindex('pg_compression_compname_index'::regclass) from gp_id;


CREATE OR REPLACE FUNCTION readindex(oid,bool) RETURNS SETOF record AS '$libdir/indexscan', 'readindex' LANGUAGE C STRICT;

select  gp_segment_id,readindex('pg_compression_compname_index'::regclass, true) from gp_id;
select  gp_segment_id,readindex('pg_compression_compname_index'::regclass, false) from gp_id;
