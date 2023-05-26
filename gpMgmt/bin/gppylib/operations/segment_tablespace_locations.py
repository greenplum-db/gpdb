#!/usr/bin/env python
from gppylib.db import dbconn
from gppylib.db.catalog import RemoteQueryCommand
from gppylib import gplog

logger = gplog.get_default_logger()

# get tablespace locations
def get_tablespace_locations(all_hosts, mirror_data_directory):
    """
    to get user defined tablespace locations for all hosts or a specific mirror data directory.
    :param all_hosts: boolean type to indicate if tablespace locations should be fetched from all hosts.
                      Only gpdeletesystem will call it with True
    :param mirror_data_directory: string type to fetch tablespace locations for a specific mirror data directory.
                      Only gpmovemirrors will call it with a specific data directory
    :return: list of tablespace locations
    """
    tablespace_locations = []
    oid_subq = """ (SELECT *
                    FROM (
                        SELECT oid FROM pg_tablespace
                        WHERE spcname NOT IN ('pg_default', 'pg_global')
                        ) AS _q1,
                        LATERAL gp_tablespace_location(_q1.oid)
                    ) AS t """

    with dbconn.connect(dbconn.DbURL()) as conn:
        if all_hosts:
            tablespace_location_sql = """
                SELECT c.hostname, t.tblspc_loc||'/'||c.dbid tblspc_loc
                FROM {oid_subq}
                    JOIN gp_segment_configuration AS c
                    ON t.gp_segment_id = c.content
                """ .format(oid_subq=oid_subq)
        else:
            tablespace_location_sql = """
                SELECT c.hostname,c.content, t.tblspc_loc||'/'||c.dbid tblspc_loc
                FROM {oid_subq}
                    JOIN gp_segment_configuration AS c
                    ON t.gp_segment_id = c.content
                    AND c.role='m' AND c.datadir ='{mirror_data_directory}'
                """ .format(oid_subq=oid_subq, mirror_data_directory=mirror_data_directory)
        res = dbconn.execSQL(conn, tablespace_location_sql)
        for r in res:
            tablespace_locations.append(r)
    return tablespace_locations


def get_segment_tablespace_locations(primary_hostname, primary_port):
    """
        to get user defined tablespace locations for a specific primary segment. This function is called by
        gprecoverseg --differential to get the tablespace locations by connecting to primary while mirror is down.
        Above function get_tablespace_locations() can't be used as it takes mirror (failed segment) data_dir
        as parameter and it is called before mirrors are moved to new location by gpmovemirrors.
        :param primary_hostname: string type primary hostname
        :param primary_port: int type primary segment port
        :return: list of tablespace locations
        """
    sql = "SELECT distinct(tblspc_loc) FROM ( SELECT oid FROM pg_tablespace WHERE spcname NOT IN " \
          "('pg_default', 'pg_global')) AS q,LATERAL gp_tablespace_location(q.oid);"
    try:
        query = RemoteQueryCommand("Get segment tablespace locations", sql, primary_hostname, primary_port)
        query.run()
    except Exception as e:
        raise Exception("Failed to get segment tablespace locations for segment with host {} and port {} : {}".format(
            primary_hostname, primary_port, str(e)))

    logger.debug("Successfully got tablespace locations for segment with host {}, port {}".
                 format(primary_hostname, primary_port))
    return query.get_results()
