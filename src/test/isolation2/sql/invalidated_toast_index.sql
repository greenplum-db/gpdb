--
-- Test to make sure the error for an invalidated toast index is sane. This is
-- done as an isolation2 test to make it easy to update the catalogs on all
-- segments.
--

CREATE TABLE toastable_heap(a text, b varchar, c int);

-- Force external storage for toasted columns.
ALTER TABLE toastable_heap ALTER COLUMN a SET STORAGE EXTERNAL;
ALTER TABLE toastable_heap ALTER COLUMN b SET STORAGE EXTERNAL;

-- Insert two values that we know will be toasted.
INSERT INTO toastable_heap VALUES(repeat('a',100000), repeat('b',100001), 1);
INSERT INTO toastable_heap VALUES(repeat('A',100000), repeat('B',100001), 2);

-- start_ignore
--
-- Invalidate the index of the toast table for our relation. Because this is a
-- catalog change, we have to execute it on the coordinator and all segments.
--
-- This is done in an ignore block so it can run correctly with any number of
-- segments.
*U: SET allow_system_table_mods = true;
*U: UPDATE pg_index
        SET indisvalid = false
        FROM pg_class heap
        WHERE indrelid = heap.reltoastrelid
          AND heap.oid = 'toastable_heap'::regclass;
-- end_ignore

-- Fetch, slice, save, and delete should all fail.
SELECT * FROM toastable_heap;
SELECT substr(a, 500, 1) FROM toastable_heap;
UPDATE toastable_heap SET b = repeat('b',100001) WHERE c = 2;
DELETE FROM toastable_heap WHERE c = 1;

-- Don't leave an unusable table in the DB for others to trip over.
DROP TABLE toastable_heap;
