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

def install_rpm(location, dir, name):
    """
    Install RPM package
    @param location: location of rpm packages
    @param dir: install directory
    @param name: package name
    @return: package name
    """
    package_location = location + '/' + name
    rpm_cmd = 'sudo rpm --force --prefix %s -i %s' % (dir, package_location)
    cmd = Command(name='install rpm', cmdStr=rpm_cmd)
    cmd.run(validateAfter=True)
    
    return get_package_name(package_location)

def delete_rpm(name):
    """
    Delete RPM package
    @param name: package name
    """
    rpm_cmd = 'sudo rpm -e %s' % (name)
    cmd = Command(name='delete rpm', cmdStr=rpm_cmd)
    cmd.run(validateAfter=True)

def get_package_name(name):
    """
    Get RPM package name
    @param dir: directory
    @param name: rpm packagge 
    """
    rpm_cmd = 'rpm -qp %s' % (name)
    cmd = Command(name='get rpm package name', cmdStr=rpm_cmd)
    cmd.run(validateAfter=True)
    result = cmd.get_results()
    return result.stdout
    
def run_gppkg(pgport, gphome, mdd, loc, options="--install"):
    gppkg_cmd = "export PGPORT=%s; export MASTER_DATA_DIRECTORY=%s; source %s/greenplum_path.sh; gppkg %s %s" % (pgport, mdd, gphome, options, loc)
    cmd = Command(name = "Run gppkg", cmdStr = gppkg_cmd)
    cmd.run(validateAfter = True)
    result = cmd.get_results()
    return result.stdout
