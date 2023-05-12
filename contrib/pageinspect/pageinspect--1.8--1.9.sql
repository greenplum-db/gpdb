/* contrib/pageinspect/pageinspect--1.8--1.9.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pageinspect UPDATE TO '1.9'" to load this file. \quit

--
-- brin_metapage_info()
--
DROP FUNCTION brin_metapage_info(IN page bytea, OUT magic text,
                                 OUT version integer, OUT pagesperrange integer, OUT lastrevmappage bigint);
CREATE FUNCTION brin_metapage_info(IN page bytea, OUT magic text,
                                   OUT version integer, OUT pagesperrange integer, OUT lastrevmappage bigint,
                                   /* GPDB specific for AO/CO tables */
                                   OUT isAo boolean,
                                   OUT firstrevmappages bigint[],
                                   OUT lastrevmappages bigint[],
                                   OUT lastrevmappagenums bigint[])
AS 'MODULE_PATHNAME', 'brin_metapage_info'
LANGUAGE C STRICT PARALLEL SAFE;

--
-- brin_revmap_chain()
--
CREATE FUNCTION brin_revmap_chain(IN indexrelid regclass, IN segno int)
    RETURNS bigint[]
AS 'MODULE_PATHNAME', 'brin_revmap_chain'
    LANGUAGE C STRICT PARALLEL SAFE;
