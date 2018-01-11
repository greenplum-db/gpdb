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

from mpp.lib.gpstop import GpStop
from mpp.lib.PgHba import PgHba, Entry

'''
Base class for Default Storage Parameters
'''

class DspClass():
    """
    
    """

    def add_user(self):
        """
        @description: Add an entry for each user in pg_hba.conf
        """
        pghba = PgHba()
        for role in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4', 'dsp_role5'):
            new_entry = Entry(entry_type='local', 
                                    database = 'all',
                                    user = role, 
                                    authmethod = 'password')
            pghba.add_entry(new_entry)
            pghba.write()
            # Check if the roles are added correctly    
            res = pghba.search(type='local', 
                                     database='all',
                                     user=role,
                                     authmethod='password')
            if not res:
                raise Exception('The entry is not added to pg_hba.conf correctly')

        gpstop = GpStop()
        gpstop.run_gpstop_cmd(reload=True) # reload to activate the users
        

    def remove_user(self):
        pghba = PgHba()
        for role in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4', 'dsp_role5'):
            entries = pghba.search( user = role )
            for ent in entries:
                ent.delete()
            pghba.write()
        gpstop = GpStop()
        gpstop.run_gpstop_cmd(reload=True) # reload to activate the users
