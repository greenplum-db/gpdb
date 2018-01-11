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

from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.walrepl import lib as walrepl

class Gpfault(object):
    def run_gpfaultinjector(self, fault_type, fault_name):
        cmd_str = 'gpfaultinjector -s 1 -y {0} -f {1}'.format(
                        fault_type, fault_name)
        cmd = Command(cmd_str, cmd_str)
        cmd.run()

        return cmd.get_results()

    def suspend_at(self, fault_name):
        return self.run_gpfaultinjector('suspend', fault_name)

    def reset_fault(self, fault_name):
        return self.run_gpfaultinjector('reset', fault_name)

    def fault_status(self, fault_name):
        return self.run_gpfaultinjector('status', fault_name)

    def wait_triggered(self, fault_name):

        search = "fault injection state:'triggered'"
        for i in walrepl.polling(10, 3):
            result = self.fault_status(fault_name)
            stdout = result.stdout
            if stdout.find(search) > 0:
                return True

        return False

