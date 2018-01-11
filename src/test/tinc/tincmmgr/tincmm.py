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

import inspect
import re
import tokenize
from collections import namedtuple, defaultdict
import shutil

import tinctest

from tinctest.suite import TINCTestSuite
from mpp.models import SQLTestCase

from tincmmgr import TINCMMException

"""
This comment describes the overall flow:
The caller (almost always a main-type function) will create TINCTestSuiteMM object.
TINCTestSuiteMM requires a valid TINCTestSuite object.
For each TINCTestCase in TINCTestSuite, it finds a class file and a method file.
For regular TINCTestCase, the class file and the method file is the same.
For SQLTestCase, the method file is the SQL file.
For each found file, it creates a valid FileHandler object.
PythonFileHandler handles python files and SQLFileHandler handles sql files.

The FileHandler objects are responsible for parsing the files (python or sql), and finding
all the docstring present in the files. These docstrings could belong to class and/or methods.

TINCTestSuiteMM doesn't know how this information is stored.
It just uses the API provided by FileHandler classes to request metadata dictionary of the docstring,
updating the docstring, or updating the files themselves with the given metadata dictionary.

For each TINCTestCase, TINCTestSuiteMM also creates TINCTestCaseMM.
TINCTestCaseMM keeps the metadata dictionary (class and method) for a particular test case updated.
It provides API for each type of query (INSERT, UPDATE, DELETE, SELECT).

TINCTestSuiteMM is the main driver to call the above methods in TINCTestCaseMM to keep all
the metadata dictionaries up-to-date. It also calls all the FileHandler methods to keep the files
themselves up-to-date.
"""

class TINCTestSuiteMM(object):
    """
    TINCTestSuiteMM takes in a TINCTestSuite, and allows an option to apply queries to all the tests within.
    """
    def __init__(self, test_suite):
        """
        The constructor stores the following instance variables:
        _tests_mm - List of TINCTestCaseMM objects. One object exists for each TINCTestCase.
        file_name_handlers_dict - Dict of FileHandlers (PythonFileHandler or SQLFileHandler). Key is file path. Value is FileHandler object.
        class_name_metadata_dict - Dict of Metadata dictionary. Key is full class name. Value is metadata dictionary object.
        
        @param test_suite: TINCTestSuite that this MM should work upon
        @type test_suite: L{TINCTestSuite}
        """
        # Flatten the test suite first
        flat_test_suite = TINCTestSuite()
        test_suite.flatten(flat_test_suite)
        self.test_suite = flat_test_suite
        
        self._tests_mm = []
        
        # dictionary of file handler objects corresponding to the files where the tests are located
        # This could either be a sql file handler or a (python file handler + sql file handler) for
        # the tests
        self.file_name_handlers_dict = {}
        
        # dictionary of class metadata corresponding to a completely qualified class name. Multiple tests belonging
        # to the same class will have the same class metadata dict that it should operate upon
        self.class_name_metadata_dict = {}
        
        # Construct tinc test case mm for every test in the test suite
        for test in self.test_suite:
            # find the file
            test_file = inspect.getsourcefile(inspect.getmodule(test))
            if not test_file in self.file_name_handlers_dict:
                self.file_name_handlers_dict[test_file] = PythonFileHandler(test_file)
            
            full_class_name = test.full_name.rsplit('.', 1)[0]
            
            if not full_class_name in self.class_name_metadata_dict:
                self.class_name_metadata_dict[full_class_name] = self.file_name_handlers_dict[test_file].get_metadata_dictionary(method_name = None, class_name=test.class_name)
                            
            class_metadata_dict = self.class_name_metadata_dict[full_class_name]
            
            if self._is_sql_test(test):
                if not test._original_sql_file in self.file_name_handlers_dict:
                     self.file_name_handlers_dict[test._original_sql_file] = SQLFileHandler(test._original_sql_file)
                method_metadata_dict = self.file_name_handlers_dict[test._original_sql_file].get_metadata_dictionary()
            else:
                method_metadata_dict = self.file_name_handlers_dict[test_file].get_metadata_dictionary(class_name=test.class_name, method_name=test.method_name)
            
            self._tests_mm.append(TINCTestCaseMM(test, class_metadata_dict, method_metadata_dict))
    
    def _is_sql_test(self, test_object):
        if isinstance(test_object, SQLTestCase) and \
           test_object.__dict__.has_key(test_object.__dict__['_testMethodName']) and \
           hasattr(test_object.__dict__[test_object.__dict__['_testMethodName']], '__call__'):
            return True
        else:
            return False
    
    def apply_queries(self, query_handler):
        """
        Apply metadata queries to all test case mm objects in this test suite
        """
        # First apply queries to all test case mm objects that would have updated / created class and method metadata dictionaries
        select_only = True
        
        for result in query_handler.query_result_list:
            print "-" * 80
            print "query: ", result.string, "\n"
            print "test_name|metadata|class_value|method_value\n"
            for test_case_mm in self:
                if result.query_kind.lower() == 'select':
                    test_case_mm.select_metadata(result.metadata)
                elif result.query_kind.lower() == 'insert':
                    test_case_mm.insert_metadata(result.metadata, result.value)
                    select_only = False
                elif result.query_kind.lower() == 'update':
                    test_case_mm.update_metadata(result.metadata, result.value)
                    select_only = False
                elif result.query_kind.lower() == 'delete':
                    test_case_mm.delete_metadata(result.metadata, result.value)
                    select_only = False
            print "-" * 80
            TINCTestCaseMM.updated_classes = []
        if select_only:
            # No update of docstring or file needed
            return
        
        # For every test case mm, update the class and method metadata dictionaries in the corresponding file handlers
        for test_case_mm in self:
            class_metadata_dict = test_case_mm.class_metadata
            method_metadata_dict = test_case_mm.method_metadata
            
            # Find the test case file for this test case mm
            filename = inspect.getsourcefile(inspect.getmodule(test_case_mm.tinc_test_case))
            file_handler = self.file_name_handlers_dict[filename]
            
            # First update the class docstring with the modified class metadata dictionary in the file handler
            file_handler.update_docstring(class_name=test_case_mm.tinc_test_case.class_name, method_name=None, metadata_dictionary=class_metadata_dict)
            
            # update the method docstring with the modified method metadata dictionary in the file handler
            # TODO: Handle SQLTestCase here
            if self._is_sql_test(test_case_mm.tinc_test_case):
                file_handler = self.file_name_handlers_dict[test_case_mm.tinc_test_case._original_sql_file]
                file_handler.update_docstring(metadata_dictionary=method_metadata_dict)
            else:
                file_handler.update_docstring(class_name=test_case_mm.tinc_test_case.class_name, method_name=test_case_mm.tinc_test_case.method_name, metadata_dictionary=method_metadata_dict)
        
        # Write every file handler back if there are no exceptions
        for filename in self.file_name_handlers_dict:
            self.file_name_handlers_dict[filename].update_file()
    
    def __iter__(self):
        return iter(self._tests_mm)

class _ParserHelp(object):
    """
    Helper class to check if a line is a test method, test class, docstring, etc.
    """
    @staticmethod
    def find_test_method(line):
        search_object = re.search(r'def\s+(.*)\s*\(.*self.*\).*\:', line)
        if search_object:
            return search_object.group(1).strip()
        else:
            return None
        
    @staticmethod
    def find_test_class(line):
        search_object = re.search(r'class\s+(.*)\s*\(.*TestCase.*\).*\:', line)
        if search_object:
            return search_object.group(1).strip()
        else:
            return None
    
    @staticmethod
    def find_docstring_quotes(line):
        if re.search(r'\'\'\'', line) or re.search(r'\"\"\"', line):
            return True
        else:
            return False

    @staticmethod
    def find_spaces(line):
        return (len(line) - len(line.lstrip(' ')))

class PythonFileHandler(object):
    """
    This class handles tokenizing a python file, and provides services with storing all the docstring information, updating the docstring,
    updating the original python file, or getting a metadata dictionary from a given docstring.
    """

    # Tuple to keep track of all docstring properties:
    # class_name: class that docstring belongs to.
    # method_name: method that docstring belongs to. If it is class docstring, method_name is None
    # offset: offset (column number) of the definition (NOT docstring)
    # original_docstring: the original docstring as found in the file.
    # new_docstring: the new docstring that has the latest information.
    # token_idx: The index at which the docstring token is found. If no docstring found, then this will be the index at which the new docstring should be inserted
    docstring_tuple = namedtuple('docstring_tuple', 'class_name method_name offset original_docstring new_docstring token_idx')
    def __init__(self, filename):
        """
        This constructor requires a filename to parse and store all its information. The information is stored in a list of named tuples.
        
        @type filename: string
        @param filename: Absolute path to the python file that needs docstring handling.
        """
        self.filename = filename
        
        # List of docstring namedtuple
        self.docstring_tuples = []
        self.parse_file()
    
    def parse_file(self):
        """
        This method will parse through the python file and populate docstring_tuples. Even if a class/method doesn't have docstring, it would be stored in tuples list.
        """
        try:
            file_object = open(self.filename, 'r')
            file_object.close()
        except:
            raise TINCMMException("Cannot open file %s" % self.filename)
        
        tinctest.logger.debug("Parsing file %s" %self.filename)
        with open(self.filename, 'r') as file_object:
            current_class_name = None
            current_method_name = None
            current_docstring_tuple = None
            check_indent = False
            check_dedent = False
            check_docstring = False
            token_idx = -1
            for (token_type, token_string, token_start_position, token_end_position, token_full_line) in tokenize.generate_tokens(file_object.next):
                token_idx += 1
                
                # Look for current class name
                if token_type == tokenize.NAME and token_string == "class":
                    class_name = _ParserHelp.find_test_class(token_full_line)
                    if class_name:
                        tinctest.logger.debug("Found class definition: %s" %class_name)
                        # Add any previously constructued tuple
                        if current_docstring_tuple:
                            self.docstring_tuples.append(current_docstring_tuple)
                        current_class_name = class_name
                        current_method_name = None
                        check_indent = True
                        row, column = token_start_position
                        current_docstring_tuple = PythonFileHandler.docstring_tuple(class_name=current_class_name, method_name=current_method_name, offset=column, original_docstring=None, new_docstring=None, token_idx=None)
                
                # Check if class doc string is not found
                if check_docstring and (current_class_name and not current_method_name) and ((token_type != tokenize.STRING) or (token_type == tokenize.STRING and not _ParserHelp.find_docstring_quotes(token_full_line))):
                    # This is the point where the class docstring should be inserted if any
                    tinctest.logger.debug("FileHandler %s: Did not find class docstring for: %s | %s" %(self.filename, current_docstring_tuple.class_name, current_docstring_tuple.method_name))
                    current_docstring_tuple = current_docstring_tuple._replace(token_idx=token_idx)
                    check_docstring = False
                    check_indent = False
                
                # Check for method definition 
                if ((not check_docstring) and current_class_name and token_type == tokenize.NAME and token_string == "def"):
                    method_name = _ParserHelp.find_test_method(token_full_line)
                    tinctest.logger.debug("Found method definition: %s.%s" %(current_class_name, method_name))
                    if method_name and class_name:
                        if current_docstring_tuple:
                            self.docstring_tuples.append(current_docstring_tuple)
                        current_method_name = method_name
                        check_indent = True
                        row, column = token_start_position
                        current_docstring_tuple = PythonFileHandler.docstring_tuple(class_name=current_class_name, method_name=current_method_name, offset=column, original_docstring=None, new_docstring=None, token_idx=None)
                
                # Look for indent
                if check_indent and token_type == tokenize.INDENT:
                    tinctest.logger.debug("Found indent after definition: %s | %s" %(current_class_name, current_method_name))
                    check_docstring = True
                    check_dedent = True
                    continue
                
                # Terminate docstring check if the token following a definition is not a docstring token
                if check_docstring and current_class_name and current_method_name and ((token_type != tokenize.STRING) or (token_type == tokenize.STRING and not _ParserHelp.find_docstring_quotes(token_full_line))):
                    # Token following a defintion is not a docstring token
                    tinctest.logger.debug("FileHandler %s: Did not find method docstring for: %s | %s" %(self.filename, current_docstring_tuple.class_name, current_docstring_tuple.method_name))
                    check_docstring = False
                    current_docstring_tuple = current_docstring_tuple._replace(token_idx=token_idx)
                
                # Look for docstring after a definition of class or a method
                if check_docstring and token_type == tokenize.STRING and _ParserHelp.find_docstring_quotes(token_full_line):
                    tinctest.logger.debug("FileHandler %s: Found a docstring for: %s | %s" %(self.filename, current_docstring_tuple.class_name, current_docstring_tuple.method_name))
                    tinctest.logger.debug("Docstring found: %s" %token_string)
                    # This is docstring!
                    current_docstring_tuple = current_docstring_tuple._replace(original_docstring=token_string)
                    current_docstring_tuple = current_docstring_tuple._replace(token_idx=token_idx)
                    check_docstring = False
                
                # Look for dedent
                if check_dedent and token_type == tokenize.DEDENT:
                    start_position_row, start_position_column = token_start_position
                    if start_position_column == 0:
                        # New class...
                        current_class_name = None
                        current_method_name = None
                    else:
                        # New method...
                        current_method_name = None
                    # Reset check_docstring to start over
                    check_indent = False
                    check_dedent = False
                    check_docstring = False
                
                # Look for stray methods
                elif token_type == tokenize.NAME and token_string == "def":
                    method_name = _ParserHelp.find_test_method(token_full_line)
            # Add last def/class's tuple
            if current_docstring_tuple:
                self.docstring_tuples.append(current_docstring_tuple)
    
    def update_file(self, file_to_write = None):
        """
        This method will update the original python file with the new docstring that is stored in docstring_tuples.
        If file_to_write is specified, the original python file will be left as is, and file_to_write will be used
        with new docstring.
        
        @type file_to_write: string
        @param file_to_write: Absolute path to the file to use to write back the original file with new docstring.
                              Defaults to None. If None, the original file will be updated.
        """
        if not file_to_write:
            file_to_write = self.filename
        
        with open(self.filename, 'r') as file_object:
            token_list = []
            for (token_type, token_string, token_start_position, token_end_position, token_full_line) in tokenize.generate_tokens(file_object.next):
                token_list.append((token_type, token_string))
        
        update_needed = False
        tokens_inserted = 0
        for docstring_tuple in self.docstring_tuples:
            # If there is a new docstring and an original docstring, replace the corresponding token string
            if docstring_tuple.new_docstring and docstring_tuple.original_docstring:
                original_token_type = token_list[docstring_tuple.token_idx][0]
                token_list[docstring_tuple.token_idx] = (original_token_type, docstring_tuple.new_docstring)
                update_needed = True

            # If there is a new docstring, but no original docstring, a new token has to be inserted at the right token_idx
            if docstring_tuple.new_docstring and not docstring_tuple.original_docstring:
                token = (tokenize.STRING, docstring_tuple.new_docstring)
                # Assuming we will be inserting in the right order here
                # Account for previous tokens added in the original list
                token_list.insert(int(docstring_tuple.token_idx) + tokens_inserted, token)
                tokens_inserted += 1
                token = (tokenize.NEWLINE, '')
                token_list.insert(int(docstring_tuple.token_idx) + tokens_inserted, token)
                tokens_inserted += 1
                update_needed = True
                
        if not update_needed:
            tinctest.logger.debug("No update required for file %s" %self.filename)
            if file_to_write == self.filename:
                return
            else:
                try:
                    shutil.copyfile(self.filename, file_to_write)
                except Exception, e:
                    tinctest.logger.exception(e)
                    raise TINCMMException("Encountered error while copying file %s to %s" % (self.filename, file_to_write))
        
        try:
            with open(file_to_write, 'w') as file_object:
                code_string = tokenize.untokenize(token_list)
                file_object.write(code_string)
        except Exception, e:
            tinctest.logger.exception(e)
            raise TINCMMException("Encountered error while modifying file: %s" %file_to_write)
    
    def update_docstring(self, class_name, method_name, metadata_dictionary):
        """
        Generate new_docstring with the given metadata_dictionary, and update its information in the docstring_tuples list.
        Class/Method arguments work in pair. If method is None, the docstring belongs to Class.
        
        @type class_name: string
        @param class_name: Name of the class.

        @type method_name: string
        @param method_name: Name of the method.

        @type metadata_dictionary: dict
        @param metadata_dictionary: Dictionary that contains the updated metadata information.
        """
        if not metadata_dictionary['__changed__']:
            return
        
        original_tuple = None
        
        # Get the original docstring
        for my_tuple in self.docstring_tuples:
            if my_tuple.class_name == class_name and my_tuple.method_name == method_name:
                original_tuple = my_tuple
                break
        
        new_docstring = self.generate_new_docstring(original_tuple.original_docstring, metadata_dictionary, original_tuple.offset + 4)
        
        # Update the tuple
        new_tuple = original_tuple._replace(new_docstring=new_docstring)
        
        new_tuples_list = []
        # Replace the new tuple in self.docstring_tuples.
        for my_tuple in self.docstring_tuples:
            if my_tuple.class_name == class_name and my_tuple.method_name == method_name:
                new_tuples_list.append(new_tuple)
            else:
                new_tuples_list.append(my_tuple)
        self.docstring_tuples = new_tuples_list
    
    def generate_new_docstring(self, original_docstring, metadata_dictionary, indentation):
        """
        Given a docstring, metadata dictionary, and indentation, this method generates and returns a new docstring.
        The comments with the original_docstring are left as is in the new_docstring.
        If the original docstring is None, a new_docstring is created from scratch. Indentation number is used to
        determine the spaces/offset for new_docstring that is generated from scratch.
        
        @type original_docstring: string
        @param original_docstring: Original docstring.

        @type metadata_dictionary: dict
        @param metadata_dictionary: Dictionary that has the updated information. 

        @type indentation: int
        @param indentation: Indentation gives the number of spaces that the docstring is indented.

        @rtype: string
        @return: New docstring
        """
        
        if original_docstring is None:
            # Scenario where there wasn't a docstring before, but we'll have a docstring now
            # Find how many spaces we will need
            original_docstring = "'''\n"
            for count_space in range(indentation):
                original_docstring += " "
            original_docstring += "'''\n"
                
        new_docstring = ""
        new_docstring_lines = []
        original_docstring_lines = original_docstring.splitlines()
        # Keep track number of docstring found and all the keys in dictionary.
        # Before adding the last one, add all the new keys
        metadata_keys = metadata_dictionary.keys()
        docstring_quotes_count = 0
        for line in original_docstring_lines:
            if _ParserHelp.find_docstring_quotes(line):
                docstring_quotes_count += 1
                if docstring_quotes_count == 2:
                    # Before finishing docstring, add all the leftover keys
                    new_line = ""
                    for count in range(_ParserHelp.find_spaces(line)):
                        new_line += " "
                    for key in metadata_keys:
                        # Skip __changed__
                        if key == "__changed__":
                            continue
                        new_docstring_lines.append(new_line + "@" + str(key) + " " + str(metadata_dictionary[key]))
            
            stripped_line = line.strip()
            if stripped_line.find('@') != 0:
                new_docstring_lines.append(line)
                continue
            stripped_line = stripped_line[1:]
            if len(stripped_line.split()) <= 1:
                new_docstring_lines.append(line)
                continue
            (key, value) = stripped_line.split(' ', 1)
            if key in metadata_dictionary:
                line = line.replace(value, metadata_dictionary[key])
                new_docstring_lines.append(line)
                metadata_keys.remove(key)
            else:
                # Key is deleted. Don't add this line
                pass
        new_docstring = "\n".join(new_docstring_lines)
        # Add new line at the end.
        new_docstring += "\n"
        return new_docstring
        
    def get_metadata_dictionary(self, method_name, class_name):
        """
        Given a method_name and class_name, this method returns the metadata information in the original_docstring
        in a dictionary format. If the original docstring is None, an empty defaultdict is returned.
        
        @type method_name: string
        @param method_name: Name of the method.
        
        @type class_name: string
        @param class_name: Name of the class.
        
        @rtype: defaultdict
        @return: defaultdict that has all the metadata information. Key is the metadata name, and value is the value of that metadata.
        """        
        # Metadata to return...
        metadata = defaultdict(str)
        # This key is to determine if this dictionary gets changed as a result of applying queries
        metadata['__changed__'] = False
        
        # Get the docstring from the self.docstring_tuples tuple and form a dictionary
        my_docstring_tuple = None
        for my_tuple in self.docstring_tuples:
            if my_tuple.class_name == class_name and my_tuple.method_name == method_name:
                my_docstring_tuple = my_tuple
                break
        
        if my_docstring_tuple is None:
            return metadata
        
        docstring = my_docstring_tuple.original_docstring
        if not docstring:
            return metadata
        lines = docstring.splitlines()
        for line in lines:
            line = line.strip()
            if line.find('@') != 0:
                continue
            line = line[1:]
            if len(line.split()) <= 1:
                continue
            (key, value) = line.split(' ', 1)
            metadata[key] = value
        return metadata

class SQLFileHandler(object):
    """
    This class handles parsing and update a SQL file. It provides similar services to PythonFileHandler for SQL files.
    """
    def __init__(self, filename):
        """
        This constructor requires a sql filename to parse and store all its information. The information is stored in original_docstring
        and new_docstring.
        
        @type filename: string
        @param filename: Absolute path to the sql file that needs docstring handling.
        """
        self.filename = filename
        
        # Docstring in SQL File
        self.original_docstring = ""
        self.new_docstring = ""
        self.parse_file()
    
    def parse_file(self):
        """
        This method will parse through the file and find the docstring.
        """
        try:
            file_object = open(self.filename, 'r')
            file_object.close()
        except:
            raise TINCMMException("Cannot open file %s" % self.filename)
        
        with open(self.filename, 'r') as file_object:
            for line in file_object:
                stripped_line = line.strip()
                if stripped_line.find('--') != 0:
                    break
                self.original_docstring += line
    
    def update_file(self, file_to_write = None):
        """
        This method will update the original sql file with the new docstring.
        If file_to_write is specified, the original sql file will be left as is, and file_to_write will be used
        with new docstring.
        
        @type file_to_write: string
        @param file_to_write: Absolute path to the file to use to write back the original file with new docstring.
                              Defaults to None. If None, the original file will be updated.
        """
        
        if not file_to_write:
            file_to_write = self.filename
        if self.original_docstring == self.new_docstring:
            if file_to_write == self.filename:
                return
            else:
                try:
                    shutil.copyfile(self.filename, file_to_write)
                except Exception, e:
                    tinctest.logger.exception(e)
                    raise TINCMMException("Encountered error while copying file %s to %s" % (self.filename, file_to_write))
        
        try:
            file_object = open(self.filename, 'r')
            file_object.close()
        except:
            raise TINCMMException("Cannot open file %s" % self.filename)
        
        code_string = self.new_docstring
        
        rest_of_code = ""
        with open(self.filename, 'r') as file_object:
            for line in file_object:
                stripped_line = line.strip()
                if stripped_line.find('--') == 0:
                    continue
                rest_of_code += line
        code_string += rest_of_code
        
        try:
            with open(file_to_write, 'w') as file_object:
                file_object.write(code_string)
        except Exception, e:
            tinctest.logger.exception(e)
            raise TINCMMException("Encountered error while modifying file: %s" %file_to_write)
    
    def update_docstring(self, metadata_dictionary):
        """
        Generate and store new_docstring with the given metadata_dictionary.
        
        @type metadata_dictionary: dict
        @param metadata_dictionary: Dictionary that contains the updated metadata information.
        """
        if not metadata_dictionary['__changed__']:
            self.new_docstring = self.original_docstring
            return
        
        new_docstring_lines = []
        original_docstring_lines = self.original_docstring.splitlines()
        # Keep track number of docstring found and all the keys in dictionary.
        # Before adding the last one, add all the new keys
        metadata_keys = metadata_dictionary.keys()
        for line in original_docstring_lines:
            stripped_line = line.strip()
            # Remove the dashes
            stripped_line = stripped_line[2:].strip()
            if stripped_line.find('@') != 0:
                new_docstring_lines.append(line)
                continue
            # Remove the @ sign
            stripped_line = stripped_line[1:].strip()
            if len(stripped_line.split()) <= 1:
                new_docstring_lines.append(line)
                continue
            (key, value) = stripped_line.split(' ', 1)
            if key in metadata_dictionary:
                line = line.replace(value, metadata_dictionary[key])
                new_docstring_lines.append(line)
                metadata_keys.remove(key)
            else:
                # Key is deleted. Don't add this line
                pass
        # Add leftover keys
        for key in metadata_keys:
            # Skip __changed__
            if key == "__changed__":
                continue
            new_docstring_lines.append("-- " + "@" + str(key) + " " + str(metadata_dictionary[key]))
                
        self.new_docstring = "\n".join(new_docstring_lines)
        # Add new line at the end.
        self.new_docstring += "\n"
        
    def get_metadata_dictionary(self):
        """
        This method returns the metadata information in the original_docstring
        in a dictionary format. If the original docstring is None, an empty defaultdict is returned.
        
        @rtype: defaultdict
        @return: defaultdict that has all the metadata information. Key is the metadata name, and value is the value of that metadata.
        """        
        # Metadata to return...
        metadata = defaultdict(str)
        # This key is to determine if this dictionary gets changed as a result of applying queries
        metadata['__changed__'] = False
        
        if self.original_docstring == "":
            return metadata
        lines = self.original_docstring.splitlines()
        for line in lines:
            line = line.strip()
            # Remove the dashes
            line = line[2:].strip()
            if line.find('@') != 0:
                continue
            # Remove the @ sign
            line = line[1:].strip()
            if len(line.split()) <= 1:
                continue
            (key, value) = line.split(' ', 1)
            metadata[key] = value
        return metadata

class TINCTestCaseMM(object):
    """
    This class stores and manipulates class_metadata and method_metadata dictionaries.
    """

    # List of classes that have already been updated due to insert/update/delete operation
    # This list should be emptied by the caller, after each modification.
    updated_classes = []
    def __init__(self, tinc_test_case, class_metadata, method_metadata):
        """
        This constructor requires the metadata dictionary of class and method.
        It also requires the TINC test case object. It uses this object to determine the name of the test case,
        as well as to determine whether all the tests at the class level have been loaded.

        @type tinc_test_case: TINCTestCase object
        @param tinc_test_case: Test case that will be used to determine test name and whether all tests in class are loaded.

        @type class_metadata: dict
        @param class_metadata: Metadata dictionary of the class.

        @type method_metadata: dict
        @param method_metadata: Metadata dictionary of the test method.
        """
        self.tinc_test_case = tinc_test_case
        self.class_metadata = class_metadata
        self.method_metadata = method_metadata
        
        # Find the name of the test
        self.name = self.tinc_test_case.full_name
        
        # Class name of this tinc test case object
        self.full_class_name = self.tinc_test_case.full_name.rsplit('.', 1)[0]
        
        # Determine whether the class metadata needs to be changed.
        # If all tests are loaded, only class metadata needs to be changed.
        self.change_class = self.tinc_test_case.__class__._all_tests_loaded 
        
        # This variable stores error_message of the last operation.
        self.error_message = None
    
    def select_metadata(self, metadata_key):
        """
        This method will print the value of the given metadata key.
        
        @type metadata_key: string
        @param metadata_key: Metadata's whose value is printed. 
        """
        if not metadata_key in self.class_metadata:
            class_value = "None"
        else:
            class_value = self.class_metadata[metadata_key]
        if not metadata_key in self.method_metadata:
            method_value = "None"
        else:
            method_value = self.method_metadata[metadata_key]
        
        print "%s|%s|%s|%s" % (self.name, metadata_key, class_value, method_value)
    
    def _print_error_message(self, metadata_key):
        """
        This method will print in stdout that no operation was performed.
        
        @type metadata_key: string
        @param metadata_key: Metadata which had no-op.
        """
        print "%s|%s|%s" % (self.name, metadata_key, self.error_message)
        tinctest.logger.error(self.error_message)
    
    def check_if_class_needs_update(self):
        """
        This method will return True if the class has already been updated and needs no more change.
        It returns False otherwise.

        @rtype: boolean
        @return: True if class metadata has already been processed. False otherwise.
        """
        if (self.full_class_name not in self.updated_classes):
            return True
        else:
            return False
    
    def update_metadata(self, metadata_key, metadata_value):
        """
        This method will replace the value of the metadata. Once updated, it will print the updated key/value pair.
    
        @type metadata_key: string
        @param metadata_key: Metadata's whose value will be replaced.
        
        @type metadata_value: string
        @param metadata_value: Metadata's new value.
        """
        updated = False
        
        # Also update it in the method metadata dict if metadata exists
        if metadata_key in self.method_metadata:
            self.method_metadata[metadata_key] = metadata_value
            self.method_metadata['__changed__'] = True
            updated = True
        
        # Check if metadata exists in the class metadata dict
        if self.change_class and (metadata_key in self.class_metadata) and self.check_if_class_needs_update():
            self.class_metadata[metadata_key] = metadata_value
            self.class_metadata['__changed__'] = True
            self.updated_classes.append(self.full_class_name)
            updated = True
        
        if not self.check_if_class_needs_update():
            updated = True
        
        if not updated:
            self.error_message = "Cannot update metadata %s for test %s. Metadata doesn't exist!" % (metadata_key, self.name)
            self._print_error_message(metadata_key)
        else:
            self.select_metadata(metadata_key)
    
    def insert_metadata(self, metadata_key, metadata_value):
        """
        This method will insert a value, if it doesn't exist. If value already exists, it will raise an error.
        Once the operation is complete, it will print the updated key/value pair.
        
        @type metadata_key: string
        @param metadata_key: Metadata's whose value will be replaced.
        
        @type metadata_value: string
        @param metadata_value: Metadata's new value.
        """
        changed = True
        if metadata_key in self.method_metadata:
            if metadata_key == 'tags':
                metadata_value, changed = self._insert_tags(self.method_metadata['tags'], metadata_value)
            else:
                self.error_message = "Cannot insert method metadata %s for test %s. Metadata already exists with value %s!" % (metadata_key, self.name, self.method_metadata[metadata_key])
                self._print_error_message(metadata_key)
                return
        if not self.change_class and changed:
            # Update method dict
            self.method_metadata[metadata_key] = metadata_value
            self.method_metadata['__changed__'] = True
        
        if self.change_class and self.check_if_class_needs_update():
            if metadata_key in self.class_metadata:
                if metadata_key == 'tags':
                    metadata_value, changed = self._insert_tags(self.class_metadata['tags'], metadata_value)
                else:
                    self.error_message = "Cannot insert class metadata %s for test %s. Metadata already exists with value %s!" % (metadata_key, self.name, self.class_metadata[metadata_key])
                    self._print_error_message(metadata_key)
                    return
            if changed:
                # Update class dict
                self.class_metadata[metadata_key] = metadata_value
                self.class_metadata['__changed__'] = True
                self.updated_classes.append(self.full_class_name)

        self.select_metadata(metadata_key)

    def _insert_tags(self, value1, value2):
        """
        Adds tags in value2 to tags in value1
        """
        tags1 = set(value1.split())
        tags2 = set(value2.split())
        result = tags1 | tags2
        if result == tags1:
            return (" ".join(result), False)
        return (" ".join(result), True)
            
    def delete_metadata(self, metadata_key, metadata_value=None):
        """
        This method will delete a key, if it exists. If key doesn't exist, it will raise an error.
        
        @type metadata_key: string
        @param metadata_key: Metadata's whose value will be replaced.
        
        """
        if metadata_key in self.method_metadata:
            if metadata_key == 'tags':
                metadata_value = self._delete_tags(self.method_metadata['tags'], metadata_value)
            if not metadata_value:
                self.method_metadata.pop(metadata_key)
            else:
                self.method_metadata[metadata_key] = metadata_value
            self.method_metadata['__changed__'] = True
            print "%s|%s|None|DELETED" % (self.name, metadata_key)

        elif not self.change_class:
            self.error_message = "Cannot remove method metadata %s for test %s. Metadata doesn't exist!" % (metadata_key, self.name)
            self._print_error_message(metadata_key)
            return
        
        if self.change_class and self.check_if_class_needs_update():
            if metadata_key in self.class_metadata:
                changed = True
                if metadata_key == 'tags':
                    metadata_value, changed = self._delete_tags(self.class_metadata['tags'], metadata_value)
                
                if changed:
                    if not metadata_value:
                        self.class_metadata.pop(metadata_key)
                    else:
                        self.class_metadata[metadata_key] = metadata_value
                    self.class_metadata['__changed__'] = True
                    print "%s|%s|DELETED|None" % (self.name, metadata_key)
                    self.updated_classes.append(self.full_class_name)
            else:
                self.error_message = "Cannot remove class metadata %s for test %s. Metadata doesn't exist!" % (metadata_key, self.name)
                self._print_error_message(metadata_key)
                return

    def _delete_tags(self, value1, value2):
        """
        Removes product_version represented by string value2 from product_version represented by string value1
        """
        tags1 = set(value1.split())
        tags2 = set(value2.split())
        result = tags1 - tags2
        if result == tags1:
            return (" ".join(result), False)
        return (" ".join(result), True)
        
        

