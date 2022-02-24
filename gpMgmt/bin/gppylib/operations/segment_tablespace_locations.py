#!/usr/bin/env python3
from contextlib import closing
from gppylib.db import dbconn

# get tablespace locations
def get_tablespace_locations(all_hosts, mirror_data_directory):
    """
    to get user defined tablespace locations for all hosts or a specific mirror data directory.
    :param all_hosts: boolean type to indicate if tablespace locations should be fetched from all hosts.
                      Only gpdeletesystem will call it with True
    :param mirror_data_directory: string type to fetch tablespace locations for a specific mirror data directory.
                      Only gpmovemirrors will call it with specific data directory
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

    with closing(dbconn.connect(dbconn.DbURL())) as conn:
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
        res = dbconn.query(conn, tablespace_location_sql)
        for r in res:
            tablespace_locations.append(r)
    return tablespace_locations
