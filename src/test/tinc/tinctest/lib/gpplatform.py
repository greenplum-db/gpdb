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
import platform

from gppylib.commands.base import Command

def build_info():
    """
    @description: This is mainly for build integration.
        We build each platform accordingly:
        RHEL5 = RHEL5-x86_64
        RHEL6 = RHEL6-x86_64
        OSX = OSX-i386
        SUSE = SuSE10-x86_64
        SOL = SOL-x86_64
    @return: related installation file name
    @rtype : String
    """
    os_name = get_info()
    if os_name == 'RHEL5':
        url_bin = 'RHEL5-x86_64'
    elif os_name == 'RHEL6':
        url_bin = 'RHEL5-x86_64'
    elif os_name == 'OSX':
        url_bin = 'OSX-i386'
    elif os_name == 'SUSE':
        url_bin = 'SuSE10-x86_64'
    elif os_name == 'SOL':
        url_bin = 'SOL-x86_64'
    else:
        raise Exception("We do not support this platform")
    return url_bin

def get_info():
    """
    Get the current platform
    @return: type platform of the current system
    @rtype : String
    """
    myos = platform.system()
    if myos == "Darwin":
        return 'OSX'
    elif myos == "Linux":
        if os.path.exists("/etc/SuSE-release"):
            return 'SUSE'
        elif os.path.exists("/etc/redhat-release"):
            cmd_str = "cat /etc/redhat-release"
            cmd = Command("run cat for RHEL version", cmd_str)
            cmd.run()
            result = cmd.get_results()
            msg = result.stdout
            if msg.find("5") != -1:
                return 'RHEL5'
            else:
                return 'RHEL6'
    elif myos == "SunOS":
        return 'SOL'
    return None
