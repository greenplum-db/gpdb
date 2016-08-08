-- @product_version gpdb: [4.3-4.3.98]
-- Check catalog table pg_type for new type anytable
    \x

    select * from pg_type where typname='anytable';


