-- Mapped relations can be found by following SQL:
--   SELECT oid,relname,relisshared FROM pg_class WHERE relfilenode=0 AND relisshared=false;

CLUSTER pg_type USING pg_type_oid_index;
