DROP FUNCTION IF EXISTS gp_aovisimap(oid);
CREATE FUNCTION gp_aovisimap(oid) RETURNS TABLE (tid tid, segno integer, row_num bigint) AS '$GPHOME/lib/postgresql/gp_ao_co_diagnostics.so', 'gp_aovisimap_wrapper' LANGUAGE C STRICT;

DROP FUNCTION IF EXISTS gp_aovisimap_name(text);
CREATE FUNCTION gp_aovisimap_name(text) RETURNS TABLE (tid tid, segno integer, row_num bigint) AS '$GPHOME/lib/postgresql/gp_ao_co_diagnostics.so', 'gp_aovisimap_name_wrapper' LANGUAGE C STRICT;

DROP FUNCTION IF EXISTS gp_aovisimap_entry_name(text);
CREATE FUNCTION gp_aovisimap_entry_name(text) RETURNS TABLE (sego int, first_row_num bigint, hiddentupcount int, bitmap text) AS '$GPHOME/lib/postgresql/gp_ao_co_diagnostics.so', 'gp_aovisimap_entry_name_wrapper' LANGUAGE C STRICT;

DROP FUNCTION IF EXISTS gp_aovisimap_hidden_info_name(text);
CREATE FUNCTION gp_aovisimap_hidden_info_name(text) RETURNS TABLE (segno integer, hidden_tupcount bigint, total_tupcount bigint) AS '$GPHOME/lib/postgresql/gp_ao_co_diagnostics.so','gp_aovisimap_hidden_info_name_wrapper' LANGUAGE C STRICT;

DROP FUNCTION IF EXISTS gp_aoseg_name(text);
CREATE FUNCTION gp_aoseg_name(text) RETURNS TABLE (segno int, eof bigint, tupcount bigint, varblockcount bigint, eofuncompressed bigint, modcount bigint, formatversion smallint, state smallint) AS '$GPHOME/lib/postgresql/gp_ao_co_diagnostics.so','gp_aoseg_name_wrapper' LANGUAGE C STRICT;

DROP FUNCTION IF EXISTS gp_aocsseg_name(text);
CREATE FUNCTION gp_aocsseg_name(text) RETURNS TABLE (gp_tid tid, segno int4, column_num smallint, pysical_segno int, tupcount bigint, eof bigint, eof_uncompressed bigint, modcount bigint, formatversion smallint, state smallint) AS '$GPHOME/lib/postgresql/gp_ao_co_diagnostics.so','gp_aocsseg_name_wrapper' LANGUAGE C STRICT;

