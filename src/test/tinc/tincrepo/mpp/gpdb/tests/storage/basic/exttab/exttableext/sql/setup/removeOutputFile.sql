-- WET using example protocol demoprot will export data files to data directory on primary segments
-- If NOT cleaned, these files will break data replication (between primary and mirror).
-- Therefore it is necessary to clean up generated data files after each test case run.
--
-- query to construct command to remove the generated data file from data directory
SELECT 'gpssh -h '|| gpsc.hostname ||' rm -f '|| fse.fselocation || '/exttabtest*'
FROM pg_filespace_entry fse, gp_segment_configuration gpsc 
WHERE gpsc.dbid = fse.fsedbid and gpsc.role = 'p' and gpsc.content > -1 
ORDER BY gpsc.hostname, fse.fselocation;
