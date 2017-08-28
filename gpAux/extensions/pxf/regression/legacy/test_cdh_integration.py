"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

from mpp.gpdb.tests.package.gphdfs import HadoopIntegration
from mpp.gpdb.tests.package.gphdfs import HadoopIntegrationException
from tinctest.lib import local_path, run_shell_command
from mpp.models import SQLTestCase

class CDHIntegrationTest(SQLTestCase):
    """
    Hadoop Integration tests : Greenplum DB with Cloudera hadoop

    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags gphdfs, cdh
    @db_name gphdfs_db
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'
    
    @classmethod
    def setUpClass(cls):
        hadoop_type = "cdh"
        # Url of the CDH distribution
        hadoop_artifact_url = os.getenv("HADOOP_ARTIFACT_URL")
        if not hadoop_artifact_url:
            raise HadoopIntegrationException("HADOOP_ARTIFACT_URL is not set!!")
        # hadoop installation directory; where CDH tar ball is untared
        hadoop_install_dir = os.getenv("HADOOP_INSTALL_PATH", "./installation")
        # path of the hadoop data directories and log directories    
        hadoop_data_dir = os.getenv("HADOOP_DATA_DIR", "/data")
        template_conf_dir = local_path("./configs")
        secure_hadoop = os.getenv("ENABLE_SECURITY", True)
        # provide list of nodes if cluster is multinode - would be used in future, when multinode is enabled
        node_list = os.getenv("NODES_LIST", [])
        gphdfs_connector = os.getenv("GPHDFS_CONNECTOR", "cdh4.1-gnet-1.2.0.0")
        cls.integration = HadoopIntegration(hadoop_type, gphdfs_connector, hadoop_artifact_url, hadoop_install_dir, hadoop_data_dir, template_conf_dir, secure_hadoop, node_list)
        cls.integration.integrate()
        cls.test_failures = False
        super(CDHIntegrationTest, cls).setUpClass()
    
    def get_substitutions(self):
        return self.integration.get_substitutions()

    def tearDown(self):
        self.integration.teardown()
        # if test failures, set the flag
        if self._resultForDoCleanups.failures or self._resultForDoCleanups.errors:
            self.test_failures = True
        super(CDHIntegrationTest, self).tearDown()
        
    @classmethod    
    def tearDownClass(cls):
        # if test failures are there, keep the cluster else clean up the cluster and installation
        if cls.test_failures:
            tinctest.logger.info("Test failures or errors report - Keeping the cluster intact for debugging!!")
        else:
            tinctest.logger.info("Tests passed - Going ahead and deleting the cluster and its configuration files!!")
            cls.integration.teardownclass()
            
