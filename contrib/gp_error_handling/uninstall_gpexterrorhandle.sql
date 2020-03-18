-- Adjust this setting to control where the objects get dropped.
SET search_path = public;

DROP FUNCTION gp_read_persistent_error_log(
    exttable text,
    OUT cmdtime timestamptz,
    OUT relname text,
    OUT filename text,
    OUT linenum int4,
    OUT bytenum int4,
    OUT errmsg text,
    OUT rawdata text,
    OUT rawbytes bytea
);

DROP FUNCTION gp_truncate_persistent_error_log(text);
