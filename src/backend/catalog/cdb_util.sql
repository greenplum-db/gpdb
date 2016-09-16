-- --------------------------------------------------------------------
--
-- cdb_util.sql
--
-- src/backend/catalog/cdb_util.sql
--
-- --------------------------------------------------------------------


SET log_min_messages = WARNING;

CREATE LANGUAGE PLPGSQL;

------------------------------------------------------------------
-- relation size
-- ------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_rel_size_seg(int, Oid) returns bigint AS
$$
DECLARE result bigint;
BEGIN
    if gp_execution_segment() <> $1 then
         raise exception 'gp_rel_size_seg called on different segment';
    end if;
    select pg_relation_size($2)::bigint into result;
    return result;
END
$$
LANGUAGE PLPGSQL;

CREATE OR REPLACE VIEW gp_rel_size_oid AS
    select c.oid, sum(gp_rel_size_seg(c.gp_segment_id, c.oid))::bigint relsize
    from gp_dist_random('pg_class') c
    group by c.oid;
GRANT SELECT ON gp_rel_size_oid TO PUBLIC;

CREATE OR REPLACE FUNCTION gp_relation_size(Oid) returns bigint AS
    'select relsize from gp_rel_size_oid where oid = $1;'
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION gp_relation_size(text) returns bigint AS
    'select gp_relation_size(pg_objname_to_oid($1));'
LANGUAGE SQL;

-----------------------------------------------------------------
-- ao related.
----------------------------------------------------------------

CREATE TYPE gp_type_aoseg_info AS (
    segmentid Oid,
    segno int,
    colno int,
    eof bigint,
    tupcount bigint,
    varblockcount bigint,
    eofuncompressed bigint
    );

CREATE OR REPLACE FUNCTION gp_aoseg_info(Oid)
RETURNS SETOF gp_type_aoseg_info
AS
$$
DECLARE relst CHAR(1);
DECLARE ncol int;
DECLARE sqlstr TEXT;
DECLARE rec gp_type_aoseg_info; 
BEGIN
    select relstorage into relst from pg_class where oid = $1; 
    if relst = 'a' then
        sqlstr = 'select gp_segment_id as segmentid,
            segno::int as segno,
            -1::int as colno,
            eof::bigint as eof,
            tupcount::bigint as tupcount,
            varblockcount::bigint as varblockcount,
            eofuncompressed::bigint as eofuncompressed
            from
            gp_dist_random(''pg_aoseg.pg_aoseg_' || $1 || ''');';
    else 
        if relst = 'c' then
            select relnatts into ncol from pg_class where oid = $1;
	    sqlstr = 'select gp_segment_id as segmentid,
                segno::int as segno,
                col::int as colno,
                aocsvpinfo_decode(vpinfo, col, 0) as eof,
                tupcount::bigint as tupcount,
                varblockcount::bigint as varblockcount,
                aocsvpinfo_decode(vpinfo, col, 1) as eofuncompressed
                from
                gp_dist_random(''pg_aoseg.pg_aocsseg_' || $1 || '''),
                generate_series(0, ' || (ncol-1)::varchar(100) || ') col;';
        else
            raise exception '% is not a appendonly storage type', relst;
        end if;
    end if;
            
    for rec in execute sqlstr LOOP
        RETURN NEXT rec;
    END LOOP;

    RETURN;
END
$$
LANGUAGE PLPGSQL;

CREATE OR REPLACE FUNCTION gp_aoseg_info(text)
RETURNS SETOF gp_type_aoseg_info
AS
'select * from gp_aoseg_info(pg_objname_to_oid($1));'
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION gp_ao_compression_ratio(Oid)
RETURNS float AS
'select sum(eofuncompressed)::float / sum(eof)::float from gp_aoseg_info($1);'
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION gp_ao_compression_ratio(text)
RETURNS float AS
'select gp_ao_compression_ratio(pg_objname_to_oid($1));'
LANGUAGE SQL;

RESET log_min_messages;
