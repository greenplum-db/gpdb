ALTER FUNCTION oracle.regexp_like(text,text) IMMUTABLE;
ALTER FUNCTION oracle.regexp_like(text,text,text) IMMUTABLE;
ALTER FUNCTION oracle.regexp_count(text,text) IMMUTABLE;
ALTER FUNCTION oracle.regexp_count(text,text,integer) IMMUTABLE;
ALTER FUNCTION oracle.regexp_count(text,text,integer,text) IMMUTABLE;
ALTER FUNCTION oracle.regexp_instr(text,text) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_instr(text,text,integer) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_instr(text,text,integer,integer) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_instr(text,text,integer,integer,integer) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_instr(text,text,integer,integer,integer,text) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_instr(text,text,integer,integer,integer,text,integer) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_substr(text,text) IMMUTABLE;
ALTER FUNCTION oracle.regexp_substr(text,text,integer) IMMUTABLE;
ALTER FUNCTION oracle.regexp_substr(text,text,integer,integer) IMMUTABLE;
ALTER FUNCTION oracle.regexp_substr(text,text,integer,integer,text) IMMUTABLE;
ALTER FUNCTION oracle.regexp_substr(text,text,integer,integer,text,integer) IMMUTABLE;
ALTER FUNCTION oracle.regexp_replace(text,text,text) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_replace(text,text,text,integer) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_replace(text,text,text,integer,integer) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_replace(text,text,text,integer,integer,text) IMMUTABLE PARALLEL SAFE;
ALTER FUNCTION oracle.regexp_replace(text,text,text,text) IMMUTABLE PARALLEL SAFE;
