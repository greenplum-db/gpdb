#!/usr/bin/env python3

import os 

from gppylib import gplog
from gppylib.commands.gp import get_coordinatorport

logger = gplog.get_default_logger()

def validate_pgport(coordinator_data_dir):
    # Check for pgport and make sure that it is the same as the one in postgresql.conf
    env_pgport = os.getenv('PGPORT')
    pgport = get_coordinatorport(coordinator_data_dir) 
    if env_pgport and pgport != int(env_pgport):
        raise Exception('PGPORT value in %s/postgresql.conf does not match PGPORT environment variable' % coordinator_data_dir)
    elif not env_pgport:
        logger.debug('PGPORT not set in environment. PGPORT value in %s/postgresql.conf file will be used' % coordinator_data_dir)
        env_pgport = pgport
    return int(env_pgport)
