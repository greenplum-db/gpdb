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

import tinctest
from tinctest import TINCException

from mpp.models import SQLTestCase
from mpp.lib import mppUtil
from mpp.lib.config import GPDBConfig
from mpp.lib.GPFDIST import GPFDIST

class GpfdistSQLTestCase(SQLTestCase):
    """
    This test case model starts and stops a gpfdist in it's
    setUpClass and tearDownClass fixtures. 
    """
    #: Port number at which to start gpfdist. This finds a free port starting from this one.
    port = 8080
    #: Access to all gpfdist instances started by this test class. Will be available after setUpClass
    gpfdist = []
    #: Data directory relative to the source directory
    data_dir = 'data/'
    #: Specify where gpfdist should be started. Valid values are 'master', 'segments', 'all', 'localhost'
    start_on = 'localhost'

    _start_on_valid_values = ['master', 'segments', 'all', 'localhost']

    @classmethod
    def get_data_dir(cls):
        return os.path.join(cls.get_source_dir(), cls.data_dir)

    @classmethod
    def _form_host_list(cls, start_on, config):
        """
        Find the list of hosts on which gpfdist should be started based on the attribute 'start_on'
        @returns: Hostnames based on start_on attribute
        @rtype: set
        """
        s = set()
        if start_on == 'all':
            s = config.get_hosts()

        if start_on == 'master':
            s.add(config.get_masterhost())

        if start_on == 'segments':
            s = config.get_hosts(segments=True)

        if start_on == 'localhost':
            s.add('localhost')

        return s

    @classmethod
    def setUpClass(cls):
        super(GpfdistSQLTestCase, cls).setUpClass()
        if not cls.start_on in cls._start_on_valid_values:
            raise TINCException("Invalid value specified for attribute 'start_on' : %s" %cls.start_on)

        config = GPDBConfig()
        host_list = cls._form_host_list(cls.start_on, config)

        for host in host_list:
            cls.port = mppUtil.getOpenPort(cls.port, host=host)
            tinctest.logger.info("gpfdist host = {0}, port = {1}".format(host,cls.port))
            gpfdist = GPFDIST(cls.port, host, directory=cls.get_data_dir())
            gpfdist.startGpfdist()
            cls.gpfdist.append(gpfdist)

    @classmethod
    def tearDownClass(cls):
        for gpfdist in cls.gpfdist:
            gpfdist.killGpfdist()
        super(GpfdistSQLTestCase, cls).tearDownClass()

    def get_substitutions(self):
        """
        Provide some default substitutions
        """
        substitutions = {}
        substitutions['@host@'] = 'localhost'
        substitutions['@port@'] = str(self.__class__.port)
        substitutions['@data_dir@'] = self.__class__.get_data_dir()
        return substitutions
        
        

