DROP FUNCTION IF EXISTS spoof_next_xid(xid);
CREATE FUNCTION spoof_next_xid(xid)
    RETURNS xid
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
DROP FUNCTION IF EXISTS get_oldest_xid();
CREATE FUNCTION get_oldest_xid()
    RETURNS xid
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
DROP FUNCTION IF EXISTS get_next_xid();
CREATE FUNCTION get_next_xid()
    RETURNS xid
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
DROP FUNCTION IF EXISTS get_vac_limit();
CREATE FUNCTION get_vac_limit()
    RETURNS xid 
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
DROP FUNCTION IF EXISTS get_warn_limit();
CREATE FUNCTION get_warn_limit()
    RETURNS xid
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
DROP FUNCTION IF EXISTS get_stop_limit();
CREATE FUNCTION get_stop_limit()
    RETURNS xid
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
DROP FUNCTION IF EXISTS get_wrap_limit();
CREATE FUNCTION get_wrap_limit()
    RETURNS xid
	AS '@source@'
    LANGUAGE C IMMUTABLE STRICT;
