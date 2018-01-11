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

from fnmatch import fnmatch

from gppylib.commands.base import Command, REMOTE
import tinctest

class TINCSystemException(Exception):
    """
    TINCSystemException Execption
    @copyright: Pivotal
    """
    pass

class TINCSystem(object):
    """
    TINCSystem will encompass helper methods for all system related functionalities
    
    @copyright: Pivotal 2013
    """

    @staticmethod
    def drop_caches(remoteHost='localhost'):
        """
        Refresh the system caches
        rc=0, drop caches successfully with no warning(s) or error(s)
        rc=1, drop caches successfully with warning(s) but no error(s)
        rc=2, drop caches with error(s), raise TINCSystemException

        @type remoteHost: string
        @param remoteHost: Host name of the machine where drop caches should be executed
        """
        cmdStr = "echo 'echo 3  > /proc/sys/vm/drop_caches' |sudo -s"
        cmd = Command('drop caches', cmdStr, ctxt=REMOTE, remoteHost=remoteHost)
        cmd.run()
        result = cmd.get_results()
        if result.rc > 1:
            msg = "drop caches failed with rc=%s and stderr=%s" % \
                    (result.rc, result.stderr)
            tinctest.logger.warning(msg)
            raise TINCSystemException(msg)
        tinctest.logger.info("drop caches success with %s" % result)

    @staticmethod
    def make_dirs(abs_dir_path, ignore_exists_error = False, force_create = False):
        """
        A wrapper to os.makedirs() to create the given directory structure with a flag
        to ignore file exists error thrown by os.makedirs(). This will allow us to handle
        scenarios where two concurrent tests are trying to create the same directory.

        @type abs_dir_path: string
        @param abs_dir_path: Absolute path of the directory structure to be created
        
        @type ignore_exists_error: boolean
        @param ignore_exists_error: Ignore and mask file exists error.

        @type force_create: boolean
        @param force_create: Delete the directory if it exists before creating
        """
        if not os.path.isabs(abs_dir_path):
            raise TINCSystemException("Absolute path expected: %s is not an absolute path" %abs_dir_path)

        if force_create and os.path.exists(abs_dir_path):
            shutil.rmtree(abs_dir_path)
            
        try:
            os.makedirs(abs_dir_path)
        except OSError, e:
            if e.errno == 17 and e.strerror.strip() == 'File exists' and ignore_exists_error:
                return
            raise

    @staticmethod
    def substitute_strings_in_file(input_file, output_file, sub_dictionary):
        """
        This definition will substitute all the occurences of sub_dictionary's 'keys' in 
        input_file to dictionary's 'values' to create the output_file.

        @type input_file: string
        @param input_file: Absolute path of the file to read.
        
        @type output_file: string
        @param output_file: Absolute path of the file to create.
        
        @type sub_dictionary: dictionary
        @param sub_dictionary: Dictionary that specifies substitution. Key will be replaced with Value.

        @rtype bool
        @returns True if there is at least one substitution made , False otherwise 
        """
        tinctest.logger.trace_in("input_file: %s ; output_file: %s ; sub_dictionary: %s" % (input_file, output_file, str(sub_dictionary)))
        substituted = False
        with open(output_file, 'w') as output_file_object:
            with open(input_file, 'r') as input_file_object:
                for each_line in input_file_object:
                    new_line = each_line
                    for key,value in sub_dictionary.items():
                        new_line = new_line.replace(key, value)
                    if not each_line == new_line:
                        substituted = True
                    output_file_object.write(new_line)
        tinctest.logger.trace_out("Substituted: %s" %str(substituted))
        return substituted

    @staticmethod
    def substitute_strings_in_directory(input_directory, output_directory, sub_dictionary, file_types=["*"], destination_file_prefix = ""):
        """
        This definition will substitute all the occurences of sub_dictionary's 'keys' in 
        input_directory's files to dictionary's 'values' to create the file in output_directory.

        @type input_directory: string
        @param input_directory: Absolute path of the directory where files will be read.
        
        @type output_file: string
        @param output_file: Absolute path of the directory where files will be created.
        
        @type sub_dictionary: dictionary
        @param sub_dictionary: Dictionary that specifies substitution. Key will be replaced with Value.

        @type file_types: list
        @param file_types: List that specifies the file types to match. Defaults to *.

        @type destination_file_prefix: string
        @param destination_file_prefix: String that will be prefixed to each file in the output_directory.
        """
        tinctest.logger.trace_in("input_directory: %s ; output_directory: %s ; sub_dictionary: %s ; file_types: %s" % (input_directory, output_directory, str(sub_dictionary), ','.join(file_types)))
        
        for filename in os.listdir(input_directory):
            file_match = False
            for match_pattern in file_types:
                if fnmatch(filename, match_pattern):
                    file_match = True
                    break
            if file_match:
                input_file_path = os.path.join(input_directory, filename)
                output_file_path = os.path.join(output_directory, destination_file_prefix + filename)
                TINCSystem.substitute_strings_in_file(input_file_path, output_file_path, sub_dictionary)
        
        tinctest.logger.trace_out()

    @staticmethod
    def copy_directory(input_directory, output_directory, file_types=["*"]):
        """
        This definition will copy all the file types from the input directory to the output directory.

        @type input_directory: string
        @param input_directory: Absolute path of the original directory.
        
        @type output_file: string
        @param output_file: Absolute path of the directory where files will be copied.
        
        @type file_types: list
        @param file_types: List that specifies the file types to match. Defaults to *.
        """
        tinctest.logger.trace_in("input_directory: %s ; output_directory: %s ; file_types: %s" % (input_directory, output_directory, ','.join(file_types)))
        
        for filename in os.listdir(input_directory):
            file_match = False
            for match_pattern in file_types:
                if fnmatch(filename, match_pattern):
                    file_match = True
                    break
            if file_match:
                input_file_path = os.path.join(input_directory, filename)
                output_file_path = os.path.join(output_directory, filename)
                shutil.copy(input_file_path, output_file_path)
        
        tinctest.logger.trace_out()
