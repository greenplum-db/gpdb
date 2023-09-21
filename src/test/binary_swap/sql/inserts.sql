\c regression

-- Insert into tables generated from src/test/regress
-- Following tables from freeze_aux_tables:
INSERT INTO test_table_heap_with_toast SELECT i, i*2, i*5 FROM generate_series(1, 20)i;
INSERT INTO test_table_ao_with_toast SELECT * FROM test_table_heap_with_toast;
