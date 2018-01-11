"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import os
import shutil

from gppylib.commands.base import Command

from tinctest import logger

class GpinitsystemError(Exception):
    """
    Gpinitsystem Execption
    @copyright: Pivotal
    """
    pass

class gpinitsystem(object):
    """
    gpinitsystem helper class
    @todo: gpinitsystem for multi-node
    @copyright: Pivotal 2013
    """
    
    source_path = "greenplum_path.sh"
    
    def __init__(self, dir, config_file, segment_dir, mirror=True):
        """
        gpinitsystem init
        @var dir: gpdb directory
        @var config_file: gpdb configuration file
        @var segment_dir: gpdb segment directory
        """
        self.dir = dir
        self.config_file = config_file
        self.segment_dir = segment_dir

        self.datadirs = [os.path.join(self.segment_dir, "master"), 
                         os.path.join(self.segment_dir, "primary") ]
        if mirror:
            self.datadirs.append(os.path.join(self.segment_dir, "mirror"))

    def create_datadir(self):
        """
        Create data directory for GPDB
        @todo: Ability to create remote directories for multi-node system
        """
        for dir in self.datadirs:
            if not os.path.exists(dir):
                os.makedirs(dir)
                
    def delete_datadir(self):
        """
        Delete data directory for GPDB
        @todo: Ability to delete remote directories for multi-node system
        @note: rmtree will fail when dir doesn't exist, thus we check before deleting
        """
        for dir in self.datadirs:
            if os.path.exists(dir):
                shutil.rmtree(dir)
            
    def run(self):
        """
        Run gpinitsystem
        rc=0, gpinitsystem has no warning(s) or error(s)
        rc=1, gpinitsystem has warning(s) but no error(s)
        rc=2, gpinitsystem has error(s)
        """
        self.create_datadir()
        cmd = Command(name='run gpinitsystem', cmdStr='source %s/%s; gpinitsystem -a -c %s' %\
              (self.dir, self.source_path, self.config_file))
        cmd.run()
        result = cmd.get_results()

        if result.rc > 1:
            msg = "stdout:%s\nstderr:%s" % (result.stdout, result.stderr)
            raise GpinitsystemError("gpinitsystem failed (%d): %s" % (result.rc, msg))

        logger.debug("Successfully ran gpinitsystem ...")
