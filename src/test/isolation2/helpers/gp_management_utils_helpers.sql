create or replace language plpythonu;

--
-- pg_basebackup:
--   host: host of the gpdb segment to back up
--   port: port of the gpdb segment to back up
--   slotname: desired slot name to create and associate with backup
--   datadir: destination data directory of the backup
--
-- usage: `select pg_basebackup('somehost', 12345, 'some_slot_name', '/some/destination/data/directory')`
--
create or replace function pg_basebackup(host text, dbid int, port int, slotname text, datadir text) returns text as $$
    import subprocess
    cmd = 'pg_basebackup -h %s -p %d --xlog-method stream -R -D %s --target-gp-dbid %d' % (host, port, datadir, dbid)

    if slotname is not None:
        cmd += ' --slot %s' % (slotname)

    try:
        results = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
    except subprocess.CalledProcessError as e:
        results = str(e) + "\ncommand output: " + e.output

    return results
$$ language plpythonu;
