import time

from gppylib.commands import base
from gppylib.db import dbconn
import pygresql.pg


fts_probe_query = 'SELECT gp_request_fts_probe_scan()'

class ReconfigDetectionSQLQueryCommand(base.SQLCommand):
    """A distributed query that will cause the system to detect
    the reconfiguration of the system"""

    query = "SELECT * FROM gp_dist_random('gp_id')"

    def __init__(self, conn):
        base.SQLCommand.__init__(self, "Reconfig detection sql query")
        self.cancel_conn = conn

    def run(self):
        dbconn.execSQL(self.cancel_conn, self.query)


class SegmentReconfigurer:
    def __init__(self, logger, pool):
        self.logger = logger
        self.pool = pool

    def _trigger_fts_probe(self, dburl):
        conn = pygresql.pg.connect(dburl.pgdb,
                dburl.pghost,
                dburl.pgport,
                None,
                dburl.pguser,
                dburl.pgpass,
                )
        conn.query(fts_probe_query)
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
                if now < start_time + 30:
                    continue
                else:
                    raise
            else:
                cmd = ReconfigDetectionSQLQueryCommand(conn)
                self.pool.addCommand(cmd)
                self.pool.wait_and_printdots(1, False)
                conn.close()
                break
