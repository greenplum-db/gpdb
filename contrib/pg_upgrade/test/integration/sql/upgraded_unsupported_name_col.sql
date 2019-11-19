SELECT * FROM table_with_name_column;
SELECT attname, atttypid::regtype FROM pg_attribute WHERE attrelid='table_with_name_column'::regclass and attname='a_name';
