import time

from gppylib.commands import base
from gppylib.db import dbconn
import pygresql.pg


FTS_PROBE_QUERY = 'SELECT gp_request_fts_probe_scan()'

class SegmentReconfigurer:
    def __init__(self, logger, worker_pool, timeout):
        self.logger = logger
        self.pool = worker_pool
        self.timeout = timeout

    def _trigger_fts_probe(self, dburl):
        conn = pygresql.pg.connect(dburl.pgdb,
                dburl.pghost,
                dburl.pgport,
                None,
                dburl.pguser,
                dburl.pgpass,
                )
        conn.query(FTS_PROBE_QUERY)
        conn.close()

    def reconfigure(self):
        # issue a distributed query to make sure we pick up the fault
        # that we just caused by shutting down segments
        self.logger.info("Triggering segment reconfiguration")
        dburl = dbconn.DbURL()
        self._trigger_fts_probe(dburl)
        start_time = time.time()
        while True:
            try:
                # this issues a BEGIN
                # so the primaries'd better be up
                conn = dbconn.connect(dburl)
            except Exception as e:
                now = time.time()
                if now < start_time + self.timeout:
                    continue
                else:
                    raise RuntimeError("Mirror promotion did not complete in {0} seconds.".format(self.timeout))
            else:
                conn.close()
                break
