-- Tests to validate column projection for various operations

-- Tests for COPY TO (SELECT <...> FROM <aoco_table>) TO ..

CREATE TABLE aoco(i int, j bigint, k int) WITH (appendonly=true, orientation=column);
INSERT INTO aoco SELECT 0, i, 1 FROM generate_series(1, 100000) i;
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads all blocks in the table as all columns are specified.
COPY (SELECT * FROM aoco) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads all blocks in the table as all columns are specified.
COPY (SELECT i,j,k FROM aoco) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads blocks only for cols: i int, j bigint
COPY (SELECT i,j FROM aoco) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads blocks only for cols: i int
COPY (SELECT i FROM aoco) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Tests for COPY <aoco_table> (<col_list>) TO ..

-- Reads all blocks in the table as all columns are implicitly specified.
COPY aoco TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads all blocks in the table as all columns are specified.
COPY aoco (i,j,k) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads blocks only for cols: i int, j bigint
COPY aoco (i,j) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Reads blocks only for cols: i int
COPY aoco (i) TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';


-- Ensure that a dropped column is not scanned.
ALTER TABLE aoco DROP COLUMN j;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, 100, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

COPY aoco TO '/dev/null';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
