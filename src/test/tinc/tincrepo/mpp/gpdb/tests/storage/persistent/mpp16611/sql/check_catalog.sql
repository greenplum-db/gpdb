SELECT length(description)
FROM pg_description
INNER JOIN pg_proc ON objoid = pg_proc.oid
WHERE proname = 'f';
