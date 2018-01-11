#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

"""
  gp_era.py, based on gp_dbid.py
  Copyright (c) EMC/Greenplum Inc 2011. All Rights Reserved. 
"""

import sys, os, stat, re
import hashlib

ERA_RE = re.compile(r"era\s*=\s*(\w+)")

def INFO(msg):
    self = sys._getframe(1).f_locals['self']
    if self.logger: self.logger.info(msg)

def DEBUG(msg):
    self = sys._getframe(1).f_locals['self']
    if self.logger: self.logger.debug(msg)

class GpEraFile:
    """
    Manage the gp_era file.
    """

    def __init__(self, datadir, do_read=False, logger=None):
        """
        Initialize path to gp_era file and reset values.
        Log subsequent activity using specified logger and
        if do_read is True, immediately attempt to read values.
        """
        self.datadir      = datadir
        self.filepath     = os.path.join(self.datadir, 'pg_log', 'gp_era')
        self.era          = None
        self.logger       = logger

        if do_read:
            self.read_gp_era()


    def read_gp_era(self):
        """
        Open the gp_era file and parse its contents.
        """
        DEBUG('%s - read_gp_era' % self.filepath)

        with open(self.filepath) as f:
            self.parse(f)


    def parse(self, f):
        """
        Parse f, looking for matching era ignoring all other lines. 
        Assigns era to observed value, converting matched values from strings to integers.  
        """
        self.era = None
        for line in f:
            line = line.strip()
            DEBUG('parse: %s' % line)

            m = re.match(ERA_RE, line)
            if m:
                self.era = m.group(1)
                DEBUG('match era: %s' % self.era)
                break

        assert self.era is not None


    def format(self, f):
        """
        Generate gp_era contents based on era value
        """
        f.write("# Greenplum Database era.\n")
        f.write("# Do not change the contents of this file.\n")
        f.write('era = %s\n' % self.era)
        INFO('wrote era: %s' % self.era)

 
    def write_gp_era(self):
        """
        Create or replace gp_era file with current values, changing
        permissions of the new file when done and verifying by re-reading
        the file contents and checking the values read match desired values
        """
        DEBUG('%s - write_gp_era' % self.filepath)

        if os.path.exists(self.filepath):
            DEBUG('found existing file')

            os.remove(self.filepath)
            DEBUG('removed existing file')

        DEBUG('opening new file')
        with open(self.filepath, 'w') as f:
            self.format(f)

        DEBUG('setting read only')
        os.chmod(self.filepath, stat.S_IRUSR)  # user read permissions (0400)

        DEBUG('verifying file')
        v = GpEraFile(self.datadir, do_read=True)
        assert self.era == v.era


    def new_era(self, host, port, time):
        """
        Write a new era based on the specified values
        """
        m = hashlib.sha256()
        m.update(str(host))
        m.update(str(port))
        m.update(str(self.datadir))
        self.era = '%s_%s' % (m.hexdigest()[0:16], time)
        self.write_gp_era()


    def set_era(self, era):
        """
        Write a new era based on the specified value
        """
        self.era = era
        self.write_gp_era()


    def end_era(self):
        """
        Remove the era file.
        """
        DEBUG('%s - end_gp_era' % self.filepath)

        if os.path.exists(self.filepath):
            DEBUG('found existing file')

            os.remove(self.filepath)
            DEBUG('removed existing file')


def read_era(datadir, logger):
    """
    """
    erafile = GpEraFile(datadir, logger=logger)
    if os.path.exists(erafile.filepath):
        erafile.read_gp_era()

    if logger: logger.info('era is %s' % erafile.era)
    return erafile.era


#
# trivial unit test
#
if __name__ == '__main__':
    import copy, shutil
    import unittest

    TESTDIR = 'test_gp_era1'

    class MyTestCase(unittest.TestCase):
        def test1(self):
            d = GpEraFile(TESTDIR)
            d2 = copy.copy(d)
            d.era = 10
            d.write_gp_era()
            d2.read_gp_era()
            assert d.era == d2.era
       
    if os.path.exists(TESTDIR): shutil.rmtree(TESTDIR)
    os.mkdir(TESTDIR)
    os.mkdir(os.path.join(TESTDIR, 'pg_log'))
    unittest.main()
    shutil.rmtree(TESTDIR)

