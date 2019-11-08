SHOW server_version;

CREATE LANGUAGE plpythonu;

CREATE FUNCTION someimmutablepythonfunction(foo integer) RETURNS integer IMMUTABLE STRICT AS $$
return 42 + foo
$$ LANGUAGE plpythonu;

CREATE FUNCTION someimmutablepsqlfunction(foo integer) /* in func */
RETURNS integer
LANGUAGE plpgsql IMMUTABLE STRICT AS
$$
BEGIN /* in func */
	return 42 + foo; /* in func */
END /* in func */
$$;
