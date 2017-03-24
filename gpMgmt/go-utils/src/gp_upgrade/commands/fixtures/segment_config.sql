-- test rig bootstrap:

-- gp_dump -D -t pg_filespace_entry template1

CREATE TABLE gp_segment_configuration (
  dbid             SMALLINT NOT NULL,
  content          SMALLINT NOT NULL,
  role             "CHAR"   NOT NULL,
  preferred_role   "CHAR"   NOT NULL,
  mode             "CHAR"   NOT NULL,
  status           "CHAR"   NOT NULL,
  port             INTEGER  NOT NULL,
  hostname         TEXT,
  address          TEXT,
  replication_port INTEGER,
  san_mounts       INT2VECTOR
);

INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (1, -1, 'p', 'p', 's', 'u', 15432, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', NULL, NULL);
INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (2, 0, 'p', 'p', 's', 'u', 25432, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', 25438, NULL);
INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (3, 1, 'p', 'p', 's', 'u', 25433, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', 25439, NULL);
INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (4, 2, 'p', 'p', 's', 'u', 25434, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', 25440, NULL);
INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (5, 0, 'm', 'm', 's', 'u', 25435, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', 25441, NULL);
INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (6, 1, 'm', 'm', 's', 'u', 25436, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', 25442, NULL);
INSERT INTO gp_segment_configuration (dbid, content, role, preferred_role, mode, status, port, hostname, address, replication_port, san_mounts) VALUES (7, 2, 'm', 'm', 's', 'u', 25437, 'office-5-231.pa.pivotal.io', 'office-5-231.pa.pivotal.io', 25443, NULL);


-- gp_dump -D -t pg_filespace_entry template1

CREATE TABLE pg_filespace_entry (
  fsefsoid    OID      NOT NULL,
  fsedbid     SMALLINT NOT NULL,
  fselocation TEXT
);

INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 1, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1');
INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 2, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast1/demoDataDir0');
INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 3, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast2/demoDataDir1');
INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 4, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast3/demoDataDir2');
INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 5, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror1/demoDataDir0');
INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 6, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror2/demoDataDir1');
INSERT INTO pg_filespace_entry (fsefsoid, fsedbid, fselocation) VALUES (3052, 7, '/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror3/demoDataDir2');
